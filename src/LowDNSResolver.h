// -----------------------------------------------------------------------------
//  LowDNSResolver.h
// -----------------------------------------------------------------------------

#ifndef __LOWDNSRESOLVER_H__
#define __LOWDNSRESOLVER_H__

#include "LowLoopCallback.h"

#include "../deps/c-ares/ares.h"

struct low_t;

class LowDNSResolver
{
    friend class LowDNSResolver_Query;
    friend class LowDNSResolver_GetHostByAddr;

  public:
    LowDNSResolver(low_t *low);
    virtual ~LowDNSResolver();

    bool Init();
    void Cancel();

    struct ares_addr_port_node *GetServers(int &err);
    int SetServers(struct ares_addr_port_node *list);

    ares_channel &Channel() { return mChannel; }
    bool IsActive() { return mActiveQueries != 0; }

    static int AresErr(int err);

  private:
    low_t *mLow;

    ares_channel mChannel;
    int mIndex, mActiveQueries;
};

class LowDNSResolver_Query : public LowLoopCallback
{
  public:
    LowDNSResolver_Query(LowDNSResolver *resolver);
    virtual ~LowDNSResolver_Query();

    void Resolve(const char *hostname, const char *type, bool ttl, int refIndex, int callIndex);

  protected:
    virtual bool OnLoop();

  private:
    void Callback(int status, int timeouts, unsigned char *abuf, int alen);
    static void CallbackStatic(void *arg, int status, int timeouts, unsigned char *abuf, int alen)
    {
        ((LowDNSResolver_Query *)arg)->Callback(status, timeouts, abuf, alen);
    }

  private:
    LowDNSResolver *mResolver;
    low_t *mLow;
    ares_channel mChannel;

    bool mTTL;
    int mDNSType;

    int mRefID, mCallID, mError;
    const char *mSyscall;

    unsigned char *mData;
    int mLen;

    void *mDataFree, *mAresFree;
    struct hostent *mHostFree;
};

class LowDNSResolver_GetHostByAddr : public LowLoopCallback
{
  public:
    LowDNSResolver_GetHostByAddr(LowDNSResolver *resolver);
    virtual ~LowDNSResolver_GetHostByAddr();

    int Resolve(const char *hostname, int refIndex, int callIndex);

  protected:
    virtual bool OnLoop();

  private:
    void Callback(int status, int timeouts, struct hostent *hostent);
    static void CallbackStatic(void *arg, int status, int timeouts, struct hostent *hostent)
    {
        ((LowDNSResolver_GetHostByAddr *)arg)->Callback(status, timeouts, hostent);
    }

  private:
    LowDNSResolver *mResolver;
    low_t *mLow;
    ares_channel mChannel;

    int mRefID, mCallID, mError;
    const char *mSyscall;

    char **mResult;
};

#endif /* __LOWDNSRESOLVER_H__ */
