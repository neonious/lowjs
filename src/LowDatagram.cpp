// -----------------------------------------------------------------------------
//  LowDatagram.cpp
// -----------------------------------------------------------------------------

#include "LowDatagram.h"

#include "low_system.h"

#if LOW_ESP32_LWIP_SPECIALITIES
#include <lwip/sockets.h>
#define ioctl lwip_ioctl
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>


// -----------------------------------------------------------------------------
//  LowDatagram::LowDatagram
// -----------------------------------------------------------------------------

LowDatagram::LowDatagram(low_main_t *low)
    : mLow(low), LowFD(low, LOWFD_TYPE_DATAGRAM), LowLoopCallback(low), mMessageCallID(0), mSendCallID(0), mSendBufferID(0), mHasRecv(false)
{
}


// -----------------------------------------------------------------------------
//  LowDatagram::~LowDatagram
// -----------------------------------------------------------------------------

LowDatagram::~LowDatagram()
{
    low_web_clear_poll(mLow, this);

    if (FD() >= 0)
        close(FD());

    if (mMessageCallID)
        low_remove_stash(mLow, mMessageCallID);
    if (mSendCallID)
        low_remove_stash(mLow, mSendCallID);
    if (mSendBufferID)
        low_remove_stash(mLow, mSendBufferID);
}


// -----------------------------------------------------------------------------
//  LowDatagram::Bind
// -----------------------------------------------------------------------------

bool LowDatagram::Bind(struct sockaddr *addr, int addrLen, int callIndex, int &err,
                        const char *&syscall, bool reuseAddr)
{
    if (FD() >= 0)
    {
        err = EISCONN;
        syscall = "socket";
        return false;
    }

    mFamily = addr->sa_family;
    int fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        err = errno;
        return false;
    }

    u_long mode = reuseAddr ? 1 : 0;
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

    // Get port, if we called bind with 0
    if ((addr->sa_family == AF_INET && !((struct sockaddr_in *)addr)->sin_port) || (addr->sa_family == AF_INET6 && !((struct sockaddr_in6 *)addr)->sin6_port))
        getsockname(fd, addr, (socklen_t *)&addrLen);

    SetFD(fd);
    AdvertiseFD();

    mMessageCallID = low_add_stash(mLow, callIndex);
    low_web_set_poll_events(mLow, this, POLLIN);
    return true;
}


// -----------------------------------------------------------------------------
//  LowDatagram::Send
// -----------------------------------------------------------------------------

void LowDatagram::Send(int bufferIndex, const char *address, int port, int callIndex)
{
    if(mSendCallID)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow, EAGAIN, "write");
        duk_call(mLow->duk_ctx, 1);
        return;
    }

    if(mFamily == AF_INET)
    {
        sockaddr_in *addr_in = (sockaddr_in *)&mSendAddr;

        addr_in->sin_family = AF_INET;
        if(inet_pton(AF_INET, address, &addr_in->sin_addr) != 1)
        {
            int err = errno;
            duk_dup(mLow->duk_ctx, callIndex);
            low_push_error(mLow, err, "inet_pton");
            duk_call(mLow->duk_ctx, 1);
            return;
        }
        addr_in->sin_port = htons(port);
    }
    else
    {
        sockaddr_in6 *addr_in6 = (sockaddr_in6 *)&mSendAddr;

        addr_in6->sin6_family = AF_INET6;
        if(inet_pton(AF_INET6, address, &addr_in6->sin6_addr) != 1)
        {
            int err = errno;
            duk_dup(mLow->duk_ctx, callIndex);
            low_push_error(mLow, err, "inet_pton");
            duk_call(mLow->duk_ctx, 1);
            return;
        }
        addr_in6->sin6_port = htons(port);
    }

    duk_size_t len;
    mSendData = (unsigned char *)duk_require_buffer_data(mLow->duk_ctx, bufferIndex, &len);
    mSendLen = len;

    int res = sendto(FD(), mSendData, mSendLen, 0, (struct sockaddr *)&mSendAddr, mFamily == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
    if(res != -1 || errno != EAGAIN)
    {
        int err = errno;
        mSendData = NULL;

        duk_dup(mLow->duk_ctx, callIndex);
        if(len > 0)
            duk_push_null(mLow->duk_ctx);
        else
            low_push_error(mLow, err, "sendto");
        duk_call(mLow->duk_ctx, 1);
    }
    else
    {
        mSendBufferID = low_add_stash(mLow, bufferIndex);
        mSendCallID = low_add_stash(mLow, callIndex);
        low_web_set_poll_events(mLow, this, POLLIN | POLLOUT);
    }
}


