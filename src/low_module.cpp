// -----------------------------------------------------------------------------
//  low_module.cpp
// -----------------------------------------------------------------------------

#include "low_module.h"
#include "low_alloc.h"
#include "low_config.h"
#include "low_fs.h"
#include "low_main.h"
#include "low_system.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Global variables
extern low_system_t g_low_system;

extern duk_function_list_entry g_low_native_methods[];
#if LOW_ESP32_LWIP_SPECIALITIES
extern duk_function_list_entry g_low_native_neon_methods[];
#endif /* LOW_ESP32_LWIP_SPECIALITIES */


// -----------------------------------------------------------------------------
//  low_module_init
// -----------------------------------------------------------------------------

void low_module_init(duk_context *ctx)
{
    // Initialize internal require cache
    duk_push_heap_stash(ctx);

    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, "modules");

    duk_push_object(ctx);

    // Add native object, only resolvable from lib:
    duk_push_object(ctx);
    duk_push_object(ctx);
    duk_put_function_list(ctx, -1, g_low_native_methods);
#if LOW_ESP32_LWIP_SPECIALITIES
    duk_put_function_list(ctx, -1, g_low_native_neon_methods);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    duk_put_prop_string(ctx, -2, "exports");
    duk_put_prop_string(ctx, -2, "lib:native");

    duk_put_prop_string(ctx, -2, "lib_modules");

    duk_pop(ctx);
}

// -----------------------------------------------------------------------------
//  low_module_make_native
// -----------------------------------------------------------------------------

static duk_ret_t low_module_make_native_safe(duk_context *ctx, void *udata)
{
    low_main_t *low = *(low_main_t **)udata;
    char *name = (char *)(((void **)udata)[1]);
    void (*setup_safe_cb)(low_main_t * main, void *data) =
      (void (*)(low_main_t * main, void *data))(((void **)udata)[2]);
    void *setup_safe_cb_data = ((void **)udata)[3];

    duk_push_object(ctx); // our new module!

    duk_push_string(ctx, name);
    duk_put_prop_string(ctx, -2, "id");

    duk_push_null(ctx);
    duk_put_prop_string(ctx, -2, "filename");

    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, "exports");

    duk_push_false(ctx);
    duk_put_prop_string(ctx, -2, "loaded");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "paths");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "children");

    duk_push_object(ctx);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "childrenMap");

    // [... module]

    // require function
    duk_push_c_function(ctx, low_module_require, 1);

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "require"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "module");

    // [... module require]

    // require.cache
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, "modules");
    duk_put_prop_string(ctx, -3, "cache");
    duk_pop(ctx);

    // require.resolve
    duk_push_c_function(ctx, low_module_resolve, 2);

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "resolve"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_put_prop_string(ctx, -2, "resolve");

    duk_put_prop_string(ctx, -2, "require");

    // [... module]

    duk_get_prop_string(ctx, 0, "exports");
    setup_safe_cb(low, setup_safe_cb_data);

    /* module.loaded = true */
    duk_push_true(ctx);
    duk_put_prop_string(ctx, 0, "loaded");

    // Add to lib_modules
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "lib_modules");
    duk_dup(ctx, 0);

    char txt[80];
    sprintf(txt, "lib:%s", name);
    duk_put_prop_string(ctx, -2, txt);

    return 0;
}

bool low_module_make_native(low_main_t *low,
                            const char *name,
                            void (*setup_safe_cb)(low_main_t *main, void *data),
                            void *setup_safe_cb_data)
{
    void *data[4] = {
      (void *)low, (void *)name, (void *)setup_safe_cb, setup_safe_cb_data};
    if(duk_safe_call(
         low->duk_ctx, low_module_make_native_safe, (void *)data, 0, 1) !=
       DUK_EXEC_SUCCESS)
    {
        if(!low->duk_flag_stop) // flag stop also produces error
            low_duk_print_error(low->duk_ctx);
        duk_pop(low->duk_ctx);

        return low->duk_flag_stop;
    }

    duk_pop(low->duk_ctx);
    return true;
}

