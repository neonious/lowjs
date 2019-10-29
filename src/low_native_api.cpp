// -----------------------------------------------------------------------------
//  low_native_api.cpp
// -----------------------------------------------------------------------------

#include "low_native_api.h"

#include "low_alloc.h"
#include "low_main.h"
#include "low_module.h"
#include "low_system.h"
#include "low_loop.h"

#include <duktape.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unwind.h>
#include <dlfcn.h>

#if defined(__x86_64__)
#include <sys/elf64.h>

#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Phdr    Elf64_Phdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Rela    Elf64_Rela
#define Elf_Rel     Elf64_Rel
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE
#elif defined(__i386__)
#include <sys/elf32.h>

#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Phdr    Elf32_Phdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
#define Elf_Rela    Elf32_Rela
#define Elf_Rel     Elf32_Rel
#define ELF_R_SYM   ELF32_R_SYM
#define ELF_R_TYPE  ELF32_R_TYPE
#endif


// Constants
extern low_system_t g_low_system;

struct native_api_entry_t
{
    const char *name;
    uintptr_t func;
};


// -----------------------------------------------------------------------------
//  native_api_load
// -----------------------------------------------------------------------------

void *native_api_load(const char *data, unsigned int size, const char **err, bool *err_malloc)
{
#if defined(__x86_64__) || defined(__i386__)
    const Elf_Ehdr *hdr;
    const Elf_Phdr *phdr;
    const Elf_Shdr *shdr;

    bool exec_has = false;
    unsigned int exec_min, exec_max, exec_size;
    char *exec = NULL;
    const char *strings;
    const Elf_Sym *syms;
    void *entry = NULL;

    // Validate image
    hdr = (const Elf_Ehdr *)data;
    if(size < sizeof(Elf_Ehdr))
        goto range_error;

    if(hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E'
    || hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F')
    {
        *err = "File is not an ELF file.";
        return NULL;
    }

#if defined(__x86_64__)
    if(hdr->e_ident[EI_CLASS] != ELFCLASS64
    || hdr->e_machine != EM_X86_64)
    {
        *err = "File is an ELF file, but not one for x86_64 machines.";
        return NULL;
    }
#elif defined(__i386__)
    if(hdr->e_ident[EI_CLASS] != ELFCLASS32
    || hdr->e_machine != EM_386)
    {
        *err = "File is an ELF file, but not one for x86 machines.";
        return NULL;
    }
#endif

    if(!hdr->e_phnum || size < hdr->e_phoff + hdr->e_phnum * sizeof(Elf_Phdr)
    || !hdr->e_shnum || size < hdr->e_shoff + hdr->e_shnum * sizeof(Elf_Shdr))
        goto range_error;
    phdr = (const Elf_Phdr *)(data + hdr->e_phoff);
    shdr = (const Elf_Shdr *)(data + hdr->e_shoff);

    // Get image size
    for (int i = 0; i < hdr->e_phnum; i++)
        if (phdr[i].p_type == PT_LOAD && phdr[i].p_filesz) {
            if(phdr[i].p_filesz > phdr[i].p_memsz)
                goto range_error;
            if(size < phdr[i].p_offset + phdr[i].p_filesz)
                goto range_error;

            uintptr_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if(!exec_has || exec_min > phdr[i].p_vaddr)
                exec_min = phdr[i].p_vaddr;
            if(!exec_has || exec_max < end)
                exec_max = end;
            exec_has = true;
        }
    if(!exec_has)
    {
        *err = "File is not an ELF file.";
        return NULL;
    }
    exec_size = exec_max - exec_min;

    // Copy image into memory
    exec = (char *)mmap(NULL, exec_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if(!exec)
    {
        *err = "Memory is full.";
        return NULL;
    }
    exec -= exec_min;

    memset(exec, 0, exec_size);
    for(int i = 0; i < hdr->e_phnum; i++)
        if (phdr[i].p_type == PT_LOAD && phdr[i].p_filesz) {
            const char *start = data + phdr[i].p_offset;
            char *taddr = phdr[i].p_vaddr + exec;
            memmove(taddr, start, phdr[i].p_filesz);
        }

    // Get entry point
    for(int i=0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNSYM) {
            if(shdr[i].sh_link >= hdr->e_shnum)
                goto range_error;

            // We could check further for range errors than here
            // but the user can break many things if they are allowed to load native
            // modules, so it is not a security critical thing if we do not

            strings = data + shdr[shdr[i].sh_link].sh_offset;
            syms = (const Elf_Sym *)(data + shdr[i].sh_offset);

            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Sym); j++)
                if (strcmp("module_main", strings + syms[j].st_name) == 0)
                {
                    entry = exec + syms[j].st_value;
                    break;
                }
            if(entry)
                break;
        }
    }
    if(!entry)
    {
        munmap(exec, exec_size);
        *err = "Entry point module_main not found.";
        return NULL;
    }

    // Modify relocatables
    for(int i=0; i < hdr->e_shnum; i++)
    {
        // https://www.intezer.com/executable-and-linkable-format-101-part-3-relocations/
#if defined(__x86_64__)
        if (shdr[i].sh_type == SHT_RELA)
        {
            const Elf_Rela *rel = (const Elf_Rela *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rela); j++)
                switch(ELF_R_TYPE(rel[j].r_info))
                {
                case R_X86_64_NONE: break;

                case R_X86_64_PC64:
                case R_X86_64_64:
                case R_X86_64_JMP_SLOT:
                case R_X86_64_GLOB_DAT:
                    uintptr_t func;
                    const char *sym;
                    int k, len;

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;
/*
                    // support all operator new and deletes
                    if(memcmp(sym, "_Znwm", 5) == 0
                    || memcmp(sym, "_Znam", 5) == 0)
                        len = 5;
                    else if(memcmp(sym, "_ZdlPv", 6) == 0
                         || memcmp(sym, "_ZdaPv", 6) == 0)
                        len = 6;
                    else
                        len = strlen(sym) + 1;

                    for(k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(memcmp(NATIVE_API_ENTRIES[k].name, sym, len) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }
*/
                    func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(exec, exec_size);
                        *err = (char *)low_alloc(80 + strlen(sym));
                        sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
                        *err_malloc = true;
                        return NULL;
                    }

                    if(ELF_R_TYPE(rel[j].r_info) == R_X86_64_PC64)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend - (uintptr_t)(exec + rel[j].r_offset);
                    else if(ELF_R_TYPE(rel[j].r_info) == R_X86_64_64)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend;
                    else
                        *(uintptr_t *)(exec + rel[j].r_offset) = func;
                    break;

                case R_X86_64_RELATIVE:
                    *(uintptr_t *)(exec + rel[j].r_offset) += (uintptr_t)exec;
                    break;

                default:
                    munmap(exec, exec_size);
                    *err = (char *)low_alloc(80);
                    sprintf((char *)*err, "Unknown relocatable type #%d.", (int)ELF_R_TYPE(rel[j].r_info));
                    *err_malloc = true;
                    return NULL;
                }
        }
