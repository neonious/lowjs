// -----------------------------------------------------------------------------
//  native.c
// -----------------------------------------------------------------------------

#include "low_native.h"
#include "duktape.h"


// -----------------------------------------------------------------------------
//  native_method_simple_add - add method
// -----------------------------------------------------------------------------

int native_method_simple_add(duk_context *ctx)
{
    double a = duk_get_number_default(ctx, 0, 0);
    double b = duk_get_number_default(ctx, 1, 0);

    duk_push_number(ctx, a + b);
    return 1;
}


// -----------------------------------------------------------------------------
//  native_method_cxx_new_test - 
// -----------------------------------------------------------------------------

int constructorDestructorCalls = 0;

class ClassOnHeap
{
public:
    ClassOnHeap() { constructorDestructorCalls++; }
    ~ClassOnHeap() { constructorDestructorCalls++; }
};

int native_method_cxx_new_test(duk_context *ctx)
{
    ClassOnHeap *obj = new ClassOnHeap();
    ClassOnHeap *objV = new ClassOnHeap[2];
    delete obj;
    delete[] objV;

    duk_push_boolean(ctx, constructorDestructorCalls == 6);
    return 1;
}


// -----------------------------------------------------------------------------
//  native_method_cxx_object_heap_test -
// -----------------------------------------------------------------------------

int funcStackTest;

class ClassWithConstructor
{
public:
    ClassWithConstructor() { funcStackTest = 234; }
};

ClassWithConstructor c;

int native_method_cxx_object_heap_test(duk_context *ctx)
{
    duk_push_boolean(ctx, funcStackTest == 234);
    return 1;
}


// -----------------------------------------------------------------------------
//  native_method_cxx_unwind_test -
// -----------------------------------------------------------------------------

bool destructed = false;

class ClassWithDestructor
{
public:
    ~ClassWithDestructor() { destructed = true; }
};

int native_method_cxx_unwind_stack_test_do_unwind(duk_context *ctx)
{
    ClassWithDestructor c;
    duk_push_int(ctx, 123);
    duk_throw(ctx);
    return 1;
}

int native_method_cxx_unwind_stack_test(duk_context *ctx)
{
    duk_push_boolean(ctx, destructed);
    return 1;
}


// -----------------------------------------------------------------------------
//  module_main - on stack: [module exports]
// -----------------------------------------------------------------------------

int module_main(duk_context *ctx, const char *module_path)
{
    duk_function_list_entry methods[] = {{"simple_add", native_method_simple_add, 2},
                                         {"new_test", native_method_cxx_new_test, 0},
                                         {"object_heap_test", native_method_cxx_object_heap_test, 0},
                                         {"unwind_stack_test_do_unwind", native_method_cxx_unwind_stack_test_do_unwind, 0},
                                         {"unwind_stack_test", native_method_cxx_unwind_stack_test, 0},
                                         {NULL, NULL, 0}};
    duk_put_function_list(ctx, 1, methods);

    return 0;
}