// -----------------------------------------------------------------------------
//  low_module_main
// -----------------------------------------------------------------------------

bool neonious_start_result(const char *code);

static duk_ret_t low_module_main_safe(duk_context *ctx, void *udata)
{
    char *path = (char *)udata;
    if(path)
    {
        char *res_id = (char *)duk_push_fixed_buffer(ctx, 1024);
        if(!low_module_resolve_c(ctx, path, ".", res_id))
        {
#if LOW_ESP32_LWIP_SPECIALITIES
            if(!neonious_start_result("FILE_NOT_FOUND"))
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                duk_type_error(ctx, "cannot resolve module '%s'", path);
            return 1;
        }

#if LOW_ESP32_LWIP_SPECIALITIES
        neonious_start_result(NULL);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        low_module_run(ctx, res_id, LOW_MODULE_FLAG_MAIN);
    }
    else
        low_module_run(ctx, "lib:main", LOW_MODULE_FLAG_MAIN);

    return 0;
}

bool low_module_main(low_main_t *low, const char *path)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    if(duk_safe_call(low->duk_ctx, low_module_main_safe, (void *)path, 0, 1) !=
       DUK_EXEC_SUCCESS)
#else
    char path2[PATH_MAX];
    if(path)
        realpath(path, path2);

    if(duk_safe_call(low->duk_ctx,
                     low_module_main_safe,
                     (void *)(path ? path2 : NULL),
                     0,
                     1) != DUK_EXEC_SUCCESS)
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    {
        if(!low->duk_flag_stop) // flag stop also produces error
            low_duk_print_error(low->duk_ctx);
        duk_pop(low->duk_ctx);

        return low->duk_flag_stop;
    }
    duk_pop(low->duk_ctx);
    return true;
}

