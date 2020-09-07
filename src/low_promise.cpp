// -----------------------------------------------------------------------------
//  low_promise.cpp
// -----------------------------------------------------------------------------

#include "low_promise.h"

#include "low_system.h"


// -----------------------------------------------------------------------------
//  low_register_promise
// -----------------------------------------------------------------------------

bool low_register_promise(low_t *low)
{
    duk_context *ctx = low_get_duk_context(low);

    duk_push_global_object(ctx);

    duk_push_c_function(ctx, promise_constructor, 1);

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "Promise"); // this is used in call stack
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);

    duk_function_list_entry methods[] = {{"all", promise_all, 1},
                                         {"race", promise_race, 1},
                                         {"resolve", promise_resolve, 1},
                                         {"reject", promise_reject, 1},
                                         {NULL, NULL, 0}};
    duk_put_function_list(low_get_duk_context(low), -1, methods);

    duk_push_bare_object(ctx);
    duk_function_list_entry methods2[] = {{"catch", promise_catch, 1},
                                         {"then", promise_then, 2},
                                         {NULL, NULL, 0}};
    duk_put_function_list(low_get_duk_context(low), -1, methods2);
    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_prop_string(ctx, -2, "Promise");

    return true;
}


// -----------------------------------------------------------------------------
//  promise_constructor
// -----------------------------------------------------------------------------

int promise_constructor(duk_context *ctx)
{
    duk_push_this(ctx);

    // [ executor this ]
    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");
    duk_remove(ctx, -1);

    duk_push_c_lightfunc(ctx, promise_param_resolve, 1, 1, 0);
    duk_push_string(ctx, "bind");
    duk_push_this(ctx);
    duk_call_prop(ctx, -3, 1);
    duk_remove(ctx, -2);
    // [ executor binded_func ]

    duk_push_c_lightfunc(ctx, promise_param_reject, 1, 1, 0);
    duk_push_string(ctx, "bind");
    duk_push_this(ctx);
    duk_call_prop(ctx, -3, 1);
    duk_remove(ctx, -2);
    // [ executor binded_func binded_func ]

    int rc = duk_pcall(ctx, 2);
    if (rc != DUK_EXEC_SUCCESS)
        promise_param_reject(ctx);

    return 0;
}


// -----------------------------------------------------------------------------
//  promise_param_resolve
// -----------------------------------------------------------------------------

int promise_param_resolve(duk_context *ctx)
{
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "_promiseStatus");

    if(duk_require_int(ctx, -1) == 0)
    {
        bool isPromise = false;
        if(duk_is_object(ctx, 0))
        {
            if(duk_get_prop_string(ctx, 0, "_promiseStatus"))
                isPromise = true;
            else
                duk_pop(ctx);
        }
        if(isPromise)
        {
            // [ promise this thisStatus promiseStatus ]
            int subStatus = duk_require_int(ctx, -1);
            if(subStatus)
            {
                duk_put_prop_string(ctx, -3, "_promiseStatus");
                duk_get_prop_string(ctx, 0, "_value");
                duk_put_prop_string(ctx, -3, "_value");

                if(subStatus == 2)
                {
                    duk_push_boolean(ctx, true);
                    duk_put_prop_string(ctx, 0, "_warnedUnhandled");
                }
            }
            else
            {
                // Remove _then or _catch and let this promise get fulfilled by the new promise
                duk_del_prop_string(ctx, 1, "_then");
                duk_del_prop_string(ctx, 1, "_catch");

                duk_get_prop_string(ctx, 0, "_chain");
                duk_dup(ctx, 1);
                duk_put_prop_index(ctx, -2, duk_get_length(ctx, -2));

                return 0;
            }
        }
        else
        {
            duk_push_int(ctx, 1);
            // [ val this thisStatus status ]
            duk_put_prop_string(ctx, -3, "_promiseStatus");
            duk_dup(ctx, 0);
            duk_put_prop_string(ctx, -3, "_value");
        }

        duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
        duk_push_this(ctx);
        low_call_next_tick(ctx, 1);
    }
    
    return 0;
}