// -----------------------------------------------------------------------------
//  LowDatagram::Close
// -----------------------------------------------------------------------------

bool LowDatagram::Close(int callIndex = -1)
{
    return false;
}


// -----------------------------------------------------------------------------
//  LowDatagram::OnEvents
// -----------------------------------------------------------------------------

bool LowDatagram::OnEvents(short events)
{
    bool handled = false;
    int newEvents = 0;

    if((events & POLLIN) && !mHasRecv)
    {
        mRecvAddrLen = mFamily == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
        mRecvLen = recvfrom(FD(), mRecvData, sizeof(mRecvData), 0, (struct sockaddr *)&mRecvAddr, &mRecvAddrLen);
        mRecvErr = errno;
        mHasRecv = true;

        if(mRecvLen != -1 || errno != EAGAIN)
            handled = true;
        else
            newEvents |= POLLIN;
    }
    if((events & POLLOUT) && mSendCallID)
    {
        mSendLen = sendto(FD(), mSendData, mSendLen, 0, (struct sockaddr *)&mSendAddr, mFamily == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
        mSendErr = errno;
        mSendData = NULL;

        if(mSendLen != -1 || errno != EAGAIN)
            handled = true;
        else
            newEvents |= POLLOUT;
    }

    low_web_set_poll_events(mLow, this, newEvents);
    if(handled)
        low_loop_set_callback(mLow, this);
    return true;
}


// -----------------------------------------------------------------------------
//  LowDatagram::OnLoop
// -----------------------------------------------------------------------------

bool LowDatagram::OnLoop()
{
    if(mHasRecv)
    {
        char remoteHost[INET6_ADDRSTRLEN];
        const char *syscall = "recvfrom";

        if(mRecvLen != -1)
        {
            if(mRecvAddr.sin6_family == AF_INET)
            {
                unsigned char *ip = (unsigned char *)&((struct sockaddr_in *)&mRecvAddr)->sin_addr.s_addr;
                sprintf(remoteHost, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
            }
            else
            {
                if(inet_ntop(AF_INET6,
                            mRecvAddr.sin6_addr.s6_addr,
                            remoteHost,
                            sizeof(remoteHost)) == NULL)
                {
                    mRecvErr = errno;
                    mRecvLen = -1;
                    syscall = "inet_ntop";
                }
            }
        }

        low_push_stash(mLow, mMessageCallID, false);
        if(mRecvLen == -1)
        {
            low_push_error(mLow, mRecvErr, syscall);
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            duk_push_null(mLow->duk_ctx);
            memcpy(low_push_buffer(mLow->duk_ctx, mRecvLen), mRecvData, mRecvLen);
            duk_push_object(mLow->duk_ctx);

            duk_push_string(mLow->duk_ctx, remoteHost);
            duk_put_prop_string(mLow->duk_ctx, -2, "address");
            if(mRecvAddr.sin6_family == AF_INET)
            {
                duk_push_int(mLow->duk_ctx, ntohs(((struct sockaddr_in *)&mRecvAddr)->sin_port));
                duk_put_prop_string(mLow->duk_ctx, -2, "port");
                duk_push_string(mLow->duk_ctx, "IPv4");
                duk_put_prop_string(mLow->duk_ctx, -2, "family");
            }
            else
            {
                duk_push_int(mLow->duk_ctx, ntohs(mRecvAddr.sin6_port));
                duk_put_prop_string(mLow->duk_ctx, -2, "port");
                duk_push_string(mLow->duk_ctx, "IPv6");
                duk_put_prop_string(mLow->duk_ctx, -2, "family");
            }
            duk_push_int(mLow->duk_ctx, mRecvLen);
            duk_put_prop_string(mLow->duk_ctx, -2, "size");

            duk_call(mLow->duk_ctx, 3);
        }

        mHasRecv = false;
    }
    if(!mSendData && mSendCallID)
    {
        low_push_stash(mLow, mSendCallID, true);
        mSendCallID = 0;
        low_remove_stash(mLow, mSendBufferID);
        mSendBufferID = 0;

        if(mSendLen == -1)
            low_push_error(mLow, mSendErr, "sendto");
        else
            duk_push_null(mLow->duk_ctx);
        duk_call(mLow->duk_ctx, 1);
    }
    low_web_set_poll_events(mLow, this, (mSendCallID ? POLLOUT : 0) | POLLIN);

    return true;
}