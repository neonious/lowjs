// -----------------------------------------------------------------------------
//  LowCryptoHash.cpp
// -----------------------------------------------------------------------------

#include "LowCryptoHash.h"


// -----------------------------------------------------------------------------
//  LowCryptoHash::LowCryptoHash
// -----------------------------------------------------------------------------

LowCryptoHash::LowCryptoHash(low_main_t *low) : mLow(low), mIndex(-1)
{
    mbedtls_sha1_init(&mContext);

    int res = mbedtls_sha1_starts_ret(&mContext);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}

// -----------------------------------------------------------------------------
//  LowCryptoHash::~LowCryptoHash
// -----------------------------------------------------------------------------

LowCryptoHash::~LowCryptoHash()
{
    mbedtls_sha1_free(&mContext);

    if(mIndex >= 0)
    {
        if(mIndex >= mLow->cryptoHashes.size() ||
           mLow->cryptoHashes[mIndex] != this)
            printf("assertion error at LowCryptoHash\n");

        mLow->cryptoHashes[mIndex] = NULL;
    }
}

// -----------------------------------------------------------------------------
//  LowCryptoHash::Update
// -----------------------------------------------------------------------------

void LowCryptoHash::Update(unsigned char *data, int len)
{
    int res = mbedtls_sha1_update_ret(&mContext, data, len);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}

// -----------------------------------------------------------------------------
//  LowCryptoHash::Digest
// -----------------------------------------------------------------------------

void LowCryptoHash::Digest(unsigned char *data, int len)
{
    int res = mbedtls_sha1_finish_ret(&mContext, data);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}