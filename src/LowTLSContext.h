
// -----------------------------------------------------------------------------
//  LowTLSContext.h
// -----------------------------------------------------------------------------

#ifndef __LOWTLSCONTEXT_H__
#define __LOWTLSCONTEXT_H__

#include "low_main.h"

#include "mbedtls/certs.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509.h"

using namespace std;

class LowTLSContext
{
  public:
    LowTLSContext(low_main_t *low,
                  const char *cert = NULL,
                  int certLen = 0,
                  const char *key = NULL,
                  int keyLen = 0,
                  const char *ca = NULL,
                  int caLen = 0,
                  bool isServer = false);
    ~LowTLSContext();

    bool IsOK() { return mIsOK; }

    void SetIndex(int index) { mIndex = index; }
    void AddRef();
    void DecRef();

    mbedtls_ssl_config &GetSSLConfig() { return conf; }

  private:
    low_main_t *mLow;
    int mRef;
    int mIndex;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;

    bool mIsOK, mHasCert, mHasCA;
};

#endif /* __LOWTLSCONTEXT_H__ */