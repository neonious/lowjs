// -----------------------------------------------------------------------------
//  LowTLSContext.cpp
// -----------------------------------------------------------------------------

#include "LowTLSContext.h"

#include <cstring>
#include <cstdio>

#include "mbedtls/debug.h"

// -----------------------------------------------------------------------------
//  LowTLSContext::LowTLSContext
// -----------------------------------------------------------------------------

static void my_debug(void *ctx, int level, const char *file, int line,
                     const char *str)
{
    const char *p, *basename;
    (void)ctx;

    /* Extract basename from file */
    for(p = basename = file; *p != '\0'; p++)
    {
        if(*p == '/' || *p == '\\')
        {
            basename = p + 1;
        }
    }

    printf("%s:%04d: |%d| %s", basename, line, level, str);
}

LowTLSContext::LowTLSContext(low_t *low, const char *cert, int certLen,
                             const char *key, int keyLen, const char *ca,
                             int caLen, bool isServer)
    : mLow(low), mRef(1), mIndex(-1), mIsOK(false), mHasCert(false), mHasCA(false)
{
    int ret;

    if(!cert || !key)
        cert = key = NULL;

    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ssl_config_defaults(
        &conf, isServer ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if(ret != 0)
        return;
/*
        mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);
        mbedtls_debug_set_threshold(4);
*/
    const char *pers = "low.js";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)pers, strlen(pers));
    if(ret != 0)
        return;

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if(cert)
    {
        mbedtls_x509_crt_init(&srvcert);

        mHasCert = true;
        mbedtls_pk_init(&pkey);

        ret = mbedtls_x509_crt_parse(&srvcert, (unsigned char *)cert, certLen);
        if(ret != 0)
            return;

        ret =
            mbedtls_pk_parse_key(&pkey, (unsigned char *)key, keyLen, NULL, 0);
        if(ret != 0)
            return;
    }
    if(ca)
    {
        mbedtls_x509_crt_init(&cacert);

        mHasCA = true;

        ret = mbedtls_x509_crt_parse(&cacert, (unsigned char *)ca, caLen);
        if(ret != 0)
            return;

        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    }
    else
    {
        // We have nothing to verify...
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    if(cert)
    {
        ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
        if(ret != 0)
            return;
    }

    mIsOK = true;
}

// -----------------------------------------------------------------------------
//  LowTLSContext::~LowTLSContext
// -----------------------------------------------------------------------------

LowTLSContext::~LowTLSContext()
{
    if(mIndex >= 0)
        mLow->tlsContexts[mIndex] = NULL;

    if(mHasCA)
        mbedtls_x509_crt_free(&cacert);
    if(mHasCert)
    {
        mbedtls_x509_crt_free(&srvcert);
        mbedtls_pk_free(&pkey);
    }
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

// -----------------------------------------------------------------------------
//  LowTLSContext::AddRef
// -----------------------------------------------------------------------------

void LowTLSContext::AddRef()
{
    pthread_mutex_lock(&mLow->ref_mutex);
    mRef++;
    pthread_mutex_unlock(&mLow->ref_mutex);
}

// -----------------------------------------------------------------------------
//  LowTLSContext::DecRef
// -----------------------------------------------------------------------------

void LowTLSContext::DecRef()
{
    pthread_mutex_lock(&mLow->ref_mutex);
    if(!--mRef)
    {
        pthread_mutex_unlock(&mLow->ref_mutex);
        delete this;
        return;
    }
    pthread_mutex_unlock(&mLow->ref_mutex);
}