// -----------------------------------------------------------------------------
//  low_http.cpp
// -----------------------------------------------------------------------------

#include "low_http.h"

#include "LowHTTPDirect.h"
#include "LowSocket.h"

#include "low_alloc.h"
#include "low_main.h"
#include "low_system.h"

#include <errno.h>

// -----------------------------------------------------------------------------
//  low_http_get_request
// -----------------------------------------------------------------------------

duk_ret_t low_http_get_request(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int socketFD = duk_require_int(ctx, 0);
    auto iter = low->fds.find(socketFD);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    int directType;
    LowHTTPDirect *direct = (LowHTTPDirect *)socket->GetDirect(directType);
    if(direct && directType == 0)
    {
        // Server version
        direct->SetRequestCallID(low_add_stash(ctx, 1));
    }
    else if(!direct)
    {
        // Client version
        direct = new LowHTTPDirect(low, false);
        if(!direct)
        {
            duk_dup(ctx, 1);
            low_push_error(ctx, ENOMEM, "malloc");
            low_call_next_tick(ctx, 1);
            return 0;
        }

        if(!socket->SetDirect(direct, 0))
            duk_reference_error(
              ctx, "file descriptor not available for direct object");
        direct->SetRequestCallID(low_add_stash(ctx, 1));
    }
    else
        duk_reference_error(
          ctx, "file descriptor is already acquired by direct object");

    return 0;
}

// -----------------------------------------------------------------------------
//  low_http_detach
// -----------------------------------------------------------------------------

duk_ret_t low_http_detach(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int socketFD = duk_require_int(ctx, 0);
    auto iter = low->fds.find(socketFD);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    int directType;
    LowHTTPDirect *direct = (LowHTTPDirect *)socket->GetDirect(directType);
    if(!direct || directType != 0)
        duk_reference_error(ctx, "file descriptor is not an HTTP stream");

    direct->Detach(true);
    return 1;
}

// -----------------------------------------------------------------------------
//  low_http_read
// -----------------------------------------------------------------------------

duk_ret_t low_http_read(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int socketFD = duk_require_int(ctx, 0);
    duk_size_t buf_len;
    unsigned char *buf =
      (unsigned char *)duk_require_buffer_data(ctx, 1, &buf_len);

    auto iter = low->fds.find(socketFD);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    int directType;
    LowHTTPDirect *http = (LowHTTPDirect *)socket->GetDirect(directType);
    if(!http || directType != 0)
        duk_reference_error(ctx, "file descriptor is not an HTTP stream");

    http->Read(buf, buf_len, 2);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_http_write
// -----------------------------------------------------------------------------

duk_ret_t low_http_write(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int socketFD = duk_require_int(ctx, 0);
    duk_size_t buf_len = 0;
    unsigned char *buf =
      duk_is_null(ctx, 1)
        ? NULL
        : (unsigned char *)duk_require_buffer_data(ctx, 1, &buf_len);

    auto iter = low->fds.find(socketFD);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    int directType;
    LowHTTPDirect *http = (LowHTTPDirect *)socket->GetDirect(directType);
    if(!http || directType != 0)
        duk_reference_error(ctx, "file descriptor is not an HTTP stream");

    http->Write(buf, buf_len, 1, 2);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_http_write_head
// -----------------------------------------------------------------------------

duk_ret_t low_http_write_head(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    int socketFD = duk_require_int(ctx, 0);
    const char *headers = duk_require_string(ctx, 1);
    int len = duk_require_int(ctx, 2);
    bool isChunked = duk_require_boolean(ctx, 3);

    auto iter = low->fds.find(socketFD);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_SOCKET)
        duk_reference_error(ctx, "file descriptor is not a socket");
    LowSocket *socket = (LowSocket *)iter->second;

    int directType;
    LowHTTPDirect *http = (LowHTTPDirect *)socket->GetDirect(directType);
    if(!http || directType != 0)
        duk_reference_error(ctx, "file descriptor is not an HTTP stream");

    http->WriteHeaders(headers, 1, len, isChunked);
    return 0;
}