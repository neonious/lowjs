// -----------------------------------------------------------------------------
//  low_net.cpp
// -----------------------------------------------------------------------------

#include "low_net.h"

#include "LowServerSocket.h"
#include "LowSocket.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"
#include "low_system.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#if LOW_HAS_UNIX_SOCKET
#include <sys/un.h>
#endif /* LOW_HAS_UNIX_SOCKET */

// -----------------------------------------------------------------------------
//  low_net_listen
// -----------------------------------------------------------------------------

duk_ret_t low_net_listen(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int family = duk_require_int(ctx, 0);
    const char *address = duk_require_string(ctx, 1);
    bool isHTTP = duk_require_boolean(ctx, 3);

    int addrLen;
#if LOW_HAS_UNIX_SOCKET
    if(family == 0)
    {
        addrLen = sizeof(sockaddr_un) - sizeof(sockaddr_un::sun_path) +
                  strlen(address) + 1;
        if(addrLen < sizeof(sockaddr_un))
            addrLen = sizeof(sockaddr_un);
    }
    else
#endif /* LOW_HAS_UNIX_SOCKET */
        addrLen = family == 4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

    unsigned char addrBuf[addrLen];
    sockaddr *addr = (sockaddr *)addrBuf;
    memset(addr, 0, addrLen);

#if LOW_HAS_UNIX_SOCKET
    if(family == 0)
    {
        sockaddr_un *addr_un = (sockaddr_un *)addr;

        addr_un->sun_family = AF_UNIX;
        strcpy(addr_un->sun_path, address);
    }
    else
#endif /* LOW_HAS_UNIX_SOCKET */
    {
        int port = duk_require_int(ctx, 2);
        if(family == 4)
        {
            sockaddr_in *addr_in = (sockaddr_in *)addr;

            addr_in->sin_family = AF_INET;
            if(inet_pton(AF_INET, address, &addr_in->sin_addr) != 1)
            {
                int err = errno;
                duk_dup(ctx, 5);
                low_push_error(ctx, err, "inet_pton");
                low_call_next_tick(ctx, 1);
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
                duk_dup(ctx, 5);
                low_push_error(ctx, err, "inet_pton");
                low_call_next_tick(ctx, 1);
                return 0;
            }
            addr_in6->sin6_port = htons(port);
        }
    }

    LowTLSContext *tlsContext = NULL;
    if(duk_is_object(ctx, 4))
    {
        duk_get_prop_string(ctx, 4, "_index");

        int index = duk_require_int(ctx, -1);
        if(index < 0 || index >= low->tlsContexts.size() ||
           !low->tlsContexts[index])
            duk_reference_error(ctx, "tls context not found");

        tlsContext = low->tlsContexts[index];
    }

    LowServerSocket *server =
      new LowServerSocket(low, isHTTP, tlsContext);
    if(!server)
    {
        duk_dup(ctx, 5);
        low_push_error(ctx, ENOMEM, "malloc");
        low_call_next_tick(ctx, 1);
        return 0;
    }

    int err;
    const char *syscall;
    if(!server->Listen(addr, addrLen, 6, err, syscall))
    {
        delete server;

        duk_dup(ctx, 5);
        low_push_error(ctx, err, syscall);
        low_call_next_tick(ctx, 1);
    }
    else
    {
        duk_dup(ctx, 5);
        duk_push_null(ctx);
        duk_push_int(ctx, server->FD());
        if(family == 4)
            duk_push_int(ctx, ntohs(((struct sockaddr_in *)addr)->sin_port));
        else if(family == 6)
            duk_push_int(ctx, ntohs(((struct sockaddr_in6 *)addr)->sin6_port));
        else
            duk_push_int(ctx, 0);
        low_call_next_tick(ctx, 3);
    }
    return 0;
}

// -----------------------------------------------------------------------------
//  low_net_connect
// -----------------------------------------------------------------------------

