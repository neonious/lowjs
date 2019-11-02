// -----------------------------------------------------------------------------
//  LowServerSocket.cpp
// -----------------------------------------------------------------------------

#include "LowServerSocket.h"
#include "LowSocket.h"

#include "LowHTTPDirect.h"
#include "LowTLSContext.h"

#include "low_web_thread.h"
#include "low_main.h"
#include "low_alloc.h"

#if LOW_ESP32_LWIP_SPECIALITIES
#include <lwip/sockets.h>
#define ioctl lwip_ioctl
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>

// -----------------------------------------------------------------------------
//  LowServerSocket::LowServerSocket
// -----------------------------------------------------------------------------

LowServerSocket::LowServerSocket(low_t *low, bool isHTTP,
                                 LowTLSContext *secureContext)
    : LowFD(low, LOWFD_TYPE_SERVER), mLow(low), mIsHTTP(isHTTP),
      mAcceptCallID(0), mSecureContext(secureContext)
{
    if (mSecureContext)
        mSecureContext->AddRef();
}

// -----------------------------------------------------------------------------
//  LowServerSocket::~LowServerSocket
// -----------------------------------------------------------------------------

LowServerSocket::~LowServerSocket()
{
    low_web_clear_poll(mLow, this);

    if (FD() >= 0)
        close(FD());

    if (mAcceptCallID)
        low_remove_stash(mLow->duk_ctx, mAcceptCallID);
    if (mSecureContext)
        mSecureContext->DecRef();
}

// -----------------------------------------------------------------------------
//  LowServerSocket::Listen
// -----------------------------------------------------------------------------

bool LowServerSocket::Listen(struct sockaddr *addr, int addrLen, int callIndex,
                             int &err, const char *&syscall)
{
    if (FD() >= 0)
    {
        err = EISCONN;
        syscall = "socket";
        return false;
    }

    mFamily = addr->sa_family;
    int fd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (fd < 0)
    {
        err = errno;
        return false;
    }

    u_long mode = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&mode, sizeof(mode)) <
        0)
    {
        err = errno;
        syscall = "setsockopt";

        close(fd);
        return false;
    }
    if (ioctl(fd, FIONBIO, &mode) < 0)
    {
        err = errno;
        syscall = "ioctl";

        close(fd);
        return false;
    }

    if (::bind(fd, addr, addrLen) < 0)
    {
        err = errno;
        syscall = "bind";

        close(fd);
        return false;
    }
    if (listen(fd, 10) < 0)
    {
        err = errno;
        syscall = "listen";

        close(fd);
        return false;
    }

    // Get port, if we called bind with 0
    if ((addr->sa_family == AF_INET && !((struct sockaddr_in *)addr)->sin_port) || (addr->sa_family == AF_INET6 && !((struct sockaddr_in6 *)addr)->sin6_port))
        getsockname(fd, addr, (socklen_t *)&addrLen);

    SetFD(fd);
    AdvertiseFD();

    mAcceptCallID = low_add_stash(mLow->duk_ctx, callIndex);
    low_web_set_poll_events(mLow, this, POLLIN);
    return true;
}

// -----------------------------------------------------------------------------
//  LowServerSocket::Close
// -----------------------------------------------------------------------------

bool LowServerSocket::Close(int callIndex) { return false; }

// -----------------------------------------------------------------------------
//  LowServerSocket::OnEvents
// -----------------------------------------------------------------------------

bool LowServerSocket::OnEvents(short events)
{
    LowHTTPDirect *direct = NULL;
    if (mIsHTTP)
    {
        direct = new (low_new) LowHTTPDirect(mLow, true);
        if (!direct)
        {
            // error
            return true;
        }
    }

    int fd = -1;
    LowSocket *socket = NULL;

    if (mFamily == AF_INET || mFamily == AF_INET6)
    {
        sockaddr_in6 remoteAddr;
        socklen_t remoteAddrLen = sizeof(remoteAddr);

        fd = accept(FD(), (sockaddr *)&remoteAddr, &remoteAddrLen);
        if(fd < 0 && errno == ENFILE)
        {
            low_web_set_poll_events(mLow, this, 0);
            mLow->reset_accepts = true;
        }
        if (fd >= 0)
            socket = new (low_new)
                LowSocket(mLow, fd, (sockaddr *)&remoteAddr, mAcceptCallID,
                          direct, 0, mSecureContext);
    }
    else
    {
        fd = accept(FD(), NULL, NULL); // no address if UNIX
        if(fd < 0 && errno == ENFILE)
        {
            low_web_set_poll_events(mLow, this, 0);
            mLow->reset_accepts = true;
        }
        if (fd >= 0)
            socket = new (low_new) LowSocket(mLow, fd, NULL, mAcceptCallID,
                                             direct, 0, mSecureContext);
    }
    if (!socket)
    {
        // error
        if (fd >= 0)
            close(fd);
        delete direct;
    }

    return true;
}