// -----------------------------------------------------------------------------
//  promise_param_reject
// -----------------------------------------------------------------------------

int promise_param_reject(duk_context *ctx)
{
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "_promiseStatus");

    if(duk_require_int(ctx, -1) == 0)
    {
        duk_push_int(ctx, 2);
        duk_put_prop_string(ctx, -3, "_promiseStatus");
        duk_dup(ctx, 0);
        duk_put_prop_string(ctx, -3, "_value");

        duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
        duk_push_this(ctx);
        low_call_next_tick(ctx, 1);
    }

    return 0;
}


// -----------------------------------------------------------------------------
//  promise_handle_thens
// -----------------------------------------------------------------------------

int promise_handle_thens(duk_context *ctx)
{
    // [ ... promise ]
    duk_get_prop_string(ctx, -1, "_promiseStatus");
    int status = duk_require_int(ctx, -1);
    duk_pop(ctx);

    duk_get_prop_string(ctx, -1, "_value");
    duk_get_prop_string(ctx, -2, "_chain");
    // [ ... promise value chain ]

    // Replace _chain
    duk_push_bare_array(ctx);
    // [ ... promise value chain array ]
    duk_put_prop_string(ctx, -4, "_chain");

    int count = duk_get_length(ctx, -1);
    // [ ... promise value chain ]
    for(int i = 0; i < count; i++)
    {
        duk_get_prop_index(ctx, -1, i);
        duk_get_prop_string(ctx, -1, status == 1 ? "_then" : "_catch");
        // [ promise value chain sub_promise func ]

        if(!duk_is_undefined(ctx, -1))
        {
            duk_dup(ctx, -4);
            int rc = duk_pcall(ctx, 1);

            // [ promise value chain sub_promise result ]

            bool isPromise = false;
            if(rc == DUK_EXEC_SUCCESS && duk_is_object(ctx, -1))
            {
                if(duk_get_prop_string(ctx, -1, "_promiseStatus"))
                    isPromise = true;
                else
                    duk_pop(ctx);
            }
            if(isPromise)
            {
                // [ promise value chain sub_promise sub_promise_2 _promiseStatus ]
                int subStatus = duk_require_int(ctx, -1);
                if(subStatus)
                {
                    duk_put_prop_string(ctx, -3, "_promiseStatus");
                    duk_get_prop_string(ctx, -1, "_value");
                    duk_put_prop_string(ctx, -3, "_value");

                    if(subStatus == 2)
                    {
                        duk_push_boolean(ctx, true);
                        duk_put_prop_string(ctx, -2, "_warnedUnhandled");
                    }

                    duk_pop(ctx);
                }
                else
                {
                    // Remove _then or _catch and let this promise get fulfilled by the new promise
                    duk_del_prop_string(ctx, -3, "_then");
                    duk_del_prop_string(ctx, -3, "_catch");

                    duk_get_prop_string(ctx, -2, "_chain");
                    // [ promise value chain sub_promise sub_promise_2 _promiseStatus _chain ]
                    duk_dup(ctx, -4);
                    duk_put_prop_index(ctx, -2, duk_get_length(ctx, -2));

                    // [ promise value chain sub_promise sub_promise_2 _promiseStatus sub_promise_2_chain ]
                    duk_pop_n(ctx, 4);
                    continue;
                }
            }
            else
            {
                // [ promise value chain sub_promise value ]
                duk_put_prop_string(ctx, -2, "_value");
                duk_push_int(ctx, rc == DUK_EXEC_SUCCESS ? 1 : 2);
                duk_put_prop_string(ctx, -2, "_promiseStatus");
            }
        }
        else
        {
            duk_pop(ctx);

            // [ promise value chain sub_promise ]
            duk_dup(ctx, -3);
            duk_put_prop_string(ctx, -2, "_value");
            duk_push_int(ctx, status);
            duk_put_prop_string(ctx, -2, "_promiseStatus");
        }

        promise_handle_thens(ctx);
        duk_pop(ctx);
    }
    duk_pop(ctx);
    // [ ... promise value ]
    if(count == 0 && status == 2)
    {
        if(!duk_get_prop_string(ctx, -2, "_warnedUnhandled"))
        {
            duk_pop(ctx);

            duk_push_boolean(ctx, true);
            duk_put_prop_string(ctx, -3, "_warnedUnhandled");

            low_t *low = duk_get_low_context(ctx);
            if(!low->duk_flag_stop)
            {
                // Unhandled!
                low_error("Unhandled promise rejection!");
                duk_throw(ctx);
            }
        }
        else
            duk_pop(ctx);
    }
    else if(status == 2)
    {
        duk_push_boolean(ctx, true);
        duk_put_prop_string(ctx, -3, "_warnedUnhandled");
    }
    duk_pop(ctx);

    return 0;
}


