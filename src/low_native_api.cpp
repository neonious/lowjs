// -----------------------------------------------------------------------------
//  low_native_api.cpp
// -----------------------------------------------------------------------------

#define __register_frame __register_frame_other_proto
#define __deregister_frame __deregister_frame_other_proto

#include "low_native_api.h"
#include "low_config.h"

#include "low_alloc.h"
#include "low_main.h"
#include "low_module.h"
#include "low_system.h"
#include "low_loop.h"
#include "low_web_thread.h"

#include <duktape.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>

using namespace std;

#if !LOW_ESP32_LWIP_SPECIALITIES
#include <sys/mman.h>
#include <unwind.h>
#include <dlfcn.h>
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

#if defined(__x86_64__) || defined(__aarch64__)
#include <sys/elf64.h>

#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Phdr    Elf64_Phdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Rela    Elf64_Rela
#define Elf_Rel     Elf64_Rel
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE
#elif defined(__i386__) || defined(__arm__)
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

#undef __register_frame
extern "C" void __register_frame(const void *);
#undef __deregister_frame
extern "C" void __deregister_frame(const void *);

#ifdef __arm__
// Do not exist with arm. arm uses setjmp/longjmp...
void __register_frame(const void *) {}
void __deregister_frame(const void *) {}
#endif /* __arm__ */

#ifdef LOWJS_SERV
vector<void *> gNativeAPIRegisteredFrames;
vector<pair<void *, uintptr_t> > gNativeAPIMemMapped;
#endif /* LOWJS_SERV */

#if !LOW_ESP32_LWIP_SPECIALITIES

// Constants
extern low_system_t g_low_system;

struct native_api_entry_t
{
    const char *name;
    uintptr_t func;
};

struct native_api_entry_t NATIVE_API_ENTRIES[] = {
    {"low_load_module", (uintptr_t)low_load_module},

    {"low_call_thread", (uintptr_t)low_call_thread},
    {"low_get_current_thread", (uintptr_t)low_get_current_thread},

    {"low_set_timeout", (uintptr_t)low_set_timeout},
    {"low_clear_timeout", (uintptr_t)low_clear_timeout},

    {"low_call_next_tick", (uintptr_t)low_call_next_tick},

    {"low_add_stash", (uintptr_t)low_add_stash},
    {"low_remove_stash", (uintptr_t)low_remove_stash},
    {"low_push_stash", (uintptr_t)low_push_stash},

    {"low_push_buffer", (uintptr_t)low_push_buffer},
    {"low_push_error", (uintptr_t)low_push_error},

    {"low_alloc_throw", (uintptr_t)low_alloc_throw},

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
//  native_api_load
// -----------------------------------------------------------------------------

void *native_api_load(const char *data, unsigned int size, const char **err, bool *err_malloc)
{
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__arm__)
    const Elf_Ehdr *hdr;
    const Elf_Phdr *phdr;
    const Elf_Shdr *shdr;

    bool exec_has = false;
    uintptr_t exec_min, exec_max, exec_size;
    char *exec = NULL;
    const char *strings;
    const Elf_Sym *syms;
    void *entry = NULL, *exitEntry = NULL;

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
#elif defined(__aarch64__)
    if(hdr->e_ident[EI_CLASS] != ELFCLASS64
    || hdr->e_machine != EM_AARCH64)
    {
        *err = "File is an ELF file, but not one for arm64 machines.";
        return NULL;
    }
#elif defined(__arm__)
    if(hdr->e_ident[EI_CLASS] != ELFCLASS32
    || hdr->e_machine != EM_ARM)
    {
        *err = "File is an ELF file, but not one for arm machines.";
        return NULL;
    }
#endif

    if(!hdr->e_phnum || size < hdr->e_phoff + hdr->e_phnum * sizeof(Elf_Phdr)
    || !hdr->e_shnum || size < hdr->e_shoff + hdr->e_shnum * sizeof(Elf_Shdr))
        goto range_error;
    phdr = (const Elf_Phdr *)(data + hdr->e_phoff);
    shdr = (const Elf_Shdr *)(data + hdr->e_shoff);

