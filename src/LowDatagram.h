// -----------------------------------------------------------------------------
//  LowDatagram.h
// -----------------------------------------------------------------------------

#ifndef __LOWDATAGRAM_H__
#define __LOWDATAGRAM_H__

#include "LowFD.h"
#include "LowLoopCallback.h"

#include <arpa/inet.h>

class LowDatagram
    : public LowFD
    , public LowLoopCallback
{
  public:
    LowDatagram(low_main_t *low);
    virtual ~LowDatagram();

    bool Bind(struct sockaddr *addr, int addrLen, int callIndex, int &err,
                const char *&syscall, bool reuseAddr);
    void Send(int bufferIndex, const char *address, int port, int callIndex);

    void Read(int pos, unsigned char *data, int len, int callIndex) {}
    void Write(int pos, unsigned char *data, int len, int callIndex) {}
    bool Close(int callIndex);

  protected:
    virtual bool OnEvents(short events);
    virtual bool OnLoop();

  private:
    low_main_t *mLow;

    int mMessageCallID, mFamily;

    unsigned char *mSendData;
    struct sockaddr_in6 mSendAddr;
    int mSendLen, mSendErr;
    int mSendBufferID, mSendCallID;

    unsigned char mRecvData[1500];
    struct sockaddr_in6 mRecvAddr;
    socklen_t mRecvAddrLen;
    int mRecvLen, mRecvErr;
    bool mHasRecv;
};

#endif /* __LOWDATAGRAM_H__ */