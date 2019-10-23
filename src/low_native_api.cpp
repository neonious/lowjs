// -----------------------------------------------------------------------------
//  low_native_api.cpp
// -----------------------------------------------------------------------------

#include "low_native_api.h"

#include "low_module.h"
#include "low_system.h"

#include <duktape.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#if defined(__x86_64__)
#include <sys/elf64.h>

#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Phdr    Elf64_Phdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Rela    Elf64_Rela
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE
#elif defined(__i386__)
#include <sys/elf32.h>

#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Phdr    Elf32_Phdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
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

struct native_api_entry_t NATIVE_API_ENTRIES[] = {
    {"malloc", (uintptr_t)malloc},
    {"calloc", (uintptr_t)calloc},
    {"realloc", (uintptr_t)realloc},
    {"free", (uintptr_t)free},

    {"memcmp", (uintptr_t)memcmp},
    {"memcpy", (uintptr_t)memcpy},
    {"memmove", (uintptr_t)memmove},
    {"memset", (uintptr_t)memset},

    {"strcpy", (uintptr_t)strcpy},
    {"sprintf", (uintptr_t)sprintf},

    {"open", (uintptr_t)open},
    {"creat", (uintptr_t)creat},
    {"close", (uintptr_t)close},
    {"read", (uintptr_t)read},
    {"write", (uintptr_t)write},
    {"lseek", (uintptr_t)lseek},
    {"stat", (uintptr_t)stat},
    {"fstat", (uintptr_t)fstat},
    {"rename", (uintptr_t)rename},
    {"truncate", (uintptr_t)truncate},
    {"unlink", (uintptr_t)unlink},
    {"mkdir", (uintptr_t)mkdir},
    {"rmdir", (uintptr_t)rmdir},
    {"opendir", (uintptr_t)opendir},
    {"readdir", (uintptr_t)readdir},
    {"closedir", (uintptr_t)closedir},

//    {"low_call_direct", (uintptr_t)low_call_direct},
    {"low_add_stash", (uintptr_t)low_add_stash},
    {"low_remove_stash", (uintptr_t)low_remove_stash},
    {"low_push_stash", (uintptr_t)low_push_stash},
    {"low_push_buffer", (uintptr_t)low_push_buffer},
//    {"low_call_next_tick", (uintptr_t)low_call_next_tick},

