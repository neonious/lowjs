// -----------------------------------------------------------------------------
//  low_crypto.h
// -----------------------------------------------------------------------------

#ifndef __LOW_CRYPTO_H__
#define __LOW_CRYPTO_H__

#include "duktape.h"

duk_ret_t low_crypto_create_hash(duk_context *ctx);

duk_ret_t low_crypto_hash_finalizer(duk_context *ctx);

duk_ret_t low_crypto_hash_update(duk_context *ctx);

duk_ret_t low_crypto_hash_digest(duk_context *ctx);

#endif /* __LOW_CRYPTO_H__ */