#elif defined(__i386__)
        if (shdr[i].sh_type == SHT_REL)
        {
            const Elf_Rel *rel = (const Elf_Rel *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rel); j++)
                switch(ELF_R_TYPE(rel[j].r_info))
                {
                case R_386_NONE: break;

                case R_386_PC32:
                case R_386_32:
                case R_386_JMP_SLOT:
                case R_386_GLOB_DAT:
                    uintptr_t func;
                    const char *sym;
                    int k, len;

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;
/*
                    // support all operator new and deletes
                    if(memcmp(sym, "_Znwm", 5) == 0
                    || memcmp(sym, "_Znam", 5) == 0)
                        len = 5;
                    else if(memcmp(sym, "_ZdlPv", 6) == 0
                         || memcmp(sym, "_ZdaPv", 6) == 0)
                        len = 6;
                    else
                        len = strlen(sym) + 1;

                    for(k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(memcmp(NATIVE_API_ENTRIES[k].name, sym, len) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }
*/
                    func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(exec, exec_size);
                        *err = (char *)low_alloc(80 + strlen(sym));
                        sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
                        *err_malloc = true;
                        return NULL;
                    }

                    if(ELF_R_TYPE(rel[j].r_info) == R_386_PC32)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend - (uintptr_t)(exec + rel[j].r_offset);
                    else if(ELF_R_TYPE(rel[j].r_info) == R_386_32)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend;
                    else
                        *(uintptr_t *)(exec + rel[j].r_offset) = func;
                    break;

                case R_386_RELATIVE:
                    *(uintptr_t *)(exec + rel[j].r_offset) += (uintptr_t)exec;
                    break;

                default:
                    munmap(exec, exec_size);
                    *err = (char *)low_alloc(80);
                    sprintf((char *)*err, "Unknown relocatable type #%d.", (int)ELF_R_TYPE(rel[j].r_info));
                    *err_malloc = true;
                    return NULL;
                }
        }