// -----------------------------------------------------------------------------
//  promise_all
// -----------------------------------------------------------------------------

int promise_all(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Promise");
    duk_get_prop_string(ctx, -1, "prototype");

    duk_push_bare_object(ctx);
    duk_dup(ctx, -2);
    duk_set_prototype(ctx, -2);

    duk_push_int(ctx, 1);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");
    duk_push_bare_array(ctx);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "_value");

    // [ array_promises global Promise prototype myPromise result ]
    int count = duk_get_length(ctx, 0);
    int left = 0;
    for(int i = 0; i < count; i++)
    {
        duk_get_prop_index(ctx, 0, i);

        if(!duk_get_prop_string(ctx, -1, "_promiseStatus"))
        {
            duk_pop(ctx);
            duk_put_prop_index(ctx, 5, i);
            continue;
        }

        int status = duk_require_int(ctx, -1);
        if(status == 1)
        {
            // [ array_promises global Promise prototype myPromise result array_promise status ]
            duk_get_prop_string(ctx, -2, "_value");
            // [ array_promises global Promise prototype myPromise result array_promise status value ]
            duk_put_prop_index(ctx, 5, i);
            duk_pop_2(ctx);
            continue;
        }
        else if(status == 2)
        {
            // [ array_promises global Promise prototype myPromise result array_promise status ]

            // That's it, copy!
            duk_put_prop_string(ctx, 4, "_promiseStatus");
            // [ array_promises global Promise prototype myPromise result array_promise ]
            duk_get_prop_string(ctx, -1, "_value");
            duk_put_prop_string(ctx, 4, "_value");

            duk_push_boolean(ctx, true);
            duk_put_prop_string(ctx, -2, "_warnedUnhandled");

            duk_pop_2(ctx);
            // [ array_promises global Promise prototype myPromise result ]

            duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
            duk_dup(ctx, -2);
            low_call_next_tick(ctx, 1);

            return 1;
        }
        duk_pop(ctx);
        // [ array_promises global Promise prototype myPromise result array_promise ]

        left++;

        // Add sub promise
        duk_push_bare_object(ctx);
        // [ array_promises global Promise prototype myPromise result array_promise subPromise ]

        duk_push_int(ctx, 0);
        duk_put_prop_string(ctx, -2, "_promiseStatus");
        duk_push_bare_array(ctx);
        duk_put_prop_string(ctx, -2, "_chain");
        duk_dup(ctx, 4);
        duk_put_prop_string(ctx, -2, "_promise");
        duk_push_int(ctx, i);
        duk_put_prop_string(ctx, -2, "_index");

        duk_push_c_lightfunc(ctx, promise_all_resolved, 1, 1, 0);
        duk_push_string(ctx, "bind");
        duk_dup(ctx, -3);
        duk_call_prop(ctx, -3, 1);
        // [ object func bind_func ]
        duk_remove(ctx, -2);
        duk_put_prop_string(ctx, -2, "_then");

        duk_push_c_lightfunc(ctx, promise_all_rejected, 1, 1, 0);
        duk_push_string(ctx, "bind");
        duk_dup(ctx, -3);
        duk_call_prop(ctx, -3, 1);
        // [ object func bind_func ]
        duk_remove(ctx, -2);
        duk_put_prop_string(ctx, -2, "_catch");

        // [ array_promises global Promise prototype myPromise result array_promise subPromise ]

        duk_get_prop_string(ctx, -2, "_chain");
        duk_dup(ctx, -2);
        duk_put_prop_index(ctx, -2, duk_get_length(ctx, -2));

        // [ array_promises global Promise prototype myPromise result array_promise subPromise arrayPromiseChain ]
        duk_pop_3(ctx);
    }

    duk_pop(ctx);
    if(left)
    {
        duk_push_int(ctx, left);
        duk_put_prop_string(ctx, 4, "_left");
    }
    duk_push_int(ctx, left == 0 ? 1 : 0);
    duk_put_prop_string(ctx, 4, "_promiseStatus");

    return 1;
}