    // Get image size
    for(int i = 0; i < hdr->e_shnum; i++)
    {
        if(!shdr[i].sh_size)
            continue;

        uintptr_t end = shdr[i].sh_addr + shdr[i].sh_size;
        if(!exec_has || exec_min > shdr[i].sh_addr)
            exec_min = shdr[i].sh_addr;
        if(!exec_has || exec_max < end)
            exec_max = end;
        exec_has = true;
    }
    if(!exec_has)
    {
        *err = "File is not an ELF file.";
        return NULL;
    }
    exec_min = exec_min & ~15;
    exec_size = exec_max - exec_min;

    // Copy image into memory
    char *base;
    base = exec = (char *)mmap(NULL, exec_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#ifdef LOWJS_SERV
    gNativeAPIMemMapped.push_back(pair<void *, uintptr_t>((void *)base, exec_size));
#endif /* LOWJS_SERV */
    if(!exec)
    {
        *err = "Memory is full.";
        return NULL;
    }
    memset(exec, 0, exec_size);

    exec -= exec_min;
    for(int i = 0; i < hdr->e_phnum; i++)
        if (phdr[i].p_type == PT_LOAD && phdr[i].p_filesz)
        {
            const char *start = data + phdr[i].p_offset;
            char *taddr = phdr[i].p_vaddr + exec;
            uintptr_t size = exec_max - phdr[i].p_vaddr;
            if(size > phdr[i].p_filesz)
                size = phdr[i].p_filesz;
            memmove(taddr, start, size);
        }

    // Get entry point
    for(int i=0; i < hdr->e_shnum; i++)
    {
        if (shdr[i].sh_type == SHT_DYNSYM)
        {
            if(shdr[i].sh_link >= hdr->e_shnum)
                goto range_error;

            // We could check further for range errors than here
            // but the user can break many things if they are allowed to load native
            // modules, so it is not a security critical thing if we do not

            strings = data + shdr[shdr[i].sh_link].sh_offset;
            syms = (const Elf_Sym *)(data + shdr[i].sh_offset);

            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Sym); j++)
            {
                if(strcmp("module_load", strings + syms[j].st_name) == 0)
                {
                    entry = exec + syms[j].st_value;
                    if(exitEntry)
                        break;
                }
                if(strcmp("module_unload", strings + syms[j].st_name) == 0)
                {
                    exitEntry = exec + syms[j].st_value;
                    if(entry)
                        break;
                }
            }
            if(entry && exitEntry)
                break;
        }
    }
    if(!entry)
    {
        munmap(base, exec_size);
        *err = "Entry point module_load not found.";
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

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;

                    for(int k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }

                    if(!func)
                        func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(base, exec_size);
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
                    *(uintptr_t *)(exec + rel[j].r_offset) = (uintptr_t)exec + rel[j].r_addend;
                    break;

                default:
                    munmap(base, exec_size);
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

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;

                    for(int k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }

                    if(!func)
                        func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(base, exec_size);
                        *err = (char *)low_alloc(80 + strlen(sym));
                        sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
                        *err_malloc = true;
                        return NULL;
                    }

                    if(ELF_R_TYPE(rel[j].r_info) == R_386_PC32)
                        *(uintptr_t *)(exec + rel[j].r_offset) += func - (uintptr_t)(exec + rel[j].r_offset);
                    else if(ELF_R_TYPE(rel[j].r_info) == R_386_32)
                        *(uintptr_t *)(exec + rel[j].r_offset) += func;
                    else
                        *(uintptr_t *)(exec + rel[j].r_offset) = func;
                    break;

                case R_386_RELATIVE:
                    *(uintptr_t *)(exec + rel[j].r_offset) += (uintptr_t)exec;
                    break;

                default:
                    munmap(base, exec_size);
                    *err = (char *)low_alloc(80);
                    sprintf((char *)*err, "Unknown relocatable type #%d.", (int)ELF_R_TYPE(rel[j].r_info));
                    *err_malloc = true;
                    return NULL;
                }
        }
