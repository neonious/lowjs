// -----------------------------------------------------------------------------
//  transpile.cpp
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

    // see node_modules/@babel/standalone/src/generated/plugins.js for supported plugins
    duk_push_string(ctx, "{"
        "\"presets\": [\"es2015\", \"stage-3\"],"
        "\"plugins\": [\"proposal-object-rest-spread\"],"
        "\"parserOpts\": {\"allowReturnOutsideFunction\": true"
    "}}");
    duk_json_decode(ctx, 2);
    transpile_config_stash = low_add_stash(ctx, 2);

    return true;
}

bool init_transpile(low_t *low)
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

    duk_context *ctx = low_get_duk_context(low);
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

    low->module_transpile_hook = transpile;
    return true;
}


// -----------------------------------------------------------------------------
//  transpile
// -----------------------------------------------------------------------------

int transpile(duk_context *ctx)
{
    low_push_stash(ctx, transpile_babel_stash, false);
    duk_push_string(ctx, "transform");
    duk_dup(ctx, -3);
    low_push_stash(ctx, transpile_config_stash, false);

    // [code babel result]
    duk_call_prop(ctx, -4, 2);

    // [code babel result codeOut]
    duk_get_prop_string(ctx, -1, "code");

    duk_remove(ctx, -2);
    duk_remove(ctx, -2);
    duk_remove(ctx, -2);

    return 1;
}