// -----------------------------------------------------------------------------
//  promise_all_resolved
// -----------------------------------------------------------------------------

int promise_all_resolved(duk_context *ctx)
{
    duk_push_this(ctx);
    duk_get_prop_string(ctx, 1, "_promise");

    // [ value this promise ]

    duk_get_prop_string(ctx, 1, "_index");
    // [ value this promise index ]
    int index = duk_require_int(ctx, 3);

    duk_get_prop_string(ctx, 2, "_left");
    // [ value this promise index left ]
    int left = duk_require_int(ctx, 4);

    duk_get_prop_string(ctx, 2, "_value");
    duk_dup(ctx, 0);
    duk_put_prop_index(ctx, 5, index);

    // [ value this promise index left result ]

    if(--left == 0)
    {
        duk_pop_n(ctx, 3);

        duk_push_int(ctx, 1);
        duk_put_prop_string(ctx, 2, "_promiseStatus");

        duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
        duk_dup(ctx, -2);
        low_call_next_tick(ctx, 1);
    }
    else
    {
        duk_push_int(ctx, left);
        duk_put_prop_string(ctx, 2, "_left");
    }

    return 0;
}


// -----------------------------------------------------------------------------
//  promise_all_rejected
// -----------------------------------------------------------------------------

int promise_all_rejected(duk_context *ctx)
{
    duk_push_this(ctx);
    duk_get_prop_string(ctx, 1, "_promise");

    // [ value this promise ]

    duk_push_int(ctx, 2);
    duk_put_prop_string(ctx, 2, "_promiseStatus");
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, 2, "_value");

    duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
    duk_dup(ctx, -2);
    low_call_next_tick(ctx, 1);

    return 0;
}


// -----------------------------------------------------------------------------
//  promise_race
// -----------------------------------------------------------------------------

int promise_race(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Promise");
    duk_get_prop_string(ctx, -1, "prototype");

    duk_push_bare_object(ctx);
    duk_dup(ctx, -2);
    duk_set_prototype(ctx, -2);

    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");

    // [ array_promises global Promise prototype myPromise ]
    int count = duk_get_length(ctx, 0);
    for(int i = 0; i < count; i++)
    {
        duk_get_prop_index(ctx, 0, i);
        if(!duk_get_prop_string(ctx, -1, "_promiseStatus"))
        {
            // That's it, copy!
            duk_dup(ctx, -2);
            duk_put_prop_string(ctx, 4, "_value");
            duk_push_int(ctx, 1);
            duk_put_prop_string(ctx, 4, "_promiseStatus");
            duk_pop_2(ctx);
            return 1;
        }

        int status = duk_require_int(ctx, -1);
        if(status)
        {
            // [ array_promises global Promise prototype myPromise array_promise status ]
                
            // That's it, copy!
            duk_put_prop_string(ctx, 4, "_promiseStatus");
            duk_get_prop_string(ctx, -1, "_value");
            duk_put_prop_string(ctx, 4, "_value");

            if(status == 2)
            {
                duk_push_boolean(ctx, true);
                duk_put_prop_string(ctx, -2, "_warnedUnhandled");
            }

            // [ array_promises global Promise prototype myPromise array_promise ]
            duk_pop(ctx);

            if(status == 2)
            {
                duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
                duk_dup(ctx, -2);
                low_call_next_tick(ctx, 1);
            }
            return 1;
        }

        duk_get_prop_string(ctx, -2, "_chain");
        duk_dup(ctx, 4);
        duk_put_prop_index(ctx, -2, duk_get_length(ctx, -2));
        // [ array_promises global Promise prototype myPromise array_promise status array_promise_chain ]

        duk_pop_3(ctx);
    }

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    return 1;
}


