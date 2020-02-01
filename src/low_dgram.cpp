
// -----------------------------------------------------------------------------
//  low_dgram.cpp
// -----------------------------------------------------------------------------

#include "low_dgram.h"
#include "low_system.h"

#include "LowDatagram.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

// -----------------------------------------------------------------------------
//  low_dgram_bind
// -----------------------------------------------------------------------------

duk_ret_t low_dgram_bind(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    const char *address = duk_require_string(ctx, 0);
    int port = duk_require_int(ctx, 1);
    int family = duk_require_int(ctx, 2);
    bool reuseAddr = duk_require_boolean(ctx, 3);
    // 4 = listening cb
    // 5 = message cb

    int addrLen = family == 4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    unsigned char addrBuf[addrLen];
    sockaddr *addr = (sockaddr *)addrBuf;
    memset(addr, 0, addrLen);

    if(family == 4)
    {
        sockaddr_in *addr_in = (sockaddr_in *)addr;

        addr_in->sin_family = AF_INET;
        if(inet_pton(AF_INET, address, &addr_in->sin_addr) != 1)
        {
            int err = errno;
            duk_dup(low->duk_ctx, 4);
            low_push_error(ctx, err, "inet_pton");
            low_call_next_tick(low->duk_ctx, 1);
            return 0;
        }
        addr_in->sin_port = htons(port);
    }
    else
    {
        sockaddr_in6 *addr_in6 = (sockaddr_in6 *)addr;

        addr_in6->sin6_family = AF_INET6;
        if(inet_pton(AF_INET6, address, &addr_in6->sin6_addr) != 1)
        {
            int err = errno;
            duk_dup(low->duk_ctx, 4);
            low_push_error(ctx, err, "inet_pton");
            low_call_next_tick(low->duk_ctx, 1);
            return 0;
        }
        addr_in6->sin6_port = htons(port);
    }

    LowDatagram *datagram =
      new LowDatagram(low);
    if(!datagram)
    {
        duk_dup(low->duk_ctx, 4);
        low_push_error(ctx, ENOMEM, "malloc");
        low_call_next_tick(low->duk_ctx, 1);
        return 0;
    }

    int err;
    const char *syscall;
    if(!datagram->Bind(addr, addrLen, 5, err, syscall, reuseAddr))
    {
        delete datagram;

        duk_dup(ctx, 4);
        low_push_error(ctx, err, syscall);
        low_call_next_tick(ctx, 1);
    }
    else
    {
        duk_dup(ctx, 4);
        duk_push_null(ctx);
        duk_push_int(ctx, datagram->FD());
        if(family == 4)
            duk_push_int(ctx, ntohs(((struct sockaddr_in *)addr)->sin_port));
        else
            duk_push_int(ctx, ntohs(((struct sockaddr_in6 *)addr)->sin6_port));
        low_call_next_tick(ctx, 3);
    }

    return 0;
}

// -----------------------------------------------------------------------------
//  low_dgram_send
// -----------------------------------------------------------------------------

duk_ret_t low_dgram_send(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int fd = duk_require_int(ctx, 0);
    // 1 = array of buffers
    const char *address = duk_require_string(ctx, 2);
    int port = duk_require_int(ctx, 3);
    // 4 = callback

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    if(iter->second->FDType() != LOWFD_TYPE_DATAGRAM)
        duk_reference_error(ctx, "file descriptor is not a datagram socket");
    LowDatagram *datagram = (LowDatagram *)iter->second;

    datagram->Send(1, address, port, 4);
    return 0;
}