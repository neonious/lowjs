// -----------------------------------------------------------------------------
//  LowServerSocket.h
// -----------------------------------------------------------------------------

#ifndef __LOWSERVERSOCKET_H__
#define __LOWSERVERSOCKET_H__

#include "LowFD.h"

struct low_t;

class LowServerSocket : public LowFD
{
  public:
    LowServerSocket(low_t *low, bool isHTTP, LowTLSContext *secureContext);
    virtual ~LowServerSocket();

    bool Listen(struct sockaddr *addr, int addrLen, int callIndex, int &err,
                const char *&syscall);

    void Read(int pos, unsigned char *data, int len, int callIndex) {}
    void Write(int pos, unsigned char *data, int len, int callIndex) {}
    bool Close(int callIndex);

    void Connections(int count, int max);
    bool WaitForNotTooManyConnections() { return mWaitForNotTooManyConnections; }

  protected:
    virtual bool OnEvents(short events);

  private:
    low_t *mLow;
    bool mIsHTTP;

    int mFamily, mAcceptCallID;

    LowTLSContext *mSecureContext;

    bool mWaitForNotTooManyConnections, mTrackTooManyConnections;
};

#endif /* __LOWSERVERSOCKET_H__ */