// -----------------------------------------------------------------------------
//  low_module_run
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES
bool get_data_block(const char *path,
                    unsigned char **data,
                    int *len,
                    bool showErr,
                    bool escapeZero = false);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

  void
  low_module_run(duk_context *ctx, const char *path, int flags)
{
    unsigned char *data;
    int len;
    struct stat st;

    bool isLib = memcmp(path, "lib:", 4) == 0;

    int fullLen = strlen(path);
    for(len = fullLen; len >= 0; len--)
        if(path[len] == '.')
            break;
    if(fullLen - len == 5 && path[len] == '.' && (path[len + 1] == 'j' || path[len + 1] == 'J') &&
       (path[len + 2] == 's' || path[len + 2] == 'S') &&
       (path[len + 3] == 'o' || path[len + 3] == 'O') &&
       (path[len + 4] == 'n' || path[len + 4] == 'N'))
        flags |= LOW_MODULE_FLAG_JSON;

#if LOW_ESP32_LWIP_SPECIALITIES
    char *txt;
    bool try2 = false;

    if(len > 1000)
        goto cantLoad;

    txt = (char *)low_alloc(1024);
    if(!txt)
        goto cantLoad;

    if(isLib)
    {
        sprintf(txt, "/lib/%s.low", path + 4);
        flags |= LOW_MODULE_FLAG_DUK_FORMAT;
    }
    else if(memcmp(path, "module:", 7) == 0)
    {
        sprintf(txt, "/modules%s", path + 7);
    }
    else if(path[0] == '/')
    {
        try2 = true;
        sprintf(txt, "/fs/user/.build%s", path);
    }
    else
    {
        low_free(txt);
        goto cantFind;
    }

    if(!get_data_block(txt, &data, &len, true))
    {
        struct stat st;
        if(stat(txt, &st) == 0)
        {
            low_free(txt);
            goto cantLoad;
        }
        else
        {
            if(try2)
            {
                sprintf(txt, "/fs/user%s", path);
                if(!get_data_block(txt, &data, &len, true))
                {
                    low_free(txt);

                    struct stat st;
                    if(stat(txt, &st) == 0)
                        goto cantLoad;
                    else
                        goto cantFind;
                }
            }
            else
            {
                low_free(txt);
                goto cantFind;
            }
        }
    }
    low_free(txt);

    if(0)
    {
    cantFind:
        duk_type_error(ctx, "cannot find module '%s'", path);
    }
#else
    int fd;
    if(isLib)
    {
        if(len > 1000)
            goto cantLoad;

        char *txt = (char *)low_alloc(1024);
        if(!txt)
            goto cantLoad;

        sprintf(txt, "%s%s.low", g_low_system.lib_path, path + 4);
        flags |= LOW_MODULE_FLAG_DUK_FORMAT;

        fd = open(txt, O_RDONLY);
        low_free(txt);
    }
    else
        fd = open(path, O_RDONLY);
    if(fd < 0)
        duk_type_error(ctx, "cannot find module '%s'", path);

    if(fstat(fd, &st) < 0)
    {
        close(fd);
        goto cantLoad;
    }
    len = st.st_size; // TODO: use buffer object so we no longer have a memory
                      // leak!
    data = (unsigned char *)low_alloc(len);
    if(!data)
    {
        close(fd);
        goto cantLoad;
    }
    if(read(fd, data, len) != len)
    {
        low_free(data);
        close(fd);
        goto cantLoad;
    }
    close(fd);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    duk_push_object(ctx); // our new module!

    if(flags & LOW_MODULE_FLAG_MAIN)
    {
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, -2, "main");
    }
    else if(!(flags & LOW_MODULE_FLAG_GLOBAL))
    {
        duk_get_prop_string(ctx, -2, "main");
        duk_put_prop_string(ctx, -2, "main");

        duk_dup(ctx, -2);
        duk_put_prop_string(ctx, -2, "parent");
    }

    duk_push_string(ctx, path);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "filename");
    duk_put_prop_string(ctx, -2, "id");

    if(!(flags & LOW_MODULE_FLAG_JSON))
    {
        if(flags & LOW_MODULE_FLAG_GLOBAL)
            duk_push_global_object(ctx);
        else
            duk_push_object(ctx);
        duk_put_prop_string(ctx, -2, "exports");
    }

    duk_push_false(ctx);
    duk_put_prop_string(ctx, -2, "loaded");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "paths");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "children");

    duk_push_object(ctx);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "childrenMap");

    // [... module]

    // require function
    duk_push_c_function(ctx, low_module_require, 1);

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "require"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "module");

    // [... module require]

    // require.cache
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, "modules");
    duk_put_prop_string(ctx, -3, "cache");
    duk_pop(ctx);

    // require.resolve
    duk_push_c_function(ctx, low_module_resolve, 1);

    duk_dup(ctx, -3);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "resolve"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_put_prop_string(ctx, -2, "resolve");

    // require.main
    duk_get_prop_string(ctx, -2, "main");
    duk_put_prop_string(ctx, -2, "main");

    if(!isLib && !(flags & LOW_MODULE_FLAG_JSON)) // security problem
        duk_put_prop_string(ctx, -2, "require");

    // [... module [require]]

    if(!(flags & LOW_MODULE_FLAG_GLOBAL))
    {
        // Cache module
        duk_push_heap_stash(ctx);

        duk_get_prop_string(
          ctx, -1, memcmp(path, "lib:", 4) == 0 ? "lib_modules" : "modules");
        duk_dup(ctx, !isLib && !(flags & LOW_MODULE_FLAG_JSON) ? -3 : -4);
        duk_put_prop_string(ctx, -2, path);
        duk_pop_2(ctx);
    }

    if(flags & LOW_MODULE_FLAG_JSON)
    {
        duk_push_lstring(ctx, (char *)data, len);
        low_free(data);
        duk_json_decode(ctx, -1);

        /* [ ... module exports ] */

        duk_put_prop_string(ctx, -2, "exports");
    }
    else
    {
        if(flags & LOW_MODULE_FLAG_DUK_FORMAT)
        {
            memcpy(duk_push_fixed_buffer(ctx, len),
                   data,
                   len); // TODO: remove copy
            low_free(data);
            duk_load_function(ctx);
        }
        else
        {
            // TODO: remove concat
            bool shebang = len >= 2 && data[0] == '#' && data[1] == '!';
            duk_push_string(
              ctx,
              shebang
                ? "function(exports,require,module,__filename,__dirname){//"
                : "function(exports,require,module,__filename,__dirname){");
            duk_push_lstring(ctx, (char *)data, len);
            low_free(data);
            duk_push_string(ctx, "\n}"); /* Newline allows module last line to
                                            contain a // comment. */
            duk_concat(ctx, 3);

            duk_push_string(ctx, path);
            duk_compile(ctx, DUK_COMPILE_FUNCTION);
        }

        /* [ ... module [require] func ] */

        /* call the function wrapper */
        duk_get_prop_string(ctx, isLib ? -3 : -2, "exports"); /* exports */
        if(isLib)
        {
            duk_dup(ctx, -3); /* require */
            duk_remove(ctx, -4);
        }
        else
            duk_get_prop_string(ctx, -3, "require"); /* require */
        duk_dup(ctx, -4);                            /* module */

        duk_push_string(ctx, path); /* __filename */
        for(len = strlen(path) - 1; len > 0; len--)
            if(path[len] == '/')
                break;
        duk_push_lstring(ctx, path, len == -1 ? 0 : len); /* __dirname */
        duk_call(ctx, 5);

        /* [ ... module result ] */

        duk_pop(ctx);
    }

    /* module.loaded = true */
    duk_push_true(ctx);
    duk_put_prop_string(ctx, -2, "loaded");

    return;

