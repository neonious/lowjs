// -----------------------------------------------------------------------------
//  low_module.cpp
// -----------------------------------------------------------------------------

#include "low_module.h"
#include "low_alloc.h"
#include "low_config.h"
#include "low_fs.h"
#include "low_main.h"
#include "low_system.h"
#include "low_native_api.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Global variables
extern low_system_t g_low_system;

extern duk_function_list_entry g_low_native_methods[];
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
extern duk_function_list_entry g_low_native_neon_methods[];
extern bool gInRequire;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#if defined(LOWJS_SERV)
#define NOT_ESP32_ADD_1     + 1

int client_stat2(const char *path, struct stat *st);
#else
#define NOT_ESP32_ADD_1
#define client_stat2 stat 
#endif /* __XTENSA__ */


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
    duk_put_prop_string(ctx, -2, "native_modules");

    duk_push_object(ctx);

    // Add native object, only resolvable from lib:
    duk_push_object(ctx);
    duk_push_object(ctx);
    duk_put_function_list(ctx, -1, g_low_native_methods);
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

bool low_module_make_native(low_t *low,
                            const char *name,
                            void (*setup_cb)(low_t *main, void *data),
                            void *setup_cb_data)
{
    duk_context *ctx = low_get_duk_context(low);
    duk_pop_n(ctx, duk_get_top(ctx));

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
    setup_cb(low, setup_cb_data);

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

    duk_pop(ctx);
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        gInRequire = true;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(!low_module_resolve_c(ctx, path, ".", res_id))
        {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
            gInRequire = false;
            if(!neonious_start_result("FILE_NOT_FOUND"))
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                duk_type_error(ctx, "cannot resolve module '%s'", path);
            return 1;
        }
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        neonious_start_result(NULL);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        low_load_module(ctx, res_id, false);
    }
    else
        low_load_module(ctx, "lib:main", false);

    return 0;
}

