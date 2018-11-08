// -----------------------------------------------------------------------------
//  native_adder.c
// -----------------------------------------------------------------------------

#include "native_adder.h"


// -----------------------------------------------------------------------------
//  register_native_adder - registers the module "native_adder"
// -----------------------------------------------------------------------------

static void setup_module_safe(low_t *low, void *data)
{
    // DukTape stack is [module] [exports]

    duk_function_list_entry methods[] = {{"add", native_adder_method_add, 2},
                                         {NULL, NULL, 0}};
    duk_put_function_list(
      low_get_duk_context(low), 1, methods); // add to 1 = exports
}

bool register_native_adder(low_t *low)
{
    return low_module_make_native(low, "native_adder", setup_module_safe, NULL);
}


// -----------------------------------------------------------------------------
//  native_adder_method_add - add method
// -----------------------------------------------------------------------------

int native_adder_method_add(duk_context *ctx)
{
    int a = duk_require_int(ctx, 0);
    int b = duk_require_int(ctx, 1);

    int res = a + b;

    duk_push_int(ctx, res);
    return 1;
}
