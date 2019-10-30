// -----------------------------------------------------------------------------
//  transpile.c
// -----------------------------------------------------------------------------

#include "transpile.h"

#include "low_main.h"
#include "low_module.h"
#include "low_system.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


// Global variables
low_main_t *transpile_context;
int transpile_babel_stash, transpile_config_stash;

extern low_system_t g_low_system;


// -----------------------------------------------------------------------------
//  init_transpile
// -----------------------------------------------------------------------------

static duk_ret_t init_transpile_safe(duk_context *ctx, void *udata)
{
    low_load_module(ctx, "lib:babel", false);
    duk_get_prop_string(ctx, 0, "exports");
    transpile_babel_stash = low_add_stash(ctx, 1);

    duk_push_string(ctx, "{\"presets\": [\"es2015\"] }");
    duk_json_decode(ctx, 2);
    transpile_config_stash = low_add_stash(ctx, 2);

    return true;
}

bool init_transpile()
{
    int len = strlen(g_low_system.lib_path);
    char babel_path[len + 16];
    sprintf(babel_path, "%sbabel.low", g_low_system.lib_path);

    struct stat st;
    if(stat(babel_path, &st) == -1 && errno == ENOENT)
    {
        fprintf(stderr, "Error: This distribution lakes Babel, transpilation is not possible.\n");
        return false;
    }

    transpile_context = low_init();
    if(!transpile_context)
        return EXIT_FAILURE;

    if(!low_lib_init(transpile_context))
        return false;

    duk_context *ctx = low_get_duk_context(transpile_context);
    if(duk_safe_call(
        ctx,
        init_transpile_safe,
        NULL, 0, 1) != DUK_EXEC_SUCCESS)
    {
        low_duk_print_error(ctx);
        duk_pop(ctx);
        return false;
    }
    duk_pop(ctx);

    return true;
}


// -----------------------------------------------------------------------------
//  transpile
// -----------------------------------------------------------------------------

static duk_ret_t transpile_safe(duk_context *ctx, void *udata)
{
    const char *in_data = *(const char **)(((uintptr_t *)udata)[0]);
    int in_len = *(int *)(((uintptr_t *)udata)[1]);

    low_push_stash(ctx, transpile_babel_stash, false);
    duk_push_string(ctx, "transform");
    duk_push_lstring(ctx, in_data, in_len);
    low_push_stash(ctx, transpile_config_stash, false);
    duk_call_prop(ctx, 0, 2);
    duk_get_prop_string(ctx, -1, "code");

    return 1;
}

bool transpile(const char *in_data, int in_len,
               char **out_data, int *out_len,
               const char **err, bool *err_malloc)
{
    duk_context *ctx = low_get_duk_context(transpile_context);
    uintptr_t data[2] = {(uintptr_t)&in_data, (uintptr_t)&in_len};

    if(duk_safe_call(
        ctx,
        transpile_safe,
        data, 0, 1) != DUK_EXEC_SUCCESS)
    {
        duk_get_prop_string(ctx, -1, "message");
        *err = strdup(duk_require_string(ctx, -1));
        duk_pop_2(ctx);

        if(!*err)
            *err = "memory full";
        else
            *err_malloc = true;

        return false;
    }

    *out_data = strdup(duk_require_string(ctx, -1));
    if(!*out_data)
    {
        *err = "memory full";
        return false;
    }

    *out_len = duk_get_length(ctx, -1);
    duk_pop(ctx);

    return true;
}