bool low_module_main(low_t *low, const char *path)
{
    try
    {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
            {
                // Check for uncaughtException handler
                if(!low->signal_call_id)
                {
                    low_duk_print_error(low->duk_ctx);
                    duk_pop(low->duk_ctx);
                    return false;
                }

                low_push_stash(low->duk_ctx, low->signal_call_id, false);
                duk_push_string(low->duk_ctx, "emit");
                duk_push_string(low->duk_ctx, "uncaughtException");
                duk_dup(low->duk_ctx, -4);
                low->in_uncaught_exception = true;
                duk_call_prop(low->duk_ctx, -4, 2);
                low->in_uncaught_exception = false;

                if(!duk_require_boolean(low->duk_ctx, -1))
                {
                    duk_pop_2(low->duk_ctx);
                    low_duk_print_error(low->duk_ctx);
                    duk_pop(low->duk_ctx);
                    return false;
                }
                duk_pop_3(low->duk_ctx);
            }
        }
        else
        {
            duk_pop(low->duk_ctx);
            return true;
        }
    }
    catch(std::exception &e)
    {
        fprintf(stderr, "Fatal exception: %s\n", e.what());
    }
    catch(...)
    {
        fprintf(stderr, "Fatal exception\n");
    }
    low->in_uncaught_exception = false;

    return false;
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    gInRequire = true;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(!low_module_resolve_c(ctx, id, parent_id, res_id))
    {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        duk_type_error(
          ctx, "cannot resolve module '%s', parent '%s'", id, parent_id);
        return 1;
    }
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    // Try to find in cache
    low_load_module(ctx, res_id, true);

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

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    gInRequire = true;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(low_module_resolve_c(ctx, id, parent_id, res_id))
    {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        duk_push_string(ctx, res_id);
        return 1;
    }
    else
    {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
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
//  low_load_module
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
bool get_data_block(const char *path,
                    unsigned char **data,
                    int *len,
                    bool showErr,
                    bool escapeZero = false);

void code_gc();
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

void low_load_module(duk_context *ctx, const char *path, bool parent_on_stack)
{
    low_t *low = duk_get_low_context(ctx);

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    code_gc();
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    int flags = 0;
    int len = strlen(path);

    bool isLib = memcmp(path, "lib:", 4) == 0, addLow = false;

    int tillDot;
    for(tillDot = len; tillDot > 0; tillDot--)
        if(path[tillDot] == '.')
            break;

    if(len - tillDot == 5 && path[tillDot] == '.' &&
    (path[tillDot + 1] == 'j' || path[tillDot + 1] == 'J') &&
    (path[tillDot + 2] == 's' || path[tillDot + 2] == 'S') &&
    (path[tillDot + 3] == 'o' || path[tillDot + 3] == 'O') &&
    (path[tillDot + 4] == 'n' || path[tillDot + 4] == 'N'))
        flags |= LOW_MODULE_FLAG_JSON;
    else if(len - tillDot == 4 && path[tillDot] == '.' &&
    (path[tillDot + 1] == 'l' || path[tillDot + 1] == 'L') &&
    (path[tillDot + 2] == 'o' || path[tillDot + 2] == 'O') &&
    (path[tillDot + 3] == 'w' || path[tillDot + 2] == 'W'))
        flags |= LOW_MODULE_FLAG_DUK_FORMAT;
    else if(len - tillDot == 3 && path[tillDot] == '.' &&
    (path[tillDot + 1] == 's' || path[tillDot + 1] == 'S') &&
    (path[tillDot + 2] == 'o' || path[tillDot + 2] == 'O'))
        flags |= LOW_MODULE_FLAG_NATIVE;
    else if(isLib)
    {
        addLow = true;

        flags |= LOW_MODULE_FLAG_DUK_FORMAT;
        if(strcmp(path, "lib:init") == 0)
            flags |= LOW_MODULE_FLAG_GLOBAL;
    }

    const char *cacheName;
    if(isLib)
        cacheName = "lib_modules";
    else if(flags & LOW_MODULE_FLAG_NATIVE)
        cacheName = "native_modules";
    else
        cacheName = "modules";

    // Try to find in cache
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, cacheName);
    if(duk_get_prop_string(ctx, -1, path))
    {
        duk_remove(ctx, -2);
        duk_remove(ctx, -2);
        return;
    }
    duk_pop_2(ctx); // stash is kept on stack

    unsigned char *data;
    struct stat st;

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    gInRequire = true;

    char *txt;
    bool try2 = false;

    if(len > 1000)
        goto cantLoad;

    txt = (char *)low_alloc(1024);
    if(!txt)
        goto cantLoad;

    if(isLib)
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        sprintf(txt, addLow ? "/lib/%s.low" : "/lib/%s", path + 4);
#endif
    else if(memcmp(path, "module:", 7) == 0)
        sprintf(txt, "/modules%s", path + 7);
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

    if(!get_data_block(txt, &data, &len, true, true))
    {
        struct stat st;
        if(client_stat2(txt NOT_ESP32_ADD_1, &st) == 0)
        {
            low_free(txt);
            goto cantLoad;
        }
        else
        {
            if(try2)
            {
                sprintf(txt, "/fs/user%s", path);
                if(!get_data_block(txt, &data, &len, true, true))
                {
                    low_free(txt);

                    struct stat st;
                    if(client_stat2(txt NOT_ESP32_ADD_1, &st) == 0)
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
        gInRequire = false;
        duk_type_error(ctx, "cannot find module '%s'", path);
    }

    gInRequire = false;
#else
    int fd;
    if(isLib)
    {
        if(len > 1000)
            goto cantLoad;

        char *txt = (char *)low_alloc(1024);
        if(!txt)
            goto cantLoad;

        sprintf(txt, addLow ? "%s%s.low" : "%s%s", g_low_system.lib_path, path + 4);

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

    // [... stash module]
    if(duk_get_prop_string(ctx, -2, "main") || (flags & LOW_MODULE_FLAG_GLOBAL))
    {
        duk_put_prop_string(ctx, -2, "main");
        duk_remove(ctx, -2);
    }
    else
    {
        // save main
        duk_pop(ctx);
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, -3, "main");
        duk_dup(ctx, -1);
        duk_put_prop_string(ctx, -2, "main");
    }
    // [... module]

    if(parent_on_stack)
    {
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

    if(!(flags & LOW_MODULE_FLAG_GLOBAL))
    {
        // Cache module
        duk_push_heap_stash(ctx);

        duk_get_prop_string(ctx, -1, cacheName);
        duk_dup(ctx, -3);
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
    else if(flags & LOW_MODULE_FLAG_NATIVE)
    {
        if(low->disallow_native)
            duk_generic_error(ctx, "loading of native modules is disabled on this system");

        const char *err;
        bool err_malloc = false;

        void *module_main = native_api_load((char *)data, len, &err, &err_malloc);
        low_free(data);

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
    
        duk_push_c_lightfunc(ctx, native_api_call, 3, 0, 0);
        duk_dup(ctx, -2);
        duk_get_prop_string(ctx, -1, "exports");
        void **params = (void **)duk_push_fixed_buffer(ctx, 2 * sizeof(void *));
        params[0] = module_main;
        params[1] = (void *)path;
        duk_call(ctx, 3);
        duk_pop(ctx);
    }
    else
    {
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

        if(!isLib)
            duk_put_prop_string(ctx, -2, "require");

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
            if(low->module_transpile_hook)
                low->module_transpile_hook(ctx);
            duk_push_string(ctx, "\n}"); /* Newline allows module last line to
                                            contain a // comment. */
            duk_concat(ctx, 3);

            duk_push_string(ctx, path);
            duk_compile(ctx, DUK_COMPILE_FUNCTION);
        }

        /* [ ... module [require] func ] */

        /* call the function wrapper */
        duk_get_prop_string(ctx, isLib ? -3 : -2, "exports"); /* exports */
        duk_dup(ctx, -1);
        if(isLib)
        {
            duk_dup(ctx, -4); /* require */
            duk_remove(ctx, -5);
        }
        else
            duk_get_prop_string(ctx, -4, "require"); /* require */
        duk_dup(ctx, -5);                            /* module */

        duk_push_string(ctx, path); /* __filename */
        for(len = strlen(path) - 1; len > 0; len--)
            if(path[len] == '/')
                break;
        duk_push_lstring(ctx, path, len == -1 ? 0 : len); /* __dirname */
        duk_call_method(ctx, 5);

        /* [ ... module result ] */

        duk_pop(ctx);
    }

    /* module.loaded = true */
    duk_push_true(ctx);
    duk_put_prop_string(ctx, -2, "loaded");
    return;

cantLoad:
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    gInRequire = false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    duk_type_error(ctx, "cannot read module '%s' into memory", path);
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

    if(strcmp(module_id, "native") == 0)
    {
        if(parent_id && memcmp(parent_id, "lib:", 4) == 0)
        {
            strcpy(res_id, "lib:native");
            return true;
        }
        else
            return false;
    }

    bool is_not_absolute_path = false;
    int i;
    for(i = 0; module_id[i]; i++)
        if(module_id[i] == '/' && module_id[i + 1] == '.')
        {
            is_not_absolute_path = true;
            break;
        }

    if(module_id[0] != '.' && module_id[0] != '/' && !is_not_absolute_path
    && i < 1000 && strcmp(module_id, "init") != 0 && strcmp(module_id, "main") != 0)
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        sprintf(res_id, "/lib/%s.low", module_id);
#else
        sprintf(res_id, "%s%s.low", g_low_system.lib_path, module_id);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0)
        {
            sprintf(res_id, "lib:%s", module_id);
            return true;
        }

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        sprintf(res_id, "/lib/%s.low", module_id);
#else
        sprintf(res_id, "%s%s", g_low_system.lib_path, module_id);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0)
        {
            sprintf(res_id, "lib:%s", module_id);
            return true;
        }
    }

    int parent_id_len = parent_id ? strlen(parent_id) : 0;
    if(!parent_id || (parent_id_len >= 4 && memcmp(parent_id, "lib:", 4) == 0))
        return false;

    const char *parent_end = NULL;
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    bool user_space = parent_id_len < 7 || memcmp(parent_id, "module:", 7) != 0;
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

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        char *start = res_id + (!user_space ? 8 : 8); // /fs/user
#endif                            /* LOW_ESP32_LWIP_SPECIALITIES */

        bool isFolder = path[-1] == '/';
        if(isFolder)
            path--;

        if(!isFolder)
        {
            // LOAD_AS_FILE
            path[0] = 0;
            if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
            if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
            if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

            if(path + 3 - res_id >= 1024)
                return false;
            strcpy(path, ".so");
            if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
            {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
        }

        // LOAD_AS_DIRECTORY

        if(path + 13 - res_id >= 1024)
            return false;
        strcpy(path, "/package.json");

        if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
        {
            int len = st.st_size;
            void *data = duk_push_buffer(ctx, len, false);

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
            unsigned char *cdata;
            int clen;

            if(!get_data_block(res_id, &cdata, &clen, true, true))
                return false;
            if(len != clen)
            {
                duk_pop(ctx);
                low_free(cdata);
                return false;
            }

            memcpy(data, cdata, len);
#else
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
#endif

            // Read package.json content
            duk_buffer_to_string(ctx, -1);
            duk_json_decode(ctx, -1);

            if(duk_get_prop_string(ctx, -1, "main"))
            {
                const char *str = duk_get_string(ctx, -1);
                len = strlen(str);

                char *res_id2 = (char *)duk_push_buffer(ctx, 1024, false);
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
                if(client_stat2(res_id2, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

                if(path + 3 - res_id2 >= 1024)
                    return false;
                strcpy(path, ".so");
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

                if(path + 9 - res_id2 >= 1024)
                    return false;
                strcpy(path, "/index.js");
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

                if(path + 9 - res_id2 >= 1024)
                    return false;
                strcpy(path, "/index.so");
                if(client_stat2(res_id2 NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
                {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

                duk_pop_3(ctx);
            }
            else
                duk_pop_2(ctx);
        }

        if(path + 9 - res_id >= 1024)
            return false;
        strcpy(path, "/index.js");
        if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
        {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
        if(client_stat2(res_id, &st) == 0 && S_ISREG(st.st_mode))
        {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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

        if(path + 9 - res_id >= 1024)
            return false;
        strcpy(path, "/index.so");
        if(client_stat2(res_id NOT_ESP32_ADD_1, &st) == 0 && S_ISREG(st.st_mode))
        {
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
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
    }
    return false;
}