duk_ret_t low_net_connect(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int family = duk_require_int(ctx, 0);
    const char *address = duk_require_string(ctx, 1);

    int addrLen;
#if LOW_HAS_UNIX_SOCKET
    if(family == 0)
    {
        addrLen = sizeof(sockaddr_un) - sizeof(sockaddr_un::sun_path) +
                  strlen(address) + 1;
        if(addrLen < sizeof(sockaddr_un))
            addrLen = sizeof(sockaddr_un);
    }
    else
#endif /* LOW_HAS_UNIX_SOCKET */
        addrLen = family == 4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

    unsigned char addrBuf[addrLen];
    sockaddr *addr = (sockaddr *)addrBuf;
    memset(addr, 0, addrLen);

#if LOW_HAS_UNIX_SOCKET
    if(family == 0)
    {
        sockaddr_un *addr_un = (sockaddr_un *)addr;

        addr_un->sun_family = AF_UNIX;
        strcpy(addr_un->sun_path, address);
    }
    else
#endif /* LOW_HAS_UNIX_SOCKET */
    {
        int port = duk_require_int(ctx, 3);
        if(family == 4)
        {
            sockaddr_in *addr_in = (sockaddr_in *)addr;

            addr_in->sin_family = AF_INET;
            if(inet_pton(AF_INET, address, &addr_in->sin_addr) != 1)
            {
                int err = errno;
                duk_dup(ctx, 5);
                low_push_error(ctx, err, "inet_pton");
                low_call_next_tick(ctx, 1);
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
                duk_dup(ctx, 5);
                low_push_error(ctx, err, "inet_pton");
                low_call_next_tick(ctx, 1);
                return 0;
            }
            addr_in6->sin6_port = htons(port);
        }
    }

    LowTLSContext *tlsContext = NULL;
    if(duk_is_object(ctx, 4))
    {
        duk_get_prop_string(ctx, 4, "_index");

        int index = duk_require_int(ctx, -1);

        if(index < 0 || index >= low->tlsContexts.size() ||
           !low->tlsContexts[index])
            duk_reference_error(ctx, "tls context not found");

        tlsContext = low->tlsContexts[index];
    }

    char *host = NULL;
    if(tlsContext)
    {
        host = low_strdup(duk_require_string(ctx, 2));
        if(!host)
        {
            duk_dup(ctx, 5);
            low_push_error(ctx, ENOMEM, "malloc");
            low_call_next_tick(ctx, 1);
            return 0;
        }
    }
    LowSocket *socket = new LowSocket(low, NULL, 0, tlsContext, host);
    if(!socket)
    {
        low_free(host);
        duk_dup(ctx, 5);
        low_push_error(ctx, ENOMEM, "malloc");
        low_call_next_tick(ctx, 1);
        return 0;
    }

    int err;
    const char *syscall;
    if(!socket->Connect(addr, addrLen, 5, err, syscall))
    {
        delete socket;

        duk_dup(ctx, 5);
        low_push_error(ctx, err, syscall);
        low_call_next_tick(ctx, 1);

        return 0;
    }

    duk_push_int(ctx, socket->FD());
    return 1;
}

// -----------------------------------------------------------------------------
//  low_net_setsockopt
// -----------------------------------------------------------------------------

duk_ret_t low_net_setsockopt(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    if(!duk_is_undefined(ctx, 4))
        if(!low_set_raw_mode(duk_require_boolean(ctx, 4)))
        {
            low_push_error(ctx, errno, "tcsetattr");
            duk_throw(ctx);
        }

    bool setKeepAlive = !duk_is_undefined(ctx, 1);
    bool setNoDelay = !duk_is_undefined(ctx, 3);
    if(!setKeepAlive && !setNoDelay)
        return 0;

    int fd = duk_require_int(ctx, 0);
    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    if(setKeepAlive)
        socket->KeepAlive(duk_require_boolean(ctx, 1), duk_require_int(ctx, 2));
    if(setNoDelay)
        socket->NoDelay(duk_require_boolean(ctx, 3));

    return 0;
}

// -----------------------------------------------------------------------------
//  low_net_shutdown
// -----------------------------------------------------------------------------

duk_ret_t low_net_shutdown(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    socket->Shutdown(1);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_net_connections
// -----------------------------------------------------------------------------

duk_ret_t low_net_connections(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SERVER)
        duk_reference_error(ctx, "file descriptor is not a server");
    LowServerSocket *socket = (LowServerSocket *)iter->second;

    socket->Connections(duk_require_int(ctx, 1), duk_require_int(ctx, 2));
    return 0;
}