    {"duk_base64_decode", (uintptr_t)duk_base64_decode},
    {"duk_base64_encode", (uintptr_t)duk_base64_encode},
    {"duk_buffer_to_string", (uintptr_t)duk_buffer_to_string},
    {"duk_call", (uintptr_t)duk_call},
    {"duk_call_method", (uintptr_t)duk_call_method},
    {"duk_call_prop", (uintptr_t)duk_call_prop},
    {"duk_char_code_at", (uintptr_t)duk_char_code_at},
    {"duk_check_type", (uintptr_t)duk_check_type},
    {"duk_check_type_mask", (uintptr_t)duk_check_type_mask},
    {"duk_components_to_time", (uintptr_t)duk_components_to_time},
    {"duk_concat", (uintptr_t)duk_concat},
    {"duk_copy", (uintptr_t)duk_copy},
    {"duk_decode_string", (uintptr_t)duk_decode_string},
    {"duk_def_prop", (uintptr_t)duk_def_prop},
    {"duk_del_prop", (uintptr_t)duk_del_prop},
    {"duk_del_prop_index", (uintptr_t)duk_del_prop_index},
    {"duk_del_prop_literal_raw", (uintptr_t)duk_del_prop_literal_raw},
    {"duk_del_prop_lstring", (uintptr_t)duk_del_prop_lstring},
    {"duk_del_prop_string", (uintptr_t)duk_del_prop_string},
    {"duk_dup", (uintptr_t)duk_dup},
    {"duk_dup_top", (uintptr_t)duk_dup_top},
    {"duk_enum", (uintptr_t)duk_enum},
    {"duk_equals", (uintptr_t)duk_equals},
    {"duk_error_raw", (uintptr_t)duk_error_raw},
    {"duk_freeze", (uintptr_t)duk_freeze},
    {"duk_get_boolean", (uintptr_t)duk_get_boolean},
    {"duk_get_boolean_default", (uintptr_t)duk_get_boolean_default},
    {"duk_get_buffer_data", (uintptr_t)duk_get_buffer_data},
    {"duk_get_buffer_data_default", (uintptr_t)duk_get_buffer_data_default},
    {"duk_get_c_function", (uintptr_t)duk_get_c_function},
    {"duk_get_c_function_default", (uintptr_t)duk_get_c_function_default},
    {"duk_get_finalizer", (uintptr_t)duk_get_finalizer},
    {"duk_get_int", (uintptr_t)duk_get_int},
    {"duk_get_int_default", (uintptr_t)duk_get_int_default},
    {"duk_get_length", (uintptr_t)duk_get_length},
    {"duk_get_lstring", (uintptr_t)duk_get_lstring},
    {"duk_get_lstring_default", (uintptr_t)duk_get_lstring_default},
    {"duk_get_now", (uintptr_t)duk_get_now},
    {"duk_get_number", (uintptr_t)duk_get_number},
    {"duk_get_number_default", (uintptr_t)duk_get_number_default},
    {"duk_get_pointer", (uintptr_t)duk_get_pointer},
    {"duk_get_pointer_default", (uintptr_t)duk_get_pointer_default},
    {"duk_get_prop", (uintptr_t)duk_get_prop},
    {"duk_get_prop_desc", (uintptr_t)duk_get_prop_desc},
    {"duk_get_prop_index", (uintptr_t)duk_get_prop_index},
    {"duk_get_prop_literal_raw", (uintptr_t)duk_get_prop_literal_raw},
    {"duk_get_prop_lstring", (uintptr_t)duk_get_prop_lstring},
    {"duk_get_prop_string", (uintptr_t)duk_get_prop_string},
    {"duk_get_prototype", (uintptr_t)duk_get_prototype},
    {"duk_get_string", (uintptr_t)duk_get_string},
    {"duk_get_string_default", (uintptr_t)duk_get_string_default},
    {"duk_get_top", (uintptr_t)duk_get_top},
    {"duk_get_top_index", (uintptr_t)duk_get_top_index},
    {"duk_get_type", (uintptr_t)duk_get_type},
    {"duk_get_type_mask", (uintptr_t)duk_get_type_mask},
    {"duk_get_uint", (uintptr_t)duk_get_uint},
    {"duk_get_uint_default", (uintptr_t)duk_get_uint_default},
    {"duk_has_prop", (uintptr_t)duk_has_prop},
    {"duk_has_prop_index", (uintptr_t)duk_has_prop_index},
    {"duk_has_prop_literal_raw", (uintptr_t)duk_has_prop_literal_raw},
    {"duk_has_prop_lstring", (uintptr_t)duk_has_prop_lstring},
    {"duk_has_prop_string", (uintptr_t)duk_has_prop_string},
    {"duk_hex_decode", (uintptr_t)duk_hex_decode},
    {"duk_hex_encode", (uintptr_t)duk_hex_encode},
    {"duk_insert", (uintptr_t)duk_insert},
    {"duk_instanceof", (uintptr_t)duk_instanceof},
    {"duk_is_array", (uintptr_t)duk_is_array},
    {"duk_is_boolean", (uintptr_t)duk_is_boolean},
    {"duk_is_buffer_data", (uintptr_t)duk_is_buffer_data},
    {"duk_is_c_function", (uintptr_t)duk_is_c_function},
    {"duk_is_constructable", (uintptr_t)duk_is_constructable},
    {"duk_is_constructor_call", (uintptr_t)duk_is_constructor_call},
    {"duk_is_function", (uintptr_t)duk_is_function},
    {"duk_is_nan", (uintptr_t)duk_is_nan},
    {"duk_is_null", (uintptr_t)duk_is_null},
    {"duk_is_number", (uintptr_t)duk_is_number},
    {"duk_is_object", (uintptr_t)duk_is_object},
    {"duk_is_pointer", (uintptr_t)duk_is_pointer},
    {"duk_is_string", (uintptr_t)duk_is_string},
    {"duk_is_symbol", (uintptr_t)duk_is_symbol},
    {"duk_is_undefined", (uintptr_t)duk_is_undefined},
    {"duk_is_valid_index", (uintptr_t)duk_is_valid_index},
    {"duk_join", (uintptr_t)duk_join},
    {"duk_json_decode", (uintptr_t)duk_json_decode},
    {"duk_json_encode", (uintptr_t)duk_json_encode},
    {"duk_map_string", (uintptr_t)duk_map_string},
    {"duk_new", (uintptr_t)duk_new},
    {"duk_next", (uintptr_t)duk_next},
    {"duk_normalize_index", (uintptr_t)duk_normalize_index},
    {"duk_pop", (uintptr_t)duk_pop},
    {"duk_pop_2", (uintptr_t)duk_pop_2},
    {"duk_pop_3", (uintptr_t)duk_pop_3},
    {"duk_pop_n", (uintptr_t)duk_pop_n},
    {"duk_push_array", (uintptr_t)duk_push_array},
    {"duk_push_boolean", (uintptr_t)duk_push_boolean},
    {"duk_push_c_function", (uintptr_t)duk_push_c_function},
    {"duk_push_current_function", (uintptr_t)duk_push_current_function},
    {"duk_push_error_object_raw", (uintptr_t)duk_push_error_object_raw},
    {"duk_push_false", (uintptr_t)duk_push_false},
    {"duk_push_int", (uintptr_t)duk_push_int},
    {"duk_push_literal_raw", (uintptr_t)duk_push_literal_raw},
    {"duk_push_lstring", (uintptr_t)duk_push_lstring},
    {"duk_push_nan", (uintptr_t)duk_push_nan},
    {"duk_push_new_target", (uintptr_t)duk_push_new_target},
    {"duk_push_null", (uintptr_t)duk_push_null},
    {"duk_push_number", (uintptr_t)duk_push_number},
    {"duk_push_object", (uintptr_t)duk_push_object},
    {"duk_push_pointer", (uintptr_t)duk_push_pointer},
    {"duk_push_proxy", (uintptr_t)duk_push_proxy},
    {"duk_push_sprintf", (uintptr_t)duk_push_sprintf},
    {"duk_push_string", (uintptr_t)duk_push_string},
    {"duk_push_this", (uintptr_t)duk_push_this},
    {"duk_push_true", (uintptr_t)duk_push_true},
    {"duk_push_uint", (uintptr_t)duk_push_uint},
    {"duk_push_undefined", (uintptr_t)duk_push_undefined},
    {"duk_put_function_list", (uintptr_t)duk_put_function_list},
    {"duk_put_number_list", (uintptr_t)duk_put_number_list},
    {"duk_put_prop", (uintptr_t)duk_put_prop},
    {"duk_put_prop_index", (uintptr_t)duk_put_prop_index},
    {"duk_put_prop_literal_raw", (uintptr_t)duk_put_prop_literal_raw},
    {"duk_put_prop_lstring", (uintptr_t)duk_put_prop_lstring},
    {"duk_put_prop_string", (uintptr_t)duk_put_prop_string},
    {"duk_random", (uintptr_t)duk_random},
    {"duk_remove", (uintptr_t)duk_remove},
    {"duk_replace", (uintptr_t)duk_replace},
    {"duk_samevalue", (uintptr_t)duk_samevalue},
    {"duk_seal", (uintptr_t)duk_seal},
    {"duk_set_finalizer", (uintptr_t)duk_set_finalizer},
    {"duk_set_length", (uintptr_t)duk_set_length},
    {"duk_set_prototype", (uintptr_t)duk_set_prototype},
    {"duk_set_top", (uintptr_t)duk_set_top},
    {"duk_strict_equals", (uintptr_t)duk_strict_equals},
    {"duk_substring", (uintptr_t)duk_substring},
    {"duk_swap", (uintptr_t)duk_swap},
    {"duk_swap_top", (uintptr_t)duk_swap_top},
    {"duk_throw_raw", (uintptr_t)duk_throw_raw},
    {"duk_time_to_components", (uintptr_t)duk_time_to_components},
    {"duk_to_boolean", (uintptr_t)duk_to_boolean},
    {"duk_to_int", (uintptr_t)duk_to_int},
    {"duk_to_int32", (uintptr_t)duk_to_int32},
    {"duk_to_lstring", (uintptr_t)duk_to_lstring},
    {"duk_to_null", (uintptr_t)duk_to_null},
    {"duk_to_number", (uintptr_t)duk_to_number},
    {"duk_to_object", (uintptr_t)duk_to_object},
    {"duk_to_pointer", (uintptr_t)duk_to_pointer},
    {"duk_to_primitive", (uintptr_t)duk_to_primitive},
    {"duk_to_string", (uintptr_t)duk_to_string},
    {"duk_to_uint", (uintptr_t)duk_to_uint},
    {"duk_to_uint16", (uintptr_t)duk_to_uint16},
    {"duk_to_uint32", (uintptr_t)duk_to_uint32},
    {"duk_to_undefined", (uintptr_t)duk_to_undefined},
    {"duk_trim", (uintptr_t)duk_trim}
};


