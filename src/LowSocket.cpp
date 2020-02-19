// -----------------------------------------------------------------------------
//  LowSocket.cpp
// -----------------------------------------------------------------------------

#include "LowSocket.h"
#include "LowSocketDirect.h"
#include "LowTLSContext.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"
#include "low_system.h"
#include "low_web_thread.h"

#include <errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if LOW_ESP32_LWIP_SPECIALITIES
#include <lwip/sockets.h>
#define ioctl lwip_ioctl
#else
#include <netinet/tcp.h>
#include <sys/uio.h>
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

void add_stats(int index, bool add);


// -----------------------------------------------------------------------------
//  LowSocket::LowSocket
// -----------------------------------------------------------------------------

LowSocket::LowSocket(low_t *low, int fd) :
    LowFD(low, LOWFD_TYPE_SOCKET, fd), LowLoopCallback(low), mLow(low),
    mType(LOWSOCKET_TYPE_STDINOUT),
    mAcceptConnectCallID(0), mCloseCallID(0), mAcceptConnectError(false),
    mConnected(true), mClosed(false), mDestroyed(false),
    mReadData(NULL), mWriteData(NULL), mReadCallID(0),
    mWriteCallID(0),
    mDirect(nullptr),
    mDirectReadEnabled(false), mDirectWriteEnabled(false), mTLSContext(NULL),
    mSSL(NULL), mHost(NULL)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    add_stats(0, true);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    AdvertiseFD();
    InitSocket(NULL);
}

// -----------------------------------------------------------------------------
//  LowSocket::LowSocket
// -----------------------------------------------------------------------------

LowSocket::LowSocket(low_t *low,
                     int fd,
                     struct sockaddr *remoteAddr,
                     int acceptCallID,
                     LowSocketDirect *direct,
                     int directType,
                     LowTLSContext *tlsContext,
                     bool clearOnReset) :
    LowFD(low, LOWFD_TYPE_SOCKET, fd),
    LowLoopCallback(low), mLow(low), mType(LOWSOCKET_TYPE_ACCEPTED),
    mAcceptConnectCallID(acceptCallID), mCloseCallID(0),  mAcceptConnectError(false),
    mConnected(false), mClosed(false),
    mDestroyed(false), mReadData(NULL), mWriteData(NULL), mReadCallID(0), mWriteCallID(0),
    mDirect(direct), mDirectType(directType),
    mDirectReadEnabled(direct != NULL), mDirectWriteEnabled(direct != NULL),
    mTLSContext(tlsContext), mSSL(NULL), mHost(NULL)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    add_stats(0, true);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    mFDClearOnReset = clearOnReset;
    mLoopClearOnReset = clearOnReset;

    if(mDirect)
        mDirect->SetSocket(this);

    if(!InitSocket(remoteAddr))
    {
        if(mAcceptConnectCallID)
            low_loop_set_callback(mLow, this); // to output error
        else
            low_web_set_poll_events(mLow, this, POLLOUT);

        mTLSContext = NULL;
        return;
    }
    else if(mTLSContext)
    {
        low_web_set_poll_events(mLow, this, POLLOUT);
        AdvertiseFD();
    }
    else
    {
        mConnected = true;
        AdvertiseFD();

        if(mAcceptConnectCallID)
            low_loop_set_callback(mLow, this);

        if(mDirect)
            low_web_set_poll_events(mLow, this, POLLIN | POLLOUT);
    }

    if(mTLSContext)
        mTLSContext->AddRef();
}

// -----------------------------------------------------------------------------
//  LowSocket::LowSocket
// -----------------------------------------------------------------------------

LowSocket::LowSocket(low_t *low,
                     LowSocketDirect *direct,
                     int directType,
                     LowTLSContext *tlsContext,
                     char *host,
                     bool clearOnReset) :
    LowFD(low, LOWFD_TYPE_SOCKET),
    LowLoopCallback(low), mLow(low), mType(LOWSOCKET_TYPE_CONNECTED),
    mAcceptConnectCallID(0), mCloseCallID(0), mAcceptConnectError(false),
    mConnected(false), mClosed(false), mDestroyed(false),
    mReadData(NULL), mWriteData(NULL), mReadCallID(0),
    mWriteCallID(0),
    mDirect(direct),
    mDirectType(directType), mDirectReadEnabled(direct != NULL),
    mDirectWriteEnabled(direct != NULL), mTLSContext(tlsContext), mSSL(NULL), mHost(host)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    add_stats(0, true);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    mFDClearOnReset = clearOnReset;
    mLoopClearOnReset = clearOnReset;

    if(mDirect)
        mDirect->SetSocket(this);
    if(mTLSContext)
        mTLSContext->AddRef();
}