cantLoad:
    duk_type_error(ctx, "cannot read module '%s' into memory", path);
}

// -----------------------------------------------------------------------------
//  low_module_require - returns cached module or loads it
// -----------------------------------------------------------------------------

duk_ret_t low_module_require(duk_context *ctx)
{
    char *res_id = (char *)duk_push_fixed_buffer(ctx, 1024);

    const char *id = duk_require_string(ctx, 0);

    // Get parent ID
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx,
                        -1,
                        "\xff"
                        "module");
    duk_remove(ctx, -2);

    const char *parent_id;
    int popCount = 0;
    while(true)
    {
        duk_get_prop_string(ctx, -1, "filename");
        parent_id = duk_get_string(ctx, -1);
        duk_pop(ctx);

        if(parent_id)
            break;

        // If a module does not have a filename (vm.createContext), then try
        // parent
        popCount++;
        if(!duk_get_prop_string(ctx, -1, "parent"))
            break;
    }
    while(popCount--)
        duk_pop(ctx);

    // We always resolve with our own function
    if(!low_module_resolve_c(ctx, id, parent_id, res_id))
    {
        duk_type_error(
          ctx, "cannot resolve module '%s', parent '%s'", id, parent_id);
        return 1;
    }

    // Try to find in cache
    duk_push_heap_stash(ctx);
    duk_get_prop_string(
      ctx, -1, memcmp(res_id, "lib:", 4) == 0 ? "lib_modules" : "modules");
    if(duk_get_prop_string(ctx, -1, res_id))
    {
        duk_remove(ctx, -2);
        duk_remove(ctx, -2);
    }
    else
    {
        duk_pop_3(ctx);

        // [ id parent ]

        low_module_run(ctx, res_id, 0);
    }

    // [ id parent module ]

    if(memcmp(parent_id, "lib:", 4) != 0) // security check
    {
        duk_get_prop_string(ctx,
                            -2,
                            "\xff"
                            "childrenMap");
        if(duk_get_prop_string(ctx, -1, res_id))
            duk_pop_2(ctx);
        else
        {
            duk_push_boolean(ctx, true);
            duk_put_prop_string(ctx, -3, res_id);
            duk_pop_2(ctx);

            // Add to children
            duk_get_prop_string(ctx, -2, "children");
            duk_get_prop_string(ctx, -1, "length");
            duk_dup(ctx, -3);
            duk_put_prop(ctx, -3);
            duk_pop(ctx);
        }
    }

    duk_get_prop_string(ctx, -1, "exports");
    return 1;
}

