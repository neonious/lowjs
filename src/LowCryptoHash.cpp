// -----------------------------------------------------------------------------
//  LowCryptoHash.cpp
// -----------------------------------------------------------------------------

#include "LowCryptoHash.h"

#include "mbedtls/md_internal.h"


// -----------------------------------------------------------------------------
//  LowCryptoHash::LowCryptoHash
// -----------------------------------------------------------------------------

LowCryptoHash::LowCryptoHash(low_main_t *low,
                             const mbedtls_md_info_t *info,
                             unsigned char *key,
                             int key_len) :
    mLow(low),
    mIndex(-1), mHMAC(key ? true : false), mOutputSize(info->size)
{
    mbedtls_md_init(&mContext);

    int res = mbedtls_md_setup(&mContext, info, mHMAC ? 1 : 0);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);

    res = mHMAC ? mbedtls_md_hmac_starts(&mContext, key, key_len)
                : mbedtls_md_starts(&mContext);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}

// -----------------------------------------------------------------------------
//  LowCryptoHash::~LowCryptoHash
// -----------------------------------------------------------------------------

LowCryptoHash::~LowCryptoHash()
{
    mbedtls_md_free(&mContext);

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
    int res = mHMAC ? mbedtls_md_hmac_update(&mContext, data, len)
                    : mbedtls_md_update(&mContext, data, len);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}

// -----------------------------------------------------------------------------
//  LowCryptoHash::Digest
// -----------------------------------------------------------------------------

void LowCryptoHash::Digest(unsigned char *data, int len)
{
    int res = mHMAC ? mbedtls_md_hmac_finish(&mContext, data)
                    : mbedtls_md_finish(&mContext, data);
    if(res != 0)
        duk_generic_error(mLow->duk_ctx, "mbedtls error code #%d", res);
}