#elif defined(__aarch64__)
        if (shdr[i].sh_type == SHT_RELA)
        {
            const Elf_Rela *rel = (const Elf_Rela *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rela); j++)
                switch(ELF_R_TYPE(rel[j].r_info))
                {
                case R_AARCH64_NONE: break;

                case R_AARCH64_ABS64:
                case R_AARCH64_PREL64:
                case R_AARCH64_JUMP_SLOT:
                case R_AARCH64_GLOB_DAT:
                    uintptr_t func;
                    const char *sym;

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;

                    for(int k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }

                    if(!func)
                        func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(base, exec_size);
                        *err = (char *)low_alloc(80 + strlen(sym));
                        sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
                        *err_malloc = true;
                        return NULL;
                    }

                    if(ELF_R_TYPE(rel[j].r_info) == R_AARCH64_PREL64)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend - (uintptr_t)(exec + rel[j].r_offset);
                    else if(ELF_R_TYPE(rel[j].r_info) == R_AARCH64_ABS64)
                        *(uintptr_t *)(exec + rel[j].r_offset) = func + rel[j].r_addend;
                    else
                        *(uintptr_t *)(exec + rel[j].r_offset) = func;
                    break;

                case R_AARCH64_RELATIVE:
                    *(uintptr_t *)(exec + rel[j].r_offset) = (uintptr_t)exec + rel[j].r_addend;
                    break;

                default:
                    munmap(base, exec_size);
                    *err = (char *)low_alloc(80);
                    sprintf((char *)*err, "Unknown relocatable type #%d.", (int)ELF_R_TYPE(rel[j].r_info));
                    *err_malloc = true;
                    return NULL;
                }
        }
#elif defined(__arm__)
        if (shdr[i].sh_type == SHT_REL)
        {
            const Elf_Rel *rel = (const Elf_Rel *)(data + shdr[i].sh_offset);
            for(int j = 0; j < shdr[i].sh_size / sizeof(Elf_Rel); j++)
                switch(ELF_R_TYPE(rel[j].r_info))
                {
                case R_ARM_NONE: break;

                case R_ARM_ABS32:
                case R_ARM_REL32:
                case R_ARM_JUMP_SLOT:
                case R_ARM_GLOB_DAT:
                    uintptr_t func;
                    const char *sym;

                    sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
                    func = 0;

                    for(int k = 0; k < sizeof(NATIVE_API_ENTRIES) / sizeof(native_api_entry_t); k++)
                    {
                        if(strcmp(NATIVE_API_ENTRIES[k].name, sym) == 0)
                        {
                            func = NATIVE_API_ENTRIES[k].func;
                            break;
                        }
                    }

                    if(!func)
                        func = (uintptr_t)dlsym(RTLD_DEFAULT, sym);
                    if(!func)
                    {
                        munmap(base, exec_size);
                        *err = (char *)low_alloc(80 + strlen(sym));
                        sprintf((char *)*err, "File asks for symbol '%s' which low.js does not know of.", sym);
                        *err_malloc = true;
                        return NULL;
                    }

                    if(ELF_R_TYPE(rel[j].r_info) == R_ARM_REL32)
                        *(uintptr_t *)(exec + rel[j].r_offset) += func - (uintptr_t)(exec + rel[j].r_offset);
                    else if(ELF_R_TYPE(rel[j].r_info) == R_ARM_ABS32)
                        *(uintptr_t *)(exec + rel[j].r_offset) += func;
                    else
                        *(uintptr_t *)(exec + rel[j].r_offset) = func;
                    break;

                case R_ARM_RELATIVE:
                    *(uintptr_t *)(exec + rel[j].r_offset) += (uintptr_t)exec;
                    break;

                default:
                    munmap(base, exec_size);
                    *err = (char *)low_alloc(80);
                    sprintf((char *)*err, "Unknown relocatable type #%d.", (int)ELF_R_TYPE(rel[j].r_info));
                    *err_malloc = true;
                    return NULL;
                }
        }
