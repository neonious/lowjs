// -----------------------------------------------------------------------------
//  LowDNSWorker.h
// -----------------------------------------------------------------------------

#ifndef __LOWDNSWORKER_H__
#define __LOWDNSWORKER_H__

#include "LowLoopCallback.h"
#include "LowDataCallback.h"

#include <arpa/inet.h>

struct low_t;

class LowDNSWorker : public LowLoopCallback, public LowDataCallback
{
public:
    LowDNSWorker(low_t *low);
    virtual ~LowDNSWorker();

    bool Lookup(const char *host, int family, int hints, int callIndex);
    bool LookupService(const char *ip, int port, int callIndex);

protected:
    virtual bool OnLoop();
    virtual bool OnData();

private:
    low_t *mLow;

    char mIP[INET_ADDRSTRLEN];
    char mHost[64], mService[64];
    int mFamily, mHints, mPort, mCallID;
    bool mLookupService;
    int mError;

    struct addrinfo *mResult;
};

#endif /* __LOWDNSWORKER_H__ */