// -----------------------------------------------------------------------------
//  low_module_resolve - resolves path to module
// -----------------------------------------------------------------------------

duk_ret_t low_module_resolve(duk_context *ctx)
{
    char *res_id = (char *)duk_push_fixed_buffer(ctx, 1024);

    const char *id = duk_require_string(ctx, 0);

    // Get parent ID
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx,
                        -1,
                        "\xff"
                        "module");
    duk_remove(ctx, -2);

    const char *parent_id;
    int popCount = 1;
    while(true)
    {
        duk_get_prop_string(ctx, -1, "filename");
        parent_id = duk_get_string(ctx, -1);
        duk_pop(ctx);

        if(parent_id)
            break;

        // If a module does not have a filename (vm.createContext), then try
        // parent
        popCount++;
        if(!duk_get_prop_string(ctx, -1, "parent"))
            break;
    }
    while(popCount--)
        duk_pop(ctx);

    if(low_module_resolve_c(ctx, id, parent_id, res_id))
    {
        duk_push_string(ctx, res_id);
        return 1;
    }
    else
    {
        duk_type_error(
          ctx, "cannot resolve module '%s', parent '%s'", id, parent_id);
        return 1;
    }
}

// -----------------------------------------------------------------------------
//  low_module_make
// -----------------------------------------------------------------------------

duk_ret_t low_module_make(duk_context *ctx)
{
    duk_push_object(ctx); // our new module!

    duk_get_prop_string(ctx, -2, "main");
    duk_put_prop_string(ctx, -2, "main");

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "parent");

    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "id");

    duk_push_null(ctx);
    duk_put_prop_string(ctx, -2, "filename");

    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, "exports");

    duk_push_false(ctx);
    duk_put_prop_string(ctx, -2, "loaded");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "paths");

    duk_push_array(ctx);
    duk_put_prop_string(ctx, -2, "children");

    duk_push_object(ctx);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "childrenMap");

    // [... module]

    // require function
    duk_push_c_function(ctx, low_module_require, 1);

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "require"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "module");

    // [... module require]

    // require.cache
    duk_push_heap_stash(ctx);

    duk_get_prop_string(ctx, -1, "modules");
    duk_put_prop_string(ctx, -3, "cache");
    duk_pop(ctx);

    // require.resolve
    duk_push_c_function(ctx, low_module_resolve, 2);

    duk_dup(ctx, -2);
    duk_put_prop_string(ctx,
                        -2,
                        "\xff"
                        "module");

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "resolve"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
    duk_put_prop_string(ctx, -2, "resolve");

    // require.main
    duk_get_prop_string(ctx, -2, "main");
    duk_put_prop_string(ctx, -2, "main");

    duk_put_prop_string(ctx, -2, "require");

    // [... module]

    /* module.loaded = true */
    duk_push_true(ctx);
    duk_put_prop_string(ctx, -2, "loaded");

    return 1;
}

// -----------------------------------------------------------------------------
//  low_module_resolve_c - the C version of our resolve function
//                         result must be min 1024 bytes
// -----------------------------------------------------------------------------

