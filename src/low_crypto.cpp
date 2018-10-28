// -----------------------------------------------------------------------------
//  low_crypto.cpp
// -----------------------------------------------------------------------------

#include "low_crypto.h"
#include "LowCryptoHash.h"


// -----------------------------------------------------------------------------
//  low_crypto_create_hash
// -----------------------------------------------------------------------------

duk_ret_t low_crypto_create_hash(duk_context *ctx)
{
    low_main_t *low = low_duk_get_low(ctx);

    const char *str = duk_require_string(ctx, 1);
    if(strcmp(str, "sha1") != 0)
        duk_reference_error(
          low->duk_ctx, "currently only sha1 supported, not %s!", str);

    LowCryptoHash *hash = new(low_new) LowCryptoHash(low);

    int index;
    for(index = 0; index < low->cryptoHashes.size(); index++)
        if(!low->cryptoHashes[index])
        {
            low->cryptoHashes[index] = hash;
            break;
        }
    if(index == low->cryptoHashes.size())
        low->cryptoHashes.push_back(hash);
    hash->SetIndex(index);

    duk_push_int(low->duk_ctx, index);
    duk_push_c_function(ctx, low_crypto_hash_finalizer, 1);
    duk_set_finalizer(ctx, 0);

    return 1;
}

// -----------------------------------------------------------------------------
//  low_crypto_hash_finalizer
// -----------------------------------------------------------------------------

duk_ret_t low_crypto_hash_finalizer(duk_context *ctx)
{
    low_main_t *low = low_duk_get_low(ctx);

    int index = duk_require_int(ctx, 0);
    if(index < 0 || index >= low->cryptoHashes.size())
        duk_reference_error(ctx, "crypto hash not found");

    delete low->cryptoHashes[index];
    return 0;
}


// -----------------------------------------------------------------------------
//  low_crypto_hash_update
// -----------------------------------------------------------------------------

duk_ret_t low_crypto_hash_update(duk_context *ctx)
{
    low_main_t *low = low_duk_get_low(ctx);

    int index = duk_require_int(ctx, 0);
    if(index < 0 || index >= low->cryptoHashes.size())
        duk_reference_error(ctx, "crypto hash not found");

    duk_size_t len;
    auto buffer = duk_require_buffer_data(ctx, 1, &len);

    low->cryptoHashes[index]->Update((unsigned char *)buffer, len);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_crypto_hash_digest
// -----------------------------------------------------------------------------

duk_ret_t low_crypto_hash_digest(duk_context *ctx)
{
    low_main_t *low = low_duk_get_low(ctx);

    int index = duk_require_int(ctx, 0);
    if(index < 0 || index >= low->cryptoHashes.size())
        duk_reference_error(ctx, "crypto hash not found");

    duk_size_t len;
    auto buffer = duk_require_buffer_data(ctx, 1, &len);

    low->cryptoHashes[index]->Digest((unsigned char *)buffer, len);
    return 1;
}