#endif
    }

    // Setup stack unwinding for throws
    for(int i = 0; i < hdr->e_shnum; i++)
    {
        const Elf_Shdr *sh_strtab = &shdr[hdr->e_shstrndx];
        const char *const sh_strtab_p = data + sh_strtab->sh_offset;

        if(strcmp(sh_strtab_p + shdr[i].sh_name, ".eh_frame") == 0)
        {
            const char *P = (const char *)exec + shdr[i].sh_addr;
#ifdef __APPLE__
            // On OS X/BSD __register_frame takes a single FDE as an argument.
            // See http://lists.llvm.org/pipermail/llvm-dev/2013-April/061737.html
            // and projects/libunwind/src/UnwindLevel1-gcc-ext.c
            const char *End = P + shdr[i].sh_size;
            do  {
                uint32_t Length = *(const uint32_t *)P;
                uint32_t Offset = *(const uint32_t *)(P + 4);
                if(Offset != 0)
                {
                    void *n2 = low_alloc(Length);
                    memcpy(n2, P, Length);
                    __register_frame(n2);
#ifdef LOWJS_SERV
                    gNativeAPIRegisteredFrames.push_back(n2);
#endif /* LOWJS_SERV */
                }
                P += 4 + Length;
                if(P > End)
                {
                    munmap(base, exec_size);
                    *err = ".eh_frame section broken.";
                    return NULL;
                }
            } while(P != End);
#else
            for(int j = 0; j < hdr->e_shnum; j++)
            {
                if(i != j
                && shdr[j].sh_addr >= shdr[i].sh_addr + shdr[i].sh_size
                && shdr[j].sh_addr < shdr[i].sh_addr + shdr[i].sh_size + 4)
                {
                    munmap(base, exec_size);
                    *err = "There must not be a section right after .eh_frame, please use low_native_api linker script.";
                    return NULL;
                }
            }
            *(uint32_t *)(P + shdr[i].sh_size) = 0;
            __register_frame(P);
#ifdef LOWJS_SERV
            gNativeAPIRegisteredFrames.push_back((void *)P);
#endif /* LOWJS_SERV */
#endif /* __APPLE__ */

            break;
        }
    }

    // Call constructors, send destructors to atexit
    for(int i = 0; i < hdr->e_shnum; i++)
        if (shdr[i].sh_type == SHT_PREINIT_ARRAY)
        {
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
    if(exitEntry)
        atexit((void (*)(void))exitEntry);

    return entry;

range_error:
    if(exec)
        munmap(base, exec_size);

    *err = "File is a valid ELF file for this machine, but corrupt.";
    return NULL;
#else
    *err = "Native modules are not yet supported on this architecture.";
    return NULL;
#endif
}

#endif /* !LOW_ESP32_LWIP_SPECIALITIES */


// -----------------------------------------------------------------------------
//  native_api_call
// -----------------------------------------------------------------------------

int native_api_call(duk_context *ctx)
{
    void **params = (void **)duk_get_buffer_data(ctx, 2, NULL);
    int (*module_load)(duk_context *, const char *) = (int (*)(duk_context *, const char *))params[0];
    const char *path = (const char *)params[1];
    duk_pop(ctx);

    return module_load(ctx, path);
}


#ifdef LOWJS_SERV

// -----------------------------------------------------------------------------
//  native_api_unload_all
// -----------------------------------------------------------------------------

void native_api_unload_all()
{
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__arm__)
    for(int i = 0; i < gNativeAPIRegisteredFrames.size(); i++)
        __deregister_frame(gNativeAPIRegisteredFrames[i]);
    gNativeAPIRegisteredFrames.clear();

    for(int i = 0; i < gNativeAPIMemMapped.size(); i++)
        munmap(gNativeAPIMemMapped[i].first, gNativeAPIMemMapped[i].second);
    gNativeAPIMemMapped.clear();
#endif
}

#endif /* LOWJS_SERV */