bool low_module_resolve_c(duk_context *ctx,
                          const char *module_id,
                          const char *parent_id,
                          char *res_id)
{
    struct stat st;

    // lib: may get native
    if(strcmp(module_id, "native") == 0 && parent_id && memcmp(parent_id, "lib:", 4) == 0)
    {
        strcpy(res_id, "lib:native");
        return true;
    }

    bool is_not_absolute_path = false;
    int i;
    for(i = 0; module_id[i]; i++)
        if(module_id[i] == '.')
        {
            is_not_absolute_path = true;
            break;
        }
    if(!is_not_absolute_path && i < 1000 && strcmp(module_id, "init") != 0 &&
       strcmp(module_id, "main") != 0)
    {
        sprintf(res_id, "lib:%s", module_id);

        duk_push_heap_stash(ctx);
        duk_get_prop_string(ctx, -1, "lib_modules");
        if(duk_get_prop_string(ctx, -1, res_id))
        {
            duk_pop_3(ctx);
            return true;
        }
        duk_pop_3(ctx);

        // system module
        sprintf(res_id, "%s%s.low", g_low_system.lib_path, module_id);
        if(stat(res_id, &st) == 0)
        {
            sprintf(res_id, "lib:%s", module_id);
            return true;
        }
    }

    if(!parent_id || memcmp(parent_id, "lib:", 4) == 0)
        return false;

    const char *parent_end = NULL;
#if LOW_ESP32_LWIP_SPECIALITIES
    bool user_space = memcmp(parent_id, "module:", 7) != 0;
    bool in_base = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    while(true)
    {
        if(!parent_end)
        {
            // For next start
            parent_end = parent_id + strlen(parent_id);
            if(!((module_id[0] == '.' && module_id[1] == '/') ||
                 (module_id[0] == '/') ||
                 (module_id[0] == '.' && module_id[1] == '.' &&
                  module_id[2] == '/')))
                continue;

#if LOW_ESP32_LWIP_SPECIALITIES
            low_fs_resolve(res_id, 1024, parent_id, module_id, NULL, user_space);
            if(!user_space)
                res_id[7] = 's';    // make it /modules/
#else
            low_fs_resolve(res_id, 1024, parent_id, module_id, NULL);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        }
        else
        {
            // Go through node_modules
            while(parent_id != parent_end && parent_id != parent_end && parent_end[-1] != '/')
                parent_end--;
            if(parent_id == parent_end)
            {
#if LOW_ESP32_LWIP_SPECIALITIES
                if(in_base)
                    return false;

                in_base = true;
                user_space = false;
                low_fs_resolve(res_id, 1024, "/modules/", module_id, NULL, false);
#else
                return false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            }
            else
            {
#if LOW_ESP32_LWIP_SPECIALITIES
                low_fs_resolve(res_id, 1024, parent_id, module_id, parent_end, user_space);
                if(!user_space)
                    res_id[7] = 's';    // make it /modules/
#else
                low_fs_resolve(res_id, 1024, parent_id, module_id, parent_end);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                parent_end--;
            }
        }

        char *path = res_id + strlen(res_id);
#if LOW_ESP32_LWIP_SPECIALITIES
        char *start = res_id + (!user_space ? 8 : 8); // /fs/user
#endif                            /* LOW_ESP32_LWIP_SPECIALITIES */

        bool isFolder = path[-1] == '/';
        if(isFolder)
            path--;

        if(!isFolder)
        {
            // LOAD_AS_FILE

            path[0] = 0;
            if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES
                if(!user_space)
                {
                    memcpy(res_id, "module:", 7);
                    memmove(res_id + 7, start, path + 1 - start);
                }
                else
                    memmove(res_id, start, path + 1 - start);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                return true;
            }

            if(path + 3 - res_id >= 1024)
                return false;
            strcpy(path, ".js");
            if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES
                if(!user_space)
                {
                    memcpy(res_id, "module:", 7);
                    memmove(res_id + 7, start, path + 4 - start);
                }
                else
                    memmove(res_id, start, path + 4 - start);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                return true;
            }

            if(path + 5 - res_id >= 1024)
                return false;
            strcpy(path, ".json");
            if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES
                if(!user_space)
                {
                    memcpy(res_id, "module:", 7);
                    memmove(res_id + 7, start, path + 6 - start);
                }
                else
                    memmove(res_id, start, path + 6 - start);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                return true;
            }
        }

        // LOAD_AS_DIRECTORY

        if(path + 13 - res_id >= 1024)
            return false;
        strcpy(path, "/package.json");

        if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
        {
            int len = st.st_size;
            void *data = duk_push_buffer(ctx, len, false);

            int fd = open(res_id, O_RDONLY);
            if(fd < 0)
                return false;

            if(read(fd, data, len) != len)
            {
                close(fd);
                duk_pop(ctx);
                return false;
            }
            close(fd);

            // Read package.json content
            duk_buffer_to_string(ctx, -1);
            duk_json_decode(ctx, -1);

            if(duk_get_prop_string(ctx, -1, "main"))
            {
                const char *str = duk_get_string(ctx, -1);
                len = strlen(str);

                char *res_id2 = (char *)duk_push_buffer(ctx, 1024, false);
#if LOW_ESP32_LWIP_SPECIALITIES
                low_fs_resolve(res_id2, 1024, res_id, str, NULL, false);
                char *start = res_id2 + (!user_space ? 8 : 8); // /fs/user
#else
                low_fs_resolve(res_id2, 1024, res_id, str, NULL);
                char *start = res_id2;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                char *path = res_id2 + strlen(res_id2);

                bool isFolder = path[-1] == '/';
                if(isFolder)
                    path--;

                path[0] = 0;
                if(stat(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES
                    if(!user_space)
                    {
                        memcpy(res_id, "module:", 7);
                        memmove(res_id + 7, start, path + 1 - start);
                    }
                    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                        memmove(res_id, start, path + 1 - start);
                    duk_pop_3(ctx);
                    return true;
                }

                if(path + 3 - res_id2 >= 1024)
                    return false;
                strcpy(path, ".js");
                if(stat(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES
                    if(!user_space)
                    {
                        memcpy(res_id, "module:", 7);
                        memmove(res_id + 7, start, path + 4 - start);
                    }
                    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                        memmove(res_id, start, path + 4 - start);

                    duk_pop_3(ctx);
                    return true;
                }

                if(path + 5 - res_id2 >= 1024)
                    return false;
                strcpy(path, ".json");
                if(stat(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES
                    if(!user_space)
                    {
                        memcpy(res_id, "module:", 7);
                        memmove(res_id + 7, start, path + 6 - start);
                    }
                    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                        memmove(res_id, start, path + 6 - start);

                    duk_pop_3(ctx);
                    return true;
                }

                if(path + 9 - res_id2 >= 1024)
                    return false;
                strcpy(path, "/index.js");
                if(stat(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES
                    if(!user_space)
                    {
                        memcpy(res_id, "module:", 7);
                        memmove(res_id + 7, start, path + 10 - start);
                    }
                    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                        memmove(res_id, start, path + 10 - start);

                    duk_pop_3(ctx);
                    return true;
                }

                if(path + 11 - res_id2 >= 1024)
                    return false;
                strcpy(path, "/index.json");
                if(stat(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES
                    if(!user_space)
                    {
                        memcpy(res_id, "module:", 7);
                        memmove(res_id + 7, start, path + 12 - start);
                    }
                    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                        memmove(res_id, start, path + 12 - start);

                    duk_pop_3(ctx);
                    return true;
                }

                duk_pop_3(ctx);
            }
            else
                duk_pop_2(ctx);
        }

        if(path + 9 - res_id >= 1024)
            return false;
        strcpy(path, "/index.js");
        if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
        {
#if LOW_ESP32_LWIP_SPECIALITIES
            if(!user_space)
            {
                memcpy(res_id, "module:", 7);
                memmove(res_id + 7, start, path + 10 - start);
            }
            else
                memmove(res_id, start, path + 10 - start);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            return true;
        }

        if(path + 11 - res_id >= 1024)
            return false;
        strcpy(path, "/index.json");
        if(stat(res_id, &st) == 0 && S_ISREG(st.st_mode))
        {
#if LOW_ESP32_LWIP_SPECIALITIES
            if(!user_space)
            {
                memcpy(res_id, "module:", 7);
                memmove(res_id + 7, start, path + 12 - start);
            }
            else
                memmove(res_id, start, path + 12 - start);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            return true;
        }
    }
    return false;
}
