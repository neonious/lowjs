// -----------------------------------------------------------------------------
//  LowCryptoHash.h
// -----------------------------------------------------------------------------

#ifndef __LOWCRYPTOHASH_H__
#define __LOWCRYPTOHASH_H__

#include "low_main.h"

#include "mbedtls/sha1.h"

using namespace std;

class LowCryptoHash
{
  public:
    LowCryptoHash(low_main_t *low);
    ~LowCryptoHash();

    void SetIndex(int index) { mIndex = index; }

    void Update(unsigned char *data, int len);
    void Digest(unsigned char *data, int len);

  private:
    low_main_t *mLow;
    int mIndex;

    mbedtls_sha1_context mContext;
};

#endif /* __LOWCRYPTOHASH_H__ */