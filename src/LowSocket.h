// -----------------------------------------------------------------------------
//  LowSocket.h
// -----------------------------------------------------------------------------

#ifndef __LOWSOCKET_H__
#define __LOWSOCKET_H__

#include "LowFD.h"
#include "LowLoopCallback.h"

#include <arpa/inet.h>
#include <pthread.h>

#include "mbedtls/ssl.h"

enum LowSocketType
{
    LOWSOCKET_TYPE_STDINOUT,
    LOWSOCKET_TYPE_ACCEPTED,
    LOWSOCKET_TYPE_CONNECTED
};

const int LOWSOCKET_TRIGGER_READ = 1;
const int LOWSOCKET_TRIGGER_WRITE = 2;

struct low_main_t;

class LowSocketDirect;
class LowSocket
    : public LowFD
    , public LowLoopCallback
{
  public:
    LowSocket(low_main_t *low, int fd); // LOWSOCKET_TYPE_STDINOUT
    LowSocket(low_main_t *low,
              int fd,
              struct sockaddr *remoteAddr,
              int acceptCallID,
              LowSocketDirect *direct,
              int directType,
              LowTLSContext *tlsContext,
              bool clearOnReset = true); // LOWSOCKET_TYPE_ACCEPTED
    LowSocket(low_main_t *low,
              LowSocketDirect *direct,
              int directType,
              LowTLSContext *tlsContext,
              bool clearOnReset = true); // LOWSOCKET_TYPE_CONNECTED
    virtual ~LowSocket();

    bool Connect(struct sockaddr *remoteAddr,
                 int remoteAddrLen,
                 int callIndex,
                 int &err,
                 const char *&syscall);

    void Read(int pos, unsigned char *data, int len, int callIndex);
    void Write(int pos, unsigned char *data, int len, int callIndex);
    void Shutdown(int callIndex); // JS version
    int Shutdown();
    bool Close(int callIndex = -1);

    void KeepAlive(bool enable, int secs);
    void NoDelay(bool enable);

    bool SetDirect(LowSocketDirect *direct,
                   int type,
                   bool fromWebThread = false);
    LowSocketDirect *GetDirect(int &type);
    void TriggerDirect(int trigger);

    // for direct
    int write(const unsigned char *data, int len);
    int writev(const struct iovec *iov, int iovcnt);

    void SetError(bool write, int error, bool ssl);
    void PushError(int call);

    bool IsConnected() { return mConnected; }

  protected:
    virtual bool OnEvents(short events);
    virtual bool OnLoop();

    bool InitSocket(struct sockaddr *remoteAddr);
    bool CallAcceptConnect(int callIndex, bool onStash);

    int DoRead();
    int DoWrite();

  private:
    low_main_t *mLow;
    LowSocketType mType;
    short mLastEvents;

    int mNodeFamily;
    char mRemoteHost[INET6_ADDRSTRLEN];
    int mRemotePort;

    int mAcceptConnectCallID, mAcceptConnectErrno, mCloseCallID;
    bool mAcceptConnectError, mAcceptConnectErrnoSSL, mConnected, mClosed,
      mDestroyed;
    const char *mAcceptConnectSyscall;

    unsigned char *mReadData, *mWriteData;
    int mReadLen, mReadCallID, mReadPos, mReadErrno;
    int mWriteLen, mWriteCallID, mWritePos, mWriteErrno;
    bool mReadErrnoSSL, mWriteErrnoSSL;

    LowSocketDirect *mDirect;
    int mDirectType;
    bool mDirectReadEnabled, mDirectWriteEnabled;

    LowTLSContext *mTLSContext;
    mbedtls_ssl_context *mSSL;
    bool mSSLWantRead, mSSLWantWrite;
};

#endif /* __LOWSOCKET_H__ */