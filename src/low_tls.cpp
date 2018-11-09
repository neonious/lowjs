// -----------------------------------------------------------------------------
//  low_tls.cpp
// -----------------------------------------------------------------------------

#include "low_tls.h"
#include "LowTLSContext.h"

#include "low_alloc.h"
#include "low_main.h"
#include "low_system.h"

#include <errno.h>

// -----------------------------------------------------------------------------
//  low_tls_create_context
// -----------------------------------------------------------------------------

duk_ret_t low_tls_create_context(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);

    unsigned char *cert = NULL, *key = NULL, *ca = NULL;
    char *my_cert = NULL, *my_key = NULL, *my_ca = NULL;
    duk_size_t cert_len, key_len, ca_len;

    duk_get_prop_string(ctx, 0, "cert");
    if(duk_is_string(ctx, -1))
    {
        my_cert = low_strdup(duk_get_string(ctx, -1));
        if(!my_cert)
        {
            low_push_error(low, ENOMEM, "malloc");
            duk_throw(ctx);
        }
        cert_len = strlen(my_cert) + 1;
    }
    else
    {
        cert = (unsigned char *)duk_get_buffer_data(ctx, -1, &cert_len);
        if(cert)
        {
            my_cert = (char *)low_alloc(cert_len + 1);
            if(!my_cert)
            {
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            memcpy(my_cert, cert, cert_len);
            my_cert[cert_len++] = '\0';
        }
    }
    duk_get_prop_string(ctx, 0, "key");
    if(duk_is_string(ctx, -1))
    {
        my_key = low_strdup(duk_get_string(ctx, -1));
        if(!my_key)
        {
            low_free(my_cert);
            low_push_error(low, ENOMEM, "malloc");
            duk_throw(ctx);
        }
        key_len = strlen(my_key) + 1;
    }
    else
    {
        key = (unsigned char *)duk_get_buffer_data(ctx, -1, &key_len);
        if(key)
        {
            my_key = (char *)low_alloc(key_len + 1);
            if(!my_key)
            {
                low_free(my_cert);
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            memcpy(my_key, key, key_len);
            my_key[key_len++] = '\0';
        }
    }
    duk_get_prop_string(ctx, 0, "ca");
    ca = (unsigned char *)duk_get_buffer_data(ctx, -1, &ca_len);
    if(duk_is_string(ctx, -1))
    {
        my_ca = low_strdup(duk_get_string(ctx, -1));
        if(!my_ca)
        {
            low_free(my_cert);
            low_free(my_key);
            low_push_error(low, ENOMEM, "malloc");
            duk_throw(ctx);
        }
        ca_len = strlen(my_ca) + 1;
    }
    else
    {
        ca = (unsigned char *)duk_get_buffer_data(ctx, -1, &ca_len);
        if(ca)
        {
            my_ca = (char *)low_alloc(ca_len + 1);
            if(!my_ca)
            {
                low_free(my_cert);
                low_free(my_key);
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            memcpy(my_ca, ca, ca_len);
            my_ca[ca_len++] = '\0';
        }
    }

    LowTLSContext *context = new(low_new) LowTLSContext(
      low, my_cert, cert_len, my_key, key_len, my_ca, ca_len, true);
    low_free(my_cert);
    low_free(my_key);
    low_free(my_ca);

    if(!context->IsOK())
    {
        delete context;
        duk_generic_error(ctx, "SSL context error");
    }

    int index;
    for(index = 0; index < low->tlsContexts.size(); index++)
        if(!low->tlsContexts[index])
        {
            low->tlsContexts[index] = context;
            break;
        }
    if(index == low->tlsContexts.size())
        low->tlsContexts.push_back(context);
    context->SetIndex(index);

    duk_push_object(low->duk_ctx);
    duk_push_int(low->duk_ctx, index);
    duk_put_prop_string(low->duk_ctx, -2, "_index");

    duk_push_c_function(ctx, low_tls_context_finalizer, 1);
    duk_set_finalizer(ctx, -2);

    return 1;
}

// -----------------------------------------------------------------------------
//  low_tls_context_finalizer
// -----------------------------------------------------------------------------

duk_ret_t low_tls_context_finalizer(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);

    duk_get_prop_string(ctx, 0, "_index");
    int index = duk_require_int(ctx, -1);
    if(index < 0 || index >= low->tlsContexts.size())
        duk_reference_error(ctx, "tls context not found");

    low->tlsContexts[index]->DecRef();
    return 0;
}