// -----------------------------------------------------------------------------
//  native_adder.c
// -----------------------------------------------------------------------------

#include "low_native.h"
#include "duktape.h"


// -----------------------------------------------------------------------------
//  native_adder_method_add - add method
// -----------------------------------------------------------------------------

int native_adder_method_add(duk_context *ctx)
{
    int a = duk_get_int_default(ctx, 0, 0);
    int b = duk_get_int_default(ctx, 1, 0);

    int res = a + b;

    duk_push_int(ctx, res);
    return 1;
}


// -----------------------------------------------------------------------------
//  module_main
// -----------------------------------------------------------------------------

int module_main(duk_context *ctx)
{
    duk_push_object(ctx);

    duk_function_list_entry methods[] = {{"add", native_adder_method_add, 2},
                                         {NULL, NULL, 0}};
    duk_put_function_list(ctx, -1, methods);

    return 1;
}