// -----------------------------------------------------------------------------
//  promise_catch
// -----------------------------------------------------------------------------

int promise_catch(duk_context *ctx)
{
    promise_then_catch(ctx, true);
    return 1;
}


// -----------------------------------------------------------------------------
//  promise_then
// -----------------------------------------------------------------------------

int promise_then(duk_context *ctx)
{
    promise_then_catch(ctx, false);
    return 1;
}


// -----------------------------------------------------------------------------
//  promise_resolve
// -----------------------------------------------------------------------------

int promise_resolve(duk_context *ctx)
{
    if(duk_is_object(ctx, 0) && duk_get_prop_string(ctx, 0, "_promiseStatus"))
    {
        // If we get a promise, return it
        duk_pop(ctx);
        return 1;
    }

    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Promise");
    duk_get_prop_string(ctx, -1, "prototype");

    duk_push_bare_object(ctx);
    duk_dup(ctx, -2);
    duk_set_prototype(ctx, -2);

    duk_push_int(ctx, 1);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "_value");

    return 1;
}


// -----------------------------------------------------------------------------
//  promise_reject
// -----------------------------------------------------------------------------

int promise_reject(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Promise");
    duk_get_prop_string(ctx, -1, "prototype");

    duk_push_bare_object(ctx);
    duk_dup(ctx, -2);
    duk_set_prototype(ctx, -2);

    duk_push_int(ctx, 2);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "_value");

    duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
    duk_dup(ctx, -2);
    low_call_next_tick(ctx, 1);

    return 1;
}


// -----------------------------------------------------------------------------
//  promise_then_catch
// -----------------------------------------------------------------------------

void promise_then_catch(duk_context *ctx, bool isCatch)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "Promise");
    duk_get_prop_string(ctx, -1, "prototype");

    // [ then/catch global Promise prototype promise ]
    duk_push_bare_object(ctx);
    duk_dup(ctx, -2);
    duk_set_prototype(ctx, -2);

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "_promiseStatus");
    duk_push_bare_array(ctx);
    duk_put_prop_string(ctx, -2, "_chain");

    if(isCatch)
    {
        duk_dup(ctx, 0);
        duk_put_prop_string(ctx, -2, "_catch");
    }
    else
    {
        duk_dup(ctx, 0);
        duk_put_prop_string(ctx, -2, "_then");
        duk_dup(ctx, 1);
        duk_put_prop_string(ctx, -2, "_catch");
    }

    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, "_chain");

    // [ then/catch global Promise prototype promise thisPromise thisPromiseChain ]
    duk_dup(ctx, -3);
    duk_put_prop_index(ctx, -2, duk_get_length(ctx, -2));

    // [ then/catch global Promise prototype promise thisPromise thisPromiseChain ]
    duk_get_prop_string(ctx, -2, "_promiseStatus");
    int status = duk_require_int(ctx, -1);

    if(status)
    {
        duk_push_c_lightfunc(ctx, promise_handle_thens, 1, 1, 0);
        duk_push_this(ctx);
        low_call_next_tick(ctx, 1);
    }
    duk_pop_3(ctx);
}