// -----------------------------------------------------------------------------
//  register_native_api - registers the module "native-api"
// -----------------------------------------------------------------------------

static void setup_module_safe(low_main_t *low, void *data)
{
    duk_context *ctx = low_get_duk_context(low);

    // DukTape stack is [module] [exports]

    duk_function_list_entry methods[] = {{"load", native_api_load, 2},
                                         {"loadSync", native_api_load_sync, 1},
                                         {NULL, NULL, 0}};
    duk_put_function_list(low_get_duk_context(low), 1, methods);
}

bool low_register_native_api(low_main_t *low)
{
    return low_module_make_native(low, "native-api", setup_module_safe, NULL);
}


// -----------------------------------------------------------------------------
//  native_api_load
// -----------------------------------------------------------------------------

int native_api_load(duk_context *ctx)
{
    duk_generic_error(ctx, "async version not implemented yet, please use loadSync");
    return 0;
}


// -----------------------------------------------------------------------------
//  native_api_load_sync
// -----------------------------------------------------------------------------

static void *elf_load(char *elf_start, unsigned int size, const char **err, bool *err_malloc)
{
#if defined(__x86_64__) || defined(__i386__)
    Elf_Ehdr *hdr;
    Elf_Phdr *phdr;
    Elf_Shdr *shdr;

    bool exec_has = false;
    unsigned int exec_min, exec_max, exec_size;
    char *exec = NULL;
    char *strings;
    Elf_Sym *syms;
    void *entry = NULL;

    hdr = (Elf_Ehdr *)elf_start;
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
    phdr = (Elf_Phdr *)(elf_start + hdr->e_phoff);
    shdr = (Elf_Shdr *)(elf_start + hdr->e_shoff);

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

    exec = (char *)mmap(NULL, exec_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if(!exec)
    {
        *err = "Memory is full.";
        return NULL;
    }
    exec -= exec_min;

    // Start with clean memory
    memset(exec, 0, exec_size);

    for(int i = 0; i < hdr->e_phnum; i++)
        if (phdr[i].p_type == PT_LOAD && phdr[i].p_filesz) {
            char *start = elf_start + phdr[i].p_offset;
            char *taddr = phdr[i].p_vaddr + exec;
            memmove(taddr, start, phdr[i].p_filesz);
        }

    for(int i=0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNSYM) {
            if(shdr[i].sh_link >= hdr->e_shnum)
                goto range_error;

            // We could check further for range errors than here
            // but the user can break many things if they are allowed to load native
            // modules, so it is not a security critical thing if we do not

            strings = elf_start + shdr[shdr[i].sh_link].sh_offset;
            syms = (Elf_Sym *)(elf_start + shdr[i].sh_offset);

            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Sym); j++) {
                if (strcmp("module_main", strings + syms[j].st_name) == 0) {
                    entry = exec + syms[j].st_value;
                    break;
                }
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

    for(int i=0; i < hdr->e_shnum; i++) {
#if defined(__x86_64__)
        if (shdr[i].sh_type == SHT_RELA)
        {
            Elf_Rela *rel = (Elf_Rela *)(elf_start + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rela); j++)
                if(ELF_R_TYPE(rel[j].r_info) == R_X86_64_JMP_SLOT)
                {
                    const char *sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;

                    int k;
                    for(k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                            break;
                    }
                    if(k == sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t))
                    {
                        munmap(exec, exec_size);
			*err = (char *)low_alloc(80 + strlen(sym));
			sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
			*err_malloc = true;
                        return NULL;
                    }

                    *(uintptr_t *)(exec + rel[j].r_offset) = NATIVE_API_ENTRIES[k].func;
                }
        }
#elif defined(__i386__)
        if (shdr[i].sh_type == SHT_REL)
        {
            Elf_Rel *rel = (Elf_Rel *)(elf_start + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rel); j++)
                if(ELF_R_TYPE(rel[j].r_info) == R_386_JMP_SLOT)
                {
                    const char* sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;

                    int k;
                    for(k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                            break;
                    }
                    if(k == sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t))
                    {
                        munmap(exec, exec_size);
			*err = (char *)low_alloc(80 + strlen(sym));
			sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
			*err_malloc = true;
                        return NULL;
                    }

                    *(uintptr_t *)(exec + rel[j].r_offset) = NATIVE_API_ENTRIES[k].func;
                }
        }
#endif
    }

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

int native_api_load_sync(duk_context *ctx)
{
    const char *path = duk_require_string(ctx, 0);

    int fd = open(path, O_RDONLY);
    if(fd < 0)
        duk_generic_error(ctx, "cannot find module '%s'", path);

    struct stat st;
    if(fstat(fd, &st) < 0)
    {
        close(fd);
        duk_generic_error(ctx, "cannot stat module '%s'", path);
    }
    int len = st.st_size;

    void *data = malloc(len);
    if(!data)
    {
        close(fd);
        duk_generic_error(ctx, "out of memory");
    }

    if(read(fd, data, len) != len)
    {
        free(data);
        close(fd);
        duk_generic_error(ctx, "cannot read module '%s'", path);
    }
    close(fd);

    const char *err;
    bool err_malloc = false;

    int (*module_main)(duk_context *) = (int (*)(duk_context *))elf_load((char *)data, len, &err, &err_malloc);
    free(data);

    if(!module_main)
    {
	if(err_malloc)
	{
		const char *err2 = duk_push_string(ctx, err);
		low_free((void *)err);
	        duk_generic_error(ctx, err2);
	}
	else
	        duk_generic_error(ctx, err);
	}

    return module_main(ctx);
}