// -----------------------------------------------------------------------------
//  LowSocket::~LowSocket
// -----------------------------------------------------------------------------

LowSocket::~LowSocket()
{
#if LOW_ESP32_LWIP_SPECIALITIES
    add_stats(0, false);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    low_web_clear_poll(mLow, this);

    low_free(mHost);
    if(mDirect)
    {
        mDirect->SetSocket(NULL);
        low_free(mReadData);
    }

    if(mAcceptConnectCallID)
    {
        if(mType == LOWSOCKET_TYPE_CONNECTED)
            low_remove_stash(mLow->duk_ctx, mAcceptConnectCallID);
    }

    if(mReadCallID)
        low_remove_stash(mLow->duk_ctx, mReadCallID);
    if(mWriteCallID)
        low_remove_stash(mLow->duk_ctx, mWriteCallID);
    if(FD() >= 0 && mType != LOWSOCKET_TYPE_STDINOUT)
        close(FD());
    SetFD(-1);

    if(mSSL)
    {
        mbedtls_ssl_free(mSSL);
        low_free(mSSL);
    }
    if(mTLSContext)
    {
        mTLSContext->DecRef();
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::InitSocket
// -----------------------------------------------------------------------------

bool LowSocket::InitSocket(struct sockaddr *remoteAddr)
{
    if(remoteAddr && remoteAddr->sa_family == AF_INET)
    {
        struct sockaddr_in *addr;
        unsigned char *ip;

        mNodeFamily = 4;

        addr = (struct sockaddr_in *)remoteAddr;
        ip = (unsigned char *)&addr->sin_addr.s_addr;
        sprintf(mRemoteHost, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        mRemotePort = ntohs(addr->sin_port);
    }
    else if(remoteAddr && remoteAddr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *addr;

        mNodeFamily = 6;

        addr = (struct sockaddr_in6 *)remoteAddr;
        if(inet_ntop(AF_INET6,
                     addr->sin6_addr.s6_addr,
                     mRemoteHost,
                     sizeof(mRemoteHost)) == NULL)
        {
            mAcceptConnectErrno = errno;
            mAcceptConnectErrnoSSL = false;
            mAcceptConnectError = true;
            mAcceptConnectSyscall = "inet_ntop";
        }
        mRemotePort = ntohs(addr->sin6_port);
    }
    else
        mNodeFamily = 0; // UNIX

    u_long mode = 1;
#if LOW_ESP32_LWIP_SPECIALITIES
    if(!(FD() >= 0 && FD() <= 2))
#else
    if(FD() != 1 && FD() != 2)
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(ioctl(FD(), FIONBIO, &mode) < 0)
        {
            mAcceptConnectErrno = errno;
            mAcceptConnectErrnoSSL = false;
            mAcceptConnectError = true;
            mAcceptConnectSyscall = "ioctl";
            return false;
        }

    if(mTLSContext)
    {
        mSSL = (mbedtls_ssl_context *)low_alloc(sizeof(mbedtls_ssl_context));
        if(!mSSL)
        {
            mAcceptConnectErrno = ENOMEM;
            mAcceptConnectErrnoSSL = false;
            mAcceptConnectError = true;
            mAcceptConnectSyscall = "malloc";
            return false;
        }

        mbedtls_ssl_init(mSSL);

        int ret;
        if((ret = mbedtls_ssl_setup(mSSL, &mTLSContext->GetSSLConfig())) != 0)
        {
            mAcceptConnectErrno = ret;
            mAcceptConnectErrnoSSL = true;
            mAcceptConnectError = true;
            mAcceptConnectSyscall = "mbedtls_ssl_setup";
            return false;
        }

        if(mHost)
        {
            if((ret = mbedtls_ssl_set_hostname(mSSL, mHost)) != 0)
            {
                mAcceptConnectErrno = ret;
                mAcceptConnectErrnoSSL = true;
                mAcceptConnectError = true;
                mAcceptConnectSyscall = "mbedtls_ssl_set_hostname";
                return false;
            }
            low_free(mHost);
            mHost = NULL;
        }

        mbedtls_ssl_set_bio(
          mSSL, &FD(), mbedtls_net_send, mbedtls_net_recv, NULL);
    }

    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::Connect
// -----------------------------------------------------------------------------

bool LowSocket::Connect(struct sockaddr *remoteAddr,
                        int remoteAddrLen,
                        int callIndex,
                        int &err,
                        const char *&syscall)
{
    if(mType != LOWSOCKET_TYPE_CONNECTED || FD() >= 0)
    {
        err = mConnected ? EISCONN : EALREADY;
        syscall = "connect";
        return false;
    }

    SetFD(socket(remoteAddr->sa_family, SOCK_STREAM, 0));
    if(FD() < 0)
    {
        err = errno;
        syscall = "socket";
        return false;
    }
    AdvertiseFD();

    if(InitSocket(remoteAddr))
    {
        if(connect(FD(), remoteAddr, remoteAddrLen) < 0)
        {
            if(errno != EINPROGRESS)
            {
                mAcceptConnectErrno = errno;
                mAcceptConnectErrnoSSL = false;
                mAcceptConnectError = true;
                mAcceptConnectSyscall = "connect";
            }
        }
        else if(!mTLSContext)
            mConnected = true;
    }

    if(mAcceptConnectError)
    {
        close(FD());
        SetFD(-1);

        err = mAcceptConnectError;
        syscall = mAcceptConnectSyscall;
        return false;
    }
    else
    {
        if(mConnected)
        {
            if(callIndex == -1)
                return true;
            else
                return CallAcceptConnect(callIndex, false);
        }
        else
        {
            mAcceptConnectCallID =
              callIndex != -1 ? low_add_stash(mLow->duk_ctx, callIndex) : 0;
            low_web_set_poll_events(mLow, this, POLLOUT);

            return true;
        }
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::Read
// -----------------------------------------------------------------------------

void LowSocket::Read(int pos, unsigned char *data, int len, int callIndex)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    if(FD() >= 0 && FD() <= 2)
    {
        // don't call back, we are busy waiting for "input"
        return;
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    if(mDirect || mReadData)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EAGAIN, "read");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }

    mReadPos = 0;
    mReadData = data;
    mReadLen = len;

    // If TLS context is used, always use other thread to read
    bool tryNow = !mTLSContext && mConnected;
    len = mClosed ? 0 : (tryNow ? DoRead() : -1);
    if(len >= 0 ||
       (tryNow && len == -1 && ((mReadErrno != EAGAIN && mReadErrno != EINTR) || mReadErrnoSSL)))
    {
        if(len == 0)
            mClosed = true;

        mReadData = NULL;
        duk_dup(mLow->duk_ctx, callIndex);
        if(len >= 0)
        {
            duk_push_null(mLow->duk_ctx);
            duk_push_int(mLow->duk_ctx, len);
            low_call_next_tick(mLow->duk_ctx, 2);
        }
        else
        {
            PushError(0);
            low_call_next_tick(mLow->duk_ctx, 1);
        }
    }
    else
    {
        mReadCallID = low_add_stash(mLow->duk_ctx, callIndex);

        short events =
          mClosed ? 0
                  : (POLLIN | (!mConnected || (mWriteData && !mWritePos) ||
                                   mDirectWriteEnabled
                                 ? POLLOUT
                                 : 0));
        low_web_set_poll_events(mLow, this, events);
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::Write
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES
void neoniousConsoleInput(char *data, int len, int fd);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

void LowSocket::Write(int pos, unsigned char *data, int len, int callIndex)
{
    if(FD() >= 1 && FD() <= 2)
    {
#if LOW_ESP32_LWIP_SPECIALITIES
        neoniousConsoleInput((char *)data, len, FD());
#else
        int left = len;
        while(left)
        {
            int size = ::write(FD(), data, left);
            if(size == -1)
            {
                if(errno == EAGAIN || errno == EINTR)
                    size = 0;
                else
                    break;
            }

            data += size;
            left -= size;
        }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

        duk_dup(mLow->duk_ctx, callIndex);
        duk_push_null(mLow->duk_ctx);
        duk_push_int(mLow->duk_ctx, len);
        low_call_next_tick(mLow->duk_ctx, 2);
        return;
    }

    if(mDirect || mWriteData)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EAGAIN, "write");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }

    mWritePos = 0;
    mWriteData = data;
    mWriteLen = len;

    // If TLS context is used, always use other thread to read
    bool tryNow = !mTLSContext && mConnected;
    len = mClosed ? 0 : (tryNow ? DoWrite() : -1);
    if(len >= 0 ||
       (tryNow && len == -1 && ((mWriteErrno != EAGAIN && mWriteErrno != EINTR) || mWriteErrnoSSL)))
    {
        mWriteData = NULL;

        duk_dup(mLow->duk_ctx, callIndex);
        if(len > 0)
        {
            duk_push_null(mLow->duk_ctx);
            duk_push_int(mLow->duk_ctx, len);
            low_call_next_tick(mLow->duk_ctx, 2);
        }
        else
        {
            PushError(1);
            low_call_next_tick(mLow->duk_ctx, 1);
        }
    }
    else
    {
        mWriteCallID = low_add_stash(mLow->duk_ctx, callIndex);

        short events =
          ((mReadData && !mReadPos) || mDirectReadEnabled ? POLLIN : 0) |
          POLLOUT;
        low_web_set_poll_events(mLow, this, events);
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::Shutdown
// -----------------------------------------------------------------------------

void LowSocket::Shutdown(int callIndex)
{
    duk_dup(mLow->duk_ctx, callIndex);
    if(!mConnected)
    {
        low_push_error(mLow->duk_ctx, ENOTCONN, "shutdown");
        low_call_next_tick(mLow->duk_ctx, 1);
    }
    else if(Shutdown() < 0)
    {
        int err = errno;
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, err, "shutdown");
    }
    else
    {
        duk_dup(mLow->duk_ctx, callIndex);
        duk_push_null(mLow->duk_ctx);
    }
    low_call_next_tick(mLow->duk_ctx, 1);
}

// -----------------------------------------------------------------------------
//  LowSocket::Shutdown
// -----------------------------------------------------------------------------

int LowSocket::Shutdown()
{
    int ret = 0;
    if(mTLSContext)
    {
        if(mSSL)
            ret = mbedtls_ssl_close_notify(mSSL); // todo: errnos are wrong
    }
    else
        ret = shutdown(FD(), SHUT_WR);

    return ret;
}

// -----------------------------------------------------------------------------
//  LowSocket::Close
// -----------------------------------------------------------------------------

bool LowSocket::Close(int callIndex)
{
    if(mDestroyed)
        return true;

    if(callIndex != -1)
        mCloseCallID = low_add_stash(mLow->duk_ctx, callIndex);

    mDestroyed = true;
    if(mCloseCallID)
        low_loop_set_callback(mLow, this);

    // for direct
    else
        low_web_mark_delete(mLow, this);
    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::KeepAlive
// -----------------------------------------------------------------------------

void LowSocket::KeepAlive(bool enable, int secs)
{
    if(FD() < 0)
        return;

    int opt = enable;
    if(setsockopt(FD(), SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(int)) < 0)
    {
        low_push_error(mLow->duk_ctx, errno, "setsockopt");
        duk_throw(mLow->duk_ctx);
    }

    if(secs)
    {
#ifdef __APPLE__
        if(setsockopt(FD(), IPPROTO_TCP, TCP_KEEPALIVE, &secs, sizeof(int)) < 0)
#else
        if(setsockopt(FD(), IPPROTO_TCP, TCP_KEEPIDLE, &secs, sizeof(int)) < 0)
#endif /* #ifdef __APPLE__ */
        {
            low_push_error(mLow->duk_ctx, errno, "setsockopt");
            duk_throw(mLow->duk_ctx);
        }
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::NoDelay
// -----------------------------------------------------------------------------

void LowSocket::NoDelay(bool enable)
{
    int opt = enable;
    if(setsockopt(FD(), IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(int)) < 0)
    {
        low_push_error(mLow->duk_ctx, errno, "setsockopt");
        duk_throw(mLow->duk_ctx);
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::SetDirect
// -----------------------------------------------------------------------------

bool LowSocket::SetDirect(LowSocketDirect *direct, int type, bool fromWebThread)
{
    if(direct && (mDirect || !mConnected || mReadData || mWriteData))
        return false;
    if(!direct && !mDirect)
        return false;

    if(!direct && mDirect)
    {
        if(!fromWebThread)
            low_web_clear_poll(mLow, this);
        low_free(mReadData);
        mReadData = NULL;
    }

    mDirect = direct;
    mDirectType = type;
    if(mDirect)
    {
        mDirectReadEnabled = true;

        mDirect->SetSocket(this);
        if(!mDestroyed)
            low_web_set_poll_events(
              mLow,
              this,
              mClosed ? 0
                      : (POLLIN | (mTLSContext ? POLLOUT : 0) |
                         (!mConnected || mDirectWriteEnabled ? POLLOUT : 0)));
    }

    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::GetDirect
// -----------------------------------------------------------------------------

LowSocketDirect *LowSocket::GetDirect(int &type)
{
    type = mDirectType;
    return mDirect;
}

// -----------------------------------------------------------------------------
//  LowSocket::TriggerDirect
// -----------------------------------------------------------------------------

void LowSocket::TriggerDirect(int trigger)
{
    if(mDestroyed)
        return;

    if(trigger & LOWSOCKET_TRIGGER_READ)
        mDirectReadEnabled = true;
    if(trigger & LOWSOCKET_TRIGGER_WRITE)
        mDirectWriteEnabled = true;

    short events =
      mClosed
        ? 0
        : (mDirectReadEnabled ? (POLLIN | (mTLSContext ? POLLOUT : 0)) : 0) |
            (!mConnected || mDirectWriteEnabled ? POLLOUT : 0);
    low_web_set_poll_events(mLow, this, events);
}

// -----------------------------------------------------------------------------
//  LowSocket::OnEvents
// -----------------------------------------------------------------------------

bool LowSocket::OnEvents(short events)
{
    if((mDestroyed || mAcceptConnectError) && !mCloseCallID)
    {
        // We will be destroyed on other ways ASAP
        low_web_set_poll_events(mLow, this, 0);
        return true;
    }
    if(mClosed)
    {
        low_web_set_poll_events(mLow, this, 0);
        return true;
    }

    while(mTLSContext && mSSL->state != MBEDTLS_SSL_HANDSHAKE_OVER)
    {
        int ret = mbedtls_ssl_handshake_step(mSSL);
        if(mSSL->state == MBEDTLS_SSL_HANDSHAKE_OVER)
        {
            mDirectReadEnabled = mDirect;
            mDirectWriteEnabled = mDirect;
        }
        else if(ret)
        {
            if(ret == MBEDTLS_ERR_SSL_WANT_READ)
            {
                mDirectWriteEnabled = false;
                mDirectReadEnabled = true;
            }
            else if(ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                mDirectReadEnabled = false;
                mDirectWriteEnabled = true;
            }
            else
            {
                mDirectReadEnabled = false;
                mDirectWriteEnabled = false;

                low_web_set_poll_events(mLow, this, 0);
                if(mAcceptConnectCallID)
                {
                    mAcceptConnectErrno = ret;
                    mAcceptConnectErrnoSSL = true;
                    mAcceptConnectError = true;
                    mAcceptConnectSyscall = "mbedtls_ssl_handshake_step";
                    low_loop_set_callback(mLow, this);
                }
                else
                    return false;

                return true;
            }

            short events = (mDirectReadEnabled ? POLLIN : 0) |
                           (mDirectWriteEnabled ? POLLOUT : 0);
            low_web_set_poll_events(mLow, this, events);
            return true;
        }
    }

    if(!mConnected)
    {
        int error;
        socklen_t len = sizeof(error);

        if(getsockopt(FD(), SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            error = errno;
        if(error)
        {
            low_web_set_poll_events(mLow, this, 0);
            if(mAcceptConnectCallID)
            {
                mAcceptConnectErrno = error;
                mAcceptConnectErrnoSSL = false;
                mAcceptConnectError = true;
                mAcceptConnectSyscall = "connect";

                low_loop_set_callback(mLow, this);
            }
            else
                return false;

            return true;
        }

        if(mDirect)
            mDirect->OnSocketConnected();
        mConnected = true;
        if(mAcceptConnectCallID)
            low_loop_set_callback(mLow, this);
    }

    if(mDirect)
    {
        if(((events & (POLLIN | POLLHUP | POLLERR)) || mTLSContext) &&
            mDirectReadEnabled)
        {
            if(!mReadData)
            {
                mReadData = (unsigned char *)low_alloc(1024);
                mReadLen = 1024;
            }
            if(mReadData)
            {
                mDirectReadEnabled = false; // no race conditions
                while(true) // required with SSL b/c Read might not always
                            // be retriggered if SSL still has data
                {
                    int len = DoRead();
                    if(len < 0 && (mReadErrno == EAGAIN || mReadErrno == EINTR) && !mReadErrnoSSL)
                    {
                        mDirectReadEnabled = true;
                        break;
                    }

                    if(len == 0)
                        mClosed = true;
                    if(!mDirect->OnSocketData(mReadData, len))
                        break;

                    if(!mTLSContext)
                    {
                        mDirectReadEnabled = true;
                        break;
                    }
                }
            }
        }
        if((events & POLLOUT) && mDirectWriteEnabled)
        {
            mDirectWriteEnabled = false; // no race conditions
            if(mDirect->OnSocketWrite())
                mDirectWriteEnabled = true;
        }

        short events = mClosed ? 0
                                : (mDirectReadEnabled ? POLLIN : 0) |
                                    (mDirectWriteEnabled ? POLLOUT : 0);
        low_web_set_poll_events(mLow, this, events);
    }
    else
    {
        bool change = false;
        if((events & (POLLIN | POLLHUP | POLLERR)) && mReadData &&
            !mReadPos)
        {
            int len = DoRead();
            if(len == 0)
                mClosed = true;

            if(len >= 0 || (mReadErrno != EAGAIN && mReadErrno != EINTR) || mReadErrnoSSL)
            {
                mReadPos = len;
                change = true;
            }
        }
        if((events & POLLOUT) && mWriteData && !mWritePos)
        {
            int len = DoWrite();
            if(len >= 0 || (mReadErrno != EAGAIN && mReadErrno != EINTR) || mReadErrnoSSL)
            {
                mWritePos = len;
                change = true;
            }
        }

        low_web_set_poll_events(mLow, this, 0);
        if(change)
            low_loop_set_callback(mLow, this);
    }

    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::OnLoop
// -----------------------------------------------------------------------------

bool LowSocket::OnLoop()
{
    duk_context *ctx = mLow->duk_ctx;

    if(mDestroyed)
    {
        if(mCloseCallID)
        {
            low_push_stash(ctx, mCloseCallID, true);
            duk_push_null(ctx);
            duk_call(ctx, 1);
        }
        return false;
    }

    if(mAcceptConnectCallID)
    {
        if(!CallAcceptConnect(mAcceptConnectCallID, true))
        {
            if(mCloseCallID)
            {
                low_push_stash(ctx, mCloseCallID, true);
                duk_push_null(ctx);
                duk_call(ctx, 1);
            }
            return false;
        }
    }
    else if(mAcceptConnectError)
    {
        if(mCloseCallID)
        {
            low_push_stash(ctx, mCloseCallID, true);
            duk_push_null(ctx);
            duk_call(ctx, 1);
        }
        return false;
    }

    if(mDirect || !mConnected)
        return true;

    if(mReadData && (mClosed || mReadPos))
    {
        mReadData = NULL;

        int callID = mReadCallID;
        mReadCallID = 0;

        low_push_stash(ctx, callID, true);
        if(mReadPos >= 0)
        {
            duk_push_null(ctx);
            duk_push_int(ctx, mReadPos);
            duk_call(ctx, 2);
        }
        else
        {
            PushError(0);
            duk_call(ctx, 1);
        }
    }
    if(mWriteData && mWritePos)
    {
        mWriteData = NULL;

        int callID = mWriteCallID;
        mWriteCallID = 0;

        low_push_stash(ctx, callID, true);
        if(mWritePos > 0)
        {
            duk_push_null(ctx);
            duk_push_int(ctx, mWritePos);
            duk_call(ctx, 2);
        }
        else
        {
            PushError(1);
            duk_call(ctx, 1);
        }
    }

    short events =
      mClosed ? 0 : (mReadData ? POLLIN : 0) | (mWriteData ? POLLOUT : 0);
    low_web_set_poll_events(mLow, this, events);
    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::CallAcceptConnect
// -----------------------------------------------------------------------------

bool LowSocket::CallAcceptConnect(int callIndex, bool onStash)
{
    duk_context *ctx = mLow->duk_ctx;

    char localHost[INET6_ADDRSTRLEN];
    int localPort = 0;

    mAcceptConnectCallID = 0;

    if(!mAcceptConnectError && mNodeFamily)
    {
        if(mNodeFamily == 4)
        {
            sockaddr_in localAddr;
            socklen_t localAddrLen = sizeof(localAddr);
            if(getsockname(FD(), (sockaddr *)&localAddr, &localAddrLen) < 0)
            {
                mAcceptConnectErrno = errno;
                mAcceptConnectErrnoSSL = false;
                mAcceptConnectError = true;
                mAcceptConnectSyscall = "getsockname";
            }
            unsigned char *ip = (unsigned char *)&localAddr.sin_addr.s_addr;
            sprintf(localHost, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            localPort = ntohs(localAddr.sin_port);
        }
        else
        {
            sockaddr_in6 localAddr;
            socklen_t localAddrLen = sizeof(localAddr);
            if(getsockname(FD(), (sockaddr *)&localAddr, &localAddrLen) < 0 ||
               inet_ntop(AF_INET6,
                         localAddr.sin6_addr.s6_addr,
                         localHost,
                         sizeof(localHost)) == NULL)
            {
                mAcceptConnectErrno = errno;
                mAcceptConnectErrnoSSL = false;
                mAcceptConnectError = true;
                mAcceptConnectSyscall = "getsockname";
            }
            localPort = ntohs(localAddr.sin6_port);
        }
    }

    if(mAcceptConnectError)
    {
        if(onStash)
            low_push_stash(ctx, callIndex, mType == LOWSOCKET_TYPE_CONNECTED);
        else
            duk_dup(ctx, callIndex);
        PushError(2);
        if(onStash)
            duk_call(ctx, 1);
        else
            low_call_next_tick(ctx, 1);

        return false;
    }
    else if(mConnected)
    {
        if(onStash)
            low_push_stash(ctx, callIndex, mType == LOWSOCKET_TYPE_CONNECTED);
        else
            duk_dup(ctx, callIndex);
        duk_push_null(ctx);
        duk_push_int(ctx, FD());
        duk_push_int(ctx, mNodeFamily);
        if(mNodeFamily)
        {
            duk_push_string(ctx, localHost);
            duk_push_int(ctx, localPort);
            duk_push_string(ctx, mRemoteHost);
            duk_push_int(ctx, mRemotePort);
            if(onStash)
                duk_call(ctx, 7);
            else
                low_call_next_tick(ctx, 7);
        }
        else
        {
            if(onStash)
                duk_call(ctx, 3);
            else
                low_call_next_tick(ctx, 3);
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
//  LowSocket::DoRead
// -----------------------------------------------------------------------------

int LowSocket::DoRead()
{
    int len = 0;
    while(len != mReadLen)
    {
        int size = mTLSContext
                     ? mbedtls_ssl_read(mSSL, mReadData + len, mReadLen - len)
#if LOW_ESP32_LWIP_SPECIALITIES
                     : lwip_read(FD(), mReadData + len, mReadLen - len);
#else
                     : read(FD(), mReadData + len, mReadLen - len);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(size < 0)
        {
            if(mTLSContext)
            {
                if(size == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                    return len;
                if(size == MBEDTLS_ERR_SSL_WANT_READ)
                {
                    mReadErrno = EAGAIN;
                    mReadErrnoSSL = false;
                }
                else
                {
                    mReadErrno = size;
                    mReadErrnoSSL = true;
                }
            }
            else
            {
                mReadErrno = errno;
                mReadErrnoSSL = false;
            }
            return len ? len : -1;
        }
        else if(size == 0)
            return len;
        else
            len += size;
    }
    return len;
}

// -----------------------------------------------------------------------------
//  LowSocket::DoWrite
// -----------------------------------------------------------------------------

int LowSocket::DoWrite()
{
    int len = 0;
    while(len != mWriteLen)
    {
        int size =
          mTLSContext
            ? mbedtls_ssl_write(mSSL, mWriteData + len, mWriteLen - len)
#if LOW_ESP32_LWIP_SPECIALITIES
            : ::lwip_write(FD(), mWriteData + len, mWriteLen - len);
#else
            : ::write(FD(), mWriteData + len, mWriteLen - len);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(size < 0)
        {
            if(mTLSContext)
            {
                if(size == MBEDTLS_ERR_SSL_WANT_WRITE)
                {
                    mWriteErrno = EAGAIN;
                    mWriteErrnoSSL = false;
                }
                else
                {
                    mWriteErrno = size;
                    mWriteErrnoSSL = true;
                }
            }
            else
            {
                mWriteErrno = errno;
                mWriteErrnoSSL = false;
            }
            return len ? len : -1;
        }
        else if(size == 0)
            return len;
        else
            len += size;
    }
    return len;
}


// -----------------------------------------------------------------------------
//  LowSocket::write
// -----------------------------------------------------------------------------

int LowSocket::write(const unsigned char *data, int len)
{
    if(!len)
        return 0;

    int size;
    if(mTLSContext)
    {
        size = mbedtls_ssl_write(mSSL, data, len);
        if(size < 0)
        {
            if(size == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                errno = EAGAIN; // because we are checking this in LowHTTPDirect
                mWriteErrno = EAGAIN;
                mWriteErrnoSSL = false;
            }
            else
            {
                mWriteErrno = size;
                mWriteErrnoSSL = true;
            }
        }

        return size;
    }
    else
    {
#if LOW_ESP32_LWIP_SPECIALITIES
        size = ::lwip_write(FD(), data, len);
#else
        size = ::write(FD(), data, len);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        if(size < 0)
        {
            mWriteErrno = errno;
            mWriteErrnoSSL = false;
        }
    }
    return size;
}


// -----------------------------------------------------------------------------
//  LowSocket::writev
// -----------------------------------------------------------------------------

int LowSocket::writev(const struct iovec *iov, int iovcnt)
{
    if(!iovcnt)
        return 0;

    int size;
    if(mTLSContext)
    {
        size =
          mbedtls_ssl_write(mSSL, (unsigned char *)iov->iov_base, iov->iov_len);
        if(size < 0)
        {
            if(size == MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                errno = EAGAIN; // because we are checking this in LowHTTPDirect
                mWriteErrno = EAGAIN;
                mWriteErrnoSSL = false;
            }
            else
            {
                mWriteErrno = size;
                mWriteErrnoSSL = true;
            }
        }

        return size;
    }
    else
    {
#if LOW_ESP32_LWIP_SPECIALITIES
        size = lwip_writev(FD(), iov, iovcnt);
#else
        size = ::writev(FD(), iov, iovcnt);
#endif /* #if LOW_ESP32_LWIP_SPECIALITIES */

        if(size < 0)
        {
            mWriteErrno = errno;
            mWriteErrnoSSL = false;
        }
    }
    return size;
}

// -----------------------------------------------------------------------------
//  LowSocket::SetError
// -----------------------------------------------------------------------------

void LowSocket::SetError(bool write, int error, bool ssl)
{
    if(write)
    {
        mWriteErrno = error;
        mWriteErrnoSSL = ssl;
    }
    else
    {
        mReadErrno = error;
        mReadErrnoSSL = ssl;
    }
}

// -----------------------------------------------------------------------------
//  LowSocket::PushError
// -----------------------------------------------------------------------------

void LowSocket::PushError(int call)
{
    int error;
    bool ssl;
    const char *syscall;

    switch(call)
    {
        case 0:
            error = mReadErrno;
            ssl = mReadErrnoSSL;
            syscall = "read";
            break;

        case 1:
            error = mWriteErrno;
            ssl = mWriteErrnoSSL;
            syscall = "write";
            break;

        case 2:
            error = mAcceptConnectErrno;
            ssl = mAcceptConnectErrnoSSL;
            syscall = mAcceptConnectSyscall;
            break;

	default:
	    return;	// no warning
    }

    if(error && ssl)
    {
        duk_context *ctx = mLow->duk_ctx;

        char code[32], message[256];
        mbedtls_strerror(error, message, sizeof(message));
        strerror_r(error, message, sizeof(message) - 16 - strlen(syscall));
        sprintf(message + strlen(message), " (at %s)", syscall);
        duk_push_error_object(ctx, DUK_ERR_ERROR, message);
        sprintf(code, "ERR_MBEDTLS_%X", -error);
        duk_push_string(ctx, code);
        duk_put_prop_string(ctx, -2, "code");
        duk_push_int(ctx, -error);
        duk_put_prop_string(ctx, -2, "errno");
        duk_push_string(ctx, syscall);
        duk_put_prop_string(ctx, -2, "syscall");
    }
    else
        low_push_error(mLow->duk_ctx, error, syscall);
}
