
// -----------------------------------------------------------------------------
//  low_native_aux.cpp
// -----------------------------------------------------------------------------

#include "low_native_aux.h"

#include "low_config.h"
#include "low_main.h"

#if LOW_ESP32_LWIP_SPECIALITIES
#include "lwip/sockets.h"
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#include <arpa/inet.h>
#include <cstring>

// -----------------------------------------------------------------------------
//  low_compare
// -----------------------------------------------------------------------------

duk_ret_t low_compare(duk_context *ctx)
{
    duk_size_t a_len, b_len;
    unsigned char *a = (unsigned char *)duk_require_buffer_data(ctx, 0, &a_len);
    unsigned char *b = (unsigned char *)duk_require_buffer_data(ctx, 1, &b_len);

    if(a_len < b_len)
        duk_push_int(ctx, -1);
    else if(a_len > b_len)
        duk_push_int(ctx, 1);
    else
        duk_push_int(ctx, memcmp(a, b, a_len));
    return 1;
}

// -----------------------------------------------------------------------------
//  low_is_ip
// -----------------------------------------------------------------------------

duk_ret_t low_is_ip(duk_context *ctx)
{
    const char *ip = duk_require_string(ctx, 0);

    bool isIPv4 = false;
    for(int i = 0; ip[i]; i++)
    {
        if(ip[i] == '.')
        {
            isIPv4 = true;
            break;
        }
    }

    unsigned char buf[16];
    if(inet_pton(isIPv4 ? AF_INET : AF_INET6, ip, buf) != 1)
    {
        duk_push_int(ctx, 0);
        return 1;
    }

    duk_push_int(ctx, isIPv4 ? 4 : 6);
    return 1;
}

// -----------------------------------------------------------------------------
//  low_compile
// -----------------------------------------------------------------------------

duk_ret_t low_compile(duk_context *ctx)
{
    // If we compile here, Duktape uses global object from compilation
    // So all we can do is save it
    duk_push_object(ctx);
    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "code");

    return 1;
}

// -----------------------------------------------------------------------------
//  low_run_in_context
// -----------------------------------------------------------------------------

duk_ret_t low_run_in_context_safe(duk_context *ctx, void *udata)
{
    // TODO: cache compilation
    duk_get_prop_string(ctx, 0, "code");
    duk_get_prop_string(ctx, 0, "fileName");
    duk_compile(ctx, 0);

    duk_call(ctx, 0);
    return 1;
}

duk_ret_t low_run_in_context(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_global_object(low->duk_ctx); // 4

    duk_dup(low->duk_ctx, 1);
    duk_set_global_object(low->duk_ctx);
    duk_dup(low->duk_ctx, 0);

    if(duk_safe_call(low->duk_ctx, low_run_in_context_safe, NULL, 1, 1) !=
       DUK_EXEC_SUCCESS)
    {
        duk_dup(low->duk_ctx, 4);
        duk_set_global_object(low->duk_ctx);

        duk_throw(ctx);
        return 0;
    }

    duk_dup(low->duk_ctx, 4);
    duk_set_global_object(low->duk_ctx);

    return 1;
}