#endif
    }

    // Modify heap protection flags
    for(int i = 0; i < hdr->e_phnum; i++) {
        if(phdr[i].p_type != PT_LOAD || !phdr[i].p_filesz)
            continue;

        char *taddr = phdr[i].p_vaddr + exec;
        if(!(phdr[i].p_flags & PF_W))
            // Read-only.
            mprotect((unsigned char *)taddr, phdr[i].p_memsz, PROT_READ);
        if(phdr[i].p_flags & PF_X)
            // Executable.
            mprotect((unsigned char *)taddr, phdr[i].p_memsz, PROT_EXEC);
    }

    // Setup stack unwinding for throws
    for(int i = 0; i < hdr->e_shnum; i++)
    {
        const Elf_Shdr *sh_strtab = &shdr[hdr->e_shstrndx];
        const char *const sh_strtab_p = data + sh_strtab->sh_offset;

        if(strcmp(sh_strtab_p + shdr[i].sh_name, ".eh_frame") == 0)
        {
#ifdef __APPLE__
            // On OS X/BSD __register_frame takes a single FDE as an argument.
            // See http://lists.llvm.org/pipermail/llvm-dev/2013-April/061737.html
            // and projects/libunwind/src/UnwindLevel1-gcc-ext.c
            const char *P = (const char *)exec + shdr[i].sh_addr;
            const char *End = P + shdr[i].sh_size;
            do  {
                uint32_t Length = *(const uint32_t *)P;
                uint32_t Offset = *(const uint32_t *)(P + 4);
                if(Offset != 0)
                    __register_frame(P);
                P += 4 + Length;
                if(P > End)
                {
                    munmap(exec, exec_size);
                    *err = ".eh_frame section broken.";
                    return NULL;
                }
            } while(P != End);
#else
            // TODO
#endif /* __APPLE__ */

            break;
        }
    }

    // Call constructors, send destructors to atexit
    for(int i = 0; i < hdr->e_shnum; i++)
        if (shdr[i].sh_type == SHT_PREINIT_ARRAY) {
            uintptr_t *calls = (uintptr_t *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(uintptr_t); j++) {
                if(*calls && *calls != (uintptr_t)-1)
                    ((void (*)(void))(*calls + exec))();
                calls++;
            }
        }

    for(int i = 0; i < hdr->e_shnum; i++)
        if(shdr[i].sh_type == SHT_INIT_ARRAY)
        {
            uintptr_t *calls = (uintptr_t *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(uintptr_t); j++)
            {
                if(*calls && *calls != (uintptr_t)-1)
                    ((void (*)(void))(*calls + exec))();
                calls++;
            }
        }
        else if(shdr[i].sh_type == SHT_FINI_ARRAY)
        {
            uintptr_t *calls = (uintptr_t *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(uintptr_t); j++)
            {
                if(*calls && *calls != (uintptr_t)-1)
                    atexit((void (*)(void))(*calls + exec));
                calls++;
            }
        }

    return entry;

range_error:
    if(exec)
        munmap(exec, exec_size);

    *err = "File is a valid ELF file for this machine, but corrupt.";
    return NULL;
#else
    *err = "Native modules are not yet supported on this architecture.";
    return NULL;
#endif
}


// -----------------------------------------------------------------------------
//  native_api_call
// -----------------------------------------------------------------------------

int native_api_call(duk_context *ctx)
{
    void **params = (void **)duk_get_buffer_data(ctx, 2, NULL);
    int (*module_main)(duk_context *, const char *) = (int (*)(duk_context *, const char *))params[0];
    const char *path = (const char *)params[1];
    duk_pop(ctx);

    return module_main(ctx, path);
}