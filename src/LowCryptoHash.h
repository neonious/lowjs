// -----------------------------------------------------------------------------
//  LowCryptoHash.h
// -----------------------------------------------------------------------------

#ifndef __LOWCRYPTOHASH_H__
#define __LOWCRYPTOHASH_H__

#include "low_main.h"

#include "mbedtls/md.h"

using namespace std;

class LowCryptoHash
{
  public:
    LowCryptoHash(low_t *low,
                  const mbedtls_md_info_t *info,
                  unsigned char *key,
                  int key_len);
    ~LowCryptoHash();

    void SetIndex(int index) { mIndex = index; }

    void Update(unsigned char *data, int len);
    void Digest(unsigned char *data, int len);

    int OutputSize() { return mOutputSize; }

  private:
    low_t *mLow;
    int mIndex;

    mbedtls_md_context_t mContext;

    bool mHMAC;
    int mOutputSize;
};

#endif /* __LOWCRYPTOHASH_H__ */