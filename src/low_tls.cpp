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
    low_t *low = duk_get_low_context(ctx);
    bool isServer = duk_require_boolean(ctx, 1);

    char *my_cert = NULL, *my_key = NULL, *my_ca = NULL;
    duk_size_t cert_len, key_len, ca_len;
    bool malloc_cert = false, malloc_key = false, malloc_ca = false;

    duk_get_prop_string(ctx, 0, "cert");
    if(duk_is_string(ctx, -1))
    {
        my_cert = (char *)duk_get_string(ctx, -1);
        cert_len = duk_get_length(ctx, -1) + 1;
    }
    else
    {
        unsigned char *cert = (unsigned char *)duk_get_buffer_data(ctx, -1, &cert_len);
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
            malloc_cert = true;
        }
    }
    duk_get_prop_string(ctx, 0, "key");
    if(duk_is_string(ctx, -1))
    {
        my_key = (char *)duk_get_string(ctx, -1);
        key_len = duk_get_length(ctx, -1) + 1;
    }
    else
    {
        unsigned char *key = (unsigned char *)duk_get_buffer_data(ctx, -1, &key_len);
        if(key)
        {
            my_key = (char *)low_alloc(key_len + 1);
            if(!my_key)
            {
                if(malloc_cert)
                    low_free(my_cert);
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            memcpy(my_key, key, key_len);
            my_key[key_len++] = '\0';
            malloc_key = true;
        }
    }
    duk_get_prop_string(ctx, 0, "ca");
    if(duk_is_array(ctx, -1))
    {
        int i;
        bool firstIsString = false;

        ca_len = 0;
        for(i = 0;; i++)
        {
            if(!duk_get_prop_index(ctx, -1, i))
            {
                duk_pop(ctx);
                break;
            }
            ca_len += duk_get_length(ctx, -1) + 1;
            if(i == 0)
                firstIsString = duk_is_string(ctx, -1);
            duk_pop(ctx);
        }
        ca_len++;

        if(i == 1 && firstIsString)
        {
            // Fast path for rootCertificates or other arrays with 1 string
            duk_get_prop_index(ctx, -1, 0);
            my_ca = (char *)duk_get_string(ctx, -1);
            ca_len = duk_get_length(ctx, -1) + 1;
            duk_pop(ctx);
        }
        else if(i > 1)
        {
            malloc_ca = true;
            my_ca = (char *)low_alloc(ca_len);
            if(!my_ca)
            {
                if(malloc_cert)
                    low_free(my_cert);
                if(malloc_key)
                    low_free(my_key);
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            char *my_ca_ptr = my_ca;

            for(int i = 0;; i++)
            {
                if(!duk_get_prop_index(ctx, -1, i))
                {
                    duk_pop(ctx);
                    break;
                }

                int len = duk_get_length(ctx, -1);
                if(duk_is_string(ctx, -1))
                    memcpy(my_ca_ptr, duk_get_string(ctx, -1), len);
                else
                {
                    unsigned char *ca = (unsigned char *)duk_get_buffer_data(ctx, -1, NULL);
                    memcpy(my_ca_ptr, ca, len);
                }
                my_ca_ptr += len;
                if(len && my_ca_ptr[-1] != '\n')
                    *my_ca_ptr++ = '\n';
                duk_pop(ctx);
            }
            *my_ca_ptr = '\0';
        }
    }
    else if(duk_is_string(ctx, -1))
    {
        my_ca = (char *)duk_get_string(ctx, -1);
        ca_len = duk_get_length(ctx, -1) + 1;
    }
    else
    {
        unsigned char *ca = (unsigned char *)duk_get_buffer_data(ctx, -1, &ca_len);
        if(ca)
        {
            my_ca = (char *)low_alloc(ca_len + 1);
            if(!my_ca)
            {
                if(malloc_cert)
                    low_free(my_cert);
                if(malloc_key)
                    low_free(my_key);
                low_push_error(low, ENOMEM, "malloc");
                duk_throw(ctx);
            }
            memcpy(my_ca, ca, ca_len);
            my_ca[ca_len++] = '\0';
            malloc_ca = true;
        }
    }

    LowTLSContext *context = new(low_new) LowTLSContext(
      low, my_cert, cert_len, my_key, key_len, my_ca, ca_len, isServer);

    if(!context->IsOK())
    {
        delete context;
        duk_generic_error(ctx, "SSL context error CA LEN %d %d", (int)ca_len, (int)strlen((char *)my_ca));
    }
    if(malloc_cert)
        low_free(my_cert);
    if(malloc_key)
        low_free(my_key);
    if(malloc_ca)
        low_free(my_ca);

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
    low_t *low = duk_get_low_context(ctx);

    duk_get_prop_string(ctx, 0, "_index");
    int index = duk_require_int(ctx, -1);
    if(index < 0 || index >= low->tlsContexts.size())
        duk_reference_error(ctx, "tls context not found");

    low->tlsContexts[index]->DecRef();
    return 0;
}