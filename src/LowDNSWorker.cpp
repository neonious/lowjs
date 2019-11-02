// -----------------------------------------------------------------------------
//  LowDNSWorker.cpp
// -----------------------------------------------------------------------------

#include "LowDNSWorker.h"

#include "low_data_thread.h"
#include "low_main.h"
#include "low_system.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

// -----------------------------------------------------------------------------
//  LowDNSWorker::LowDNSWorker
// -----------------------------------------------------------------------------

LowDNSWorker::LowDNSWorker(low_t *low)
    : LowLoopCallback(low), LowDataCallback(low), mLow(low), mResult(NULL)
{
}

// -----------------------------------------------------------------------------
//  LowDNSWorker::~LowDNSWorker
// -----------------------------------------------------------------------------

LowDNSWorker::~LowDNSWorker()
{
    low_data_clear_callback(mLow, this);
    if (mResult)
        freeaddrinfo(mResult);
}

// -----------------------------------------------------------------------------
//  LowDNSWorker::Lookup
// -----------------------------------------------------------------------------

bool LowDNSWorker::Lookup(const char *host, int family, int hints,
                          int callIndex)
{
    if (strlen(host) >= sizeof(mHost))
    {
        duk_dup(mLow->duk_ctx, callIndex);
        duk_push_string(mLow->duk_ctx, "ERRIPTOOLONG");
        low_call_next_tick(mLow->duk_ctx, 1);
        return false;
    }

    strcpy(mHost, host);
    mFamily = family;
    mHints = hints;
    mLookupService = false;
    mCallID = low_add_stash(mLow->duk_ctx, callIndex);
    if(!mCallID)
        return false;

    mLow->run_ref++;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
    return true;
}

// -----------------------------------------------------------------------------
//  LowDNSWorker::LookupService
// -----------------------------------------------------------------------------

bool LowDNSWorker::LookupService(const char *ip, int port, int callIndex)
{
    if (strlen(ip) >= INET_ADDRSTRLEN)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        duk_push_string(mLow->duk_ctx, "ERRIPTOOLONG");
        low_call_next_tick(mLow->duk_ctx, 1);
        return false;
    }

    strcpy(mIP, ip);
    mPort = port;
    mLookupService = true;
    mCallID = low_add_stash(mLow->duk_ctx, callIndex);
    if(!mCallID)
        return false;

    mLow->run_ref++;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
    return true;
}

// -----------------------------------------------------------------------------
//  LowDNSWorker::OnLoop
// -----------------------------------------------------------------------------

bool LowDNSWorker::OnLoop()
{
    if (mLookupService)
    {
        if (mError)
        {
            low_push_stash(mLow->duk_ctx, mCallID, true);
            low_push_error(mLow, mError, "getnameinfo");
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            low_push_stash(mLow->duk_ctx, mCallID, true);
            duk_push_null(mLow->duk_ctx);
            duk_push_string(mLow->duk_ctx, mHost);
            duk_push_string(mLow->duk_ctx, mService);
            duk_call(mLow->duk_ctx, 3);
        }
    }
    else
    {
        if (!mError)
        {
            addrinfo *info = mResult;
            while (info)
            {
                if (info->ai_family == AF_INET || info->ai_family == AF_INET6)
                    break;
            }
            if (!info)
                mError = ENODATA;
        }

        if (mError)
        {
            low_push_stash(mLow->duk_ctx, mCallID, true);
            low_push_error(mLow, mError, "getaddrinfo");
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            low_push_stash(mLow->duk_ctx, mCallID, true);
            duk_push_null(mLow->duk_ctx);
            duk_push_array(mLow->duk_ctx);
            int arr_len = 0;

            addrinfo *info = mResult;
            while (info)
            {
                if ((info->ai_family == AF_INET ||
                     info->ai_family == AF_INET6) &&
                    inet_ntop(info->ai_family,
                              info->ai_family == AF_INET
                                  ? (void *)&((sockaddr_in *)info->ai_addr)
                                        ->sin_addr.s_addr
                                  : (void *)((sockaddr_in6 *)info->ai_addr)
                                        ->sin6_addr.s6_addr,
                              mHost, sizeof(mHost)) != NULL)
                {
                    duk_push_object(mLow->duk_ctx);
                    duk_push_string(mLow->duk_ctx, mHost);
                    duk_put_prop_string(mLow->duk_ctx, -2, "address");
                    duk_push_int(mLow->duk_ctx,
                                 info->ai_family == AF_INET ? 4 : 6);
                    duk_put_prop_string(mLow->duk_ctx, -2, "family");
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                info = info->ai_next;
            }
            duk_call(mLow->duk_ctx, 2);
        }
    }

    mLow->run_ref--;
    return false;
}

// -----------------------------------------------------------------------------
//  LowDNSWorker::OnData
// -----------------------------------------------------------------------------

bool LowDNSWorker::OnData()
{
    int err;

    if (mLookupService)
    {
#if LOW_ESP32_LWIP_SPECIALITIES
        err = EAI_NONAME;
#else
        sockaddr_in6 addr;
        int addrLen;
        bool isIPv4 = false;
        for (int i = 0; mIP[i]; i++)
        {
            if (mIP[i] == '.')
            {
                isIPv4 = true;
                break;
            }
        }

        if (isIPv4)
        {
            sockaddr_in *addr_in = (sockaddr_in *)&addr;

            addr_in->sin_family = AF_INET;
            if (inet_pton(AF_INET, mIP, &addr_in->sin_addr) != 1)
            {
                mError = errno;
                low_loop_set_callback(mLow, this);
                return true;
            }
            addr_in->sin_port = htons(mPort);
            addrLen = sizeof(sockaddr_in);
        }
        else
        {
            sockaddr_in6 *addr_in6 = &addr;

            addr_in6->sin6_family = AF_INET6;
            if (inet_pton(AF_INET6, mIP, &addr_in6->sin6_addr) != 1)
            {
                mError = errno;
                low_loop_set_callback(mLow, this);
                return true;
            }
            addr_in6->sin6_port = htons(mPort);
            addrLen = sizeof(sockaddr_in6);
        }

        err = getnameinfo((sockaddr *)&addr, addrLen, mHost, sizeof(mHost),
                          mService, sizeof(mService), 0);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    }
    else
    {
        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = mFamily;
        hints.ai_flags = mHints;
        err = getaddrinfo(mHost, NULL, &hints, &mResult);
    }
    switch (err)
    {
#if !LOW_ESP32_LWIP_SPECIALITIES
    case EAI_ADDRFAMILY:
    case EAI_SOCKTYPE:
        err = LOW_EBADFAMILY; // should not happen, as we control this
        break;
    case EAI_BADFLAGS:
        err = LOW_EBADFLAGS;
        break;
    case EAI_AGAIN:
        err = EAGAIN;
        break;
    case EAI_NODATA:
        err = LOW_ENODATA;
        break;
    case EAI_OVERFLOW:
        err = LOW_EUNKNOWN;
        break;
    case EAI_SYSTEM:
        err = errno;
        break;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    case EAI_SERVICE:
        err = LOW_EBADFAMILY; // should not happen, as we control this
        break;
    case EAI_FAIL:
        err = LOW_ESERVFAIL;
        break;
    case EAI_FAMILY:
        err = LOW_EBADFAMILY;
        break;
    case EAI_MEMORY:
        err = LOW_ENOMEM;
        break;
    case EAI_NONAME:
        err = LOW_ENONAME;
        break;
    default:
        err = LOW_EUNKNOWN;
    }
    mError = err;

    low_loop_set_callback(mLow, this);
    return true;
}