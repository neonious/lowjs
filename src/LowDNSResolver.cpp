// -----------------------------------------------------------------------------
//  LowDNSResolver.cpp
// -----------------------------------------------------------------------------

#include "low_config.h"
#if LOW_INCLUDE_CARES_RESOLVER

#include "LowDNSResolver.h"

#include "low_web_thread.h"
#include "low_main.h"
#include "low_system.h"
#include "low_alloc.h"
#include "low_config.h"

#include "../deps/c-ares/nameser.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

// -----------------------------------------------------------------------------
//  LowDNSResolver::LowDNSResolver
// -----------------------------------------------------------------------------

LowDNSResolver::LowDNSResolver(low_t *low)
    : mLow(low), mIndex(-1), mActiveQueries(0)
{
}

// -----------------------------------------------------------------------------
//  LowDNSResolver::~LowDNSResolver
// -----------------------------------------------------------------------------

LowDNSResolver::~LowDNSResolver()
{
    ares_destroy(mChannel);
    if(mActiveQueries != 0)
        fprintf(stderr, "LowDNSResolver::mActiveQueries is not 0!\n");

    if(mIndex >= 0)
    {
        for(int i = mIndex + 1; i < mLow->resolvers.size(); i++)
            if(mLow->resolvers[i])
            {
                mLow->resolvers[mIndex] = NULL;
                return;
            }
        mLow->resolvers.resize(mIndex);
    }
}

// -----------------------------------------------------------------------------
//  LowDNSResolver::Init
// -----------------------------------------------------------------------------

void neoniousGetAresServers(struct in_addr **servers, int *nservers);

bool LowDNSResolver::Init()
{
#if LOW_ESP32_LWIP_SPECIALITIES
    // We need to tell ares the servers, there is
    // no /etc/resolv.conf and no /etc/hosts
    struct ares_options options;

    options.lookups = (char *)"b";
    neoniousGetAresServers(&options.servers, &options.nservers);

    int err = ares_init_options(&mChannel, &options,
                                ARES_OPT_LOOKUPS | ARES_OPT_SERVERS);
    low_free(options.servers);
#else
    int err = ares_init(&mChannel);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(err != ARES_SUCCESS)
    {
        err = AresErr(err);
        low_push_error(mLow->duk_ctx, err, "ares_init");
        return false;
    }

    pthread_mutex_lock(&mLow->resolvers_mutex);
    for(int i = 0; i < mLow->resolvers.size(); i++)
        if(!mLow->resolvers[i])
        {
            mIndex = i;
            mLow->resolvers[i] = this;

            duk_push_int(mLow->duk_ctx, mIndex);
            return true;
        }

    mIndex = mLow->resolvers.size();
    mLow->resolvers.push_back(this);
    pthread_mutex_unlock(&mLow->resolvers_mutex);

    duk_push_int(mLow->duk_ctx, mIndex);
    return true;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver::Cancel
// -----------------------------------------------------------------------------

void LowDNSResolver::Cancel() { ares_cancel(mChannel); }

// -----------------------------------------------------------------------------
//  LowDNSResolver::GetServers
// -----------------------------------------------------------------------------

struct ares_addr_port_node *LowDNSResolver::GetServers(int &err)
{
    struct ares_addr_port_node *list;

    err = ares_get_servers_ports(mChannel, &list);
    if(err)
    {
        err = AresErr(err);
        return NULL;
    }

    return list;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver::SetServers
// -----------------------------------------------------------------------------

int LowDNSResolver::SetServers(struct ares_addr_port_node *list)
{
    int err = ares_set_servers_ports(mChannel, list);
    if(list)
        ares_free_data(list);
    if(err)
    {
        err = AresErr(err);

        low_push_error(mLow->duk_ctx, err, "ares_set_servers_ports");
        return err;
    }

    return 0;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver::AresErr
// -----------------------------------------------------------------------------

int LowDNSResolver::AresErr(int err)
{
    switch(err)
    {
    case ARES_ENODATA:
        err = LOW_ENODATA;
        break;
    case ARES_EFORMERR:
        err = LOW_EFORMERR;
        break;
    case ARES_ESERVFAIL:
        err = LOW_ESERVFAIL;
        break;
    case ARES_ENOTFOUND:
        err = LOW_ENOTFOUND;
        break;
    case ARES_ENOTIMP:
        err = LOW_ENOTIMP;
        break;
    case ARES_EREFUSED:
        err = LOW_EREFUSED;
        break;
    case ARES_EBADQUERY:
        err = LOW_EBADQUERY;
        break;
    case ARES_EBADNAME:
        err = LOW_EBADNAME;
        break;
    case ARES_EBADFAMILY:
        err = LOW_EBADFAMILY;
        break;
    case ARES_EBADRESP:
        err = LOW_EBADRESP;
        break;
    case ARES_ECONNREFUSED:
        err = LOW_ECONNREFUSED;
        break;
    case ARES_ETIMEOUT:
        err = LOW_ETIMEOUT;
        break;
    case ARES_EOF:
        err = LOW_EOF;
        break;
    case ARES_EFILE:
        err = LOW_EFILE;
        break;
    case ARES_ENOMEM:
        err = LOW_ENOMEM;
        break;
    case ARES_EDESTRUCTION:
        err = LOW_EDESTRUCTION;
        break;
    case ARES_EBADSTR:
        err = LOW_EBADSTR;
        break;
    case ARES_EBADFLAGS:
        err = LOW_EBADFLAGS;
        break;
    case ARES_ENONAME:
        err = LOW_ENONAME;
        break;
    case ARES_EBADHINTS:
        err = LOW_EBADHINTS;
        break;
    case ARES_ENOTINITIALIZED:
        err = LOW_ENOTINITIALIZED;
        break;
    case ARES_ELOADIPHLPAPI:
        err = LOW_ELOADIPHLPAPI;
        break;
    case ARES_EADDRGETNETWORKPARAMS:
        err = LOW_EADDRGETNETWORKPARAMS;
        break;
    case ARES_ECANCELLED:
        err = LOW_ECANCELLED;
        break;
    default:
        err = LOW_EUNKNOWN;
    }
    return err;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_Query::LowDNSResolver_Query
// -----------------------------------------------------------------------------

LowDNSResolver_Query::LowDNSResolver_Query(LowDNSResolver *resolver)
    : LowLoopCallback(resolver->mLow), mResolver(resolver),
      mLow(resolver->mLow), mChannel(resolver->mChannel), mRefID(0), mCallID(0),
      mData(NULL), mDataFree(NULL), mAresFree(NULL), mHostFree(NULL)
{
    resolver->mActiveQueries++;
    mLow->run_ref++;
    mLow->resolvers_active++;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_Query::~LowDNSResolver_Query
// -----------------------------------------------------------------------------

LowDNSResolver_Query::~LowDNSResolver_Query()
{
    low_loop_clear_callback(mLow, this);

    low_free(mData);
    low_free(mDataFree);
    if(mAresFree)
        ares_free_data(mAresFree);
    if(mHostFree)
        ares_free_hostent(mHostFree);

    mResolver->mActiveQueries--;
    mLow->run_ref--;
    mLow->resolvers_active--;

    if(mRefID)
        low_remove_stash(mLow->duk_ctx, mRefID);
    if(mCallID)
        low_remove_stash(mLow->duk_ctx, mCallID);
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_Query::Resolve
// -----------------------------------------------------------------------------

void LowDNSResolver_Query::Resolve(const char *hostname, const char *type,
                                   bool ttl, int refIndex, int callIndex)
{
    if(strcmp(type, "ANY") == 0)
        mDNSType = ns_t_any;
    else if(strcmp(type, "A") == 0)
        mDNSType = ns_t_a;
    else if(strcmp(type, "AAAA") == 0)
        mDNSType = ns_t_aaaa;
    else if(strcmp(type, "CNAME") == 0)
        mDNSType = ns_t_cname;
    else if(strcmp(type, "MX") == 0)
        mDNSType = ns_t_mx;
    else if(strcmp(type, "NS") == 0)
        mDNSType = ns_t_ns;
    else if(strcmp(type, "TXT") == 0)
        mDNSType = ns_t_txt;
    else if(strcmp(type, "SRV") == 0)
        mDNSType = ns_t_srv;
    else if(strcmp(type, "PTR") == 0)
        mDNSType = ns_t_ptr;
    else if(strcmp(type, "NAPTR") == 0)
        mDNSType = ns_t_naptr;
    else if(strcmp(type, "SOA") == 0)
        mDNSType = ns_t_soa;
    else
        duk_reference_error(mLow->duk_ctx, "unknown dns type");
    mTTL = ttl;

    mRefID = low_add_stash(mLow->duk_ctx, refIndex); // put Resolver object on stash so
                                            // it does not get garbage collected
    mCallID = low_add_stash(mLow->duk_ctx, callIndex);

    ares_query(mChannel, hostname, ns_c_in, mDNSType, CallbackStatic, this);
    // Make web thread start checking c-ares FDs
    low_web_thread_break(mLow);
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_Query::Callback
// -----------------------------------------------------------------------------

void LowDNSResolver_Query::Callback(int status, int timeouts,
                                    unsigned char *abuf, int alen)
{
    if(status != ARES_SUCCESS || alen == 0)
    {
        mError = LowDNSResolver::AresErr(status);
        mSyscall = "ares_query";
        low_loop_set_callback(mLow, this);
        return;
    }
    mError = 0;

    mLen = alen;
    mData = (unsigned char *)low_alloc(alen);
    if(!mData)
    {
        mError = LOW_ENOMEM;
        mSyscall = "malloc";
    }
    else
        memcpy(mData, abuf, alen);
    low_loop_set_callback(mLow, this);
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_Query::OnLoop
// -----------------------------------------------------------------------------

inline uint16_t cares_get_16bit(const unsigned char *p)
{
    return static_cast<uint32_t>(p[0] << 8U) | (static_cast<uint32_t>(p[1]));
}

inline uint32_t cares_get_32bit(const unsigned char *p)
{
    return static_cast<uint32_t>(p[0] << 24U) |
           static_cast<uint32_t>(p[1] << 16U) |
           static_cast<uint32_t>(p[2] << 8U) | static_cast<uint32_t>(p[3]);
}

bool LowDNSResolver_Query::OnLoop()
{
    if(mError)
    {
        low_push_stash(mLow->duk_ctx, mCallID, true);
        low_push_error(mLow->duk_ctx, mError, mSyscall);
        duk_call(mLow->duk_ctx, 1);
    }
    else
    {
        low_push_stash(mLow->duk_ctx, mCallID, false);
        duk_push_null(mLow->duk_ctx);
        if(mDNSType != ns_t_soa)
            duk_push_array(mLow->duk_ctx);
        int arr_len = 0;

        if(mDNSType == ns_t_a || mDNSType == ns_t_any)
        {
            struct ares_addrttl *aAddrs = NULL;
            int count;

            for(int i = 8;; i += 8)
            {
                mDataFree =
                    low_realloc(aAddrs, i * sizeof(struct ares_addrttl));
                aAddrs = (struct ares_addrttl *)mDataFree;
                if(!aAddrs)
                {
                    low_push_stash(mLow->duk_ctx, mCallID, false);
                    low_push_error(mLow->duk_ctx, LOW_ENOMEM, "realloc");
                    duk_call(mLow->duk_ctx, 1);
                    return false;
                }

                count = i;
                int status =
                    ares_parse_a_reply(mData, mLen, NULL, aAddrs, &count);
                if(status != ARES_SUCCESS)
                {
                    if(status == ARES_ENODATA && mDNSType == ns_t_any)
                        count = 0;
                    else
                    {
                        low_push_stash(mLow->duk_ctx, mCallID, false);
                        low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                                       "ares_parse_a_reply");
                        duk_call(mLow->duk_ctx, 1);
                        return false;
                    }
                }
                if(count < i)
                    break;
            }

            for(int i = 0; i < count; i++)
            {
                char host_str[64];
                if(inet_ntop(AF_INET, &aAddrs[i].ipaddr, host_str,
                             sizeof(host_str)) != NULL)
                {
                    if(mTTL || mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        if(mDNSType == ns_t_any)
                        {
                            duk_push_string(mLow->duk_ctx, "A");
                            duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        }
                        duk_push_string(mLow->duk_ctx, host_str);
                        duk_put_prop_string(mLow->duk_ctx, -2, "address");
                        duk_push_int(mLow->duk_ctx, aAddrs[i].ttl);
                        duk_put_prop_string(mLow->duk_ctx, -2, "ttl");
                    }
                    else
                    {
                        duk_push_string(mLow->duk_ctx, host_str);
                    }
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
            }

            mDataFree = NULL;
            low_free(aAddrs);
        }
        if(mDNSType == ns_t_aaaa || mDNSType == ns_t_any)
        {
            struct ares_addr6ttl *aaaaAddrs = NULL;
            int count;

            for(int i = 8;; i += 8)
            {
                mDataFree =
                    low_realloc(aaaaAddrs, i * sizeof(struct ares_addr6ttl));
                aaaaAddrs = (struct ares_addr6ttl *)mDataFree;
                if(!aaaaAddrs)
                {
                    low_push_stash(mLow->duk_ctx, mCallID, false);
                    low_push_error(mLow->duk_ctx, LOW_ENOMEM, "realloc");
                    duk_call(mLow->duk_ctx, 1);
                    return false;
                }

                count = i;
                int status =
                    ares_parse_aaaa_reply(mData, mLen, NULL, aaaaAddrs, &count);
                if(status != ARES_SUCCESS)
                {
                    if(status == ARES_ENODATA && mDNSType == ns_t_any)
                        count = 0;
                    else
                    {
                        low_push_stash(mLow->duk_ctx, mCallID, false);
                        low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                                       "ares_parse_aaaa_reply");
                        duk_call(mLow->duk_ctx, 1);
                        return false;
                    }
                }
                if(count < i)
                    break;
            }

            for(int i = 0; i < count; i++)
            {
                char host_str[64];
                if(inet_ntop(AF_INET6, &aaaaAddrs[i].ip6addr, host_str,
                             sizeof(host_str)) != NULL)
                {
                    if(mTTL || mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        if(mDNSType == ns_t_any)
                        {
                            duk_push_string(mLow->duk_ctx, "AAAA");
                            duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        }
                        duk_push_string(mLow->duk_ctx, host_str);
                        duk_put_prop_string(mLow->duk_ctx, -2, "address");
                        duk_push_int(mLow->duk_ctx, aaaaAddrs[i].ttl);
                        duk_put_prop_string(mLow->duk_ctx, -2, "ttl");
                    }
                    else
                    {
                        duk_push_string(mLow->duk_ctx, host_str);
                    }
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
            }

            mDataFree = NULL;
            low_free(aaaaAddrs);
        }
        if(mDNSType == ns_t_cname || mDNSType == ns_t_any)
        {
            struct hostent *ent;
            int status = ares_parse_a_reply(mData, mLen, &ent, NULL, NULL);
            if(status == ARES_SUCCESS)
            {
                mHostFree = ent;
                if(ent->h_aliases[0])
                {
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        duk_push_string(mLow->duk_ctx, "CNAME");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        duk_push_string(mLow->duk_ctx, ent->h_name);
                        duk_put_prop_string(mLow->duk_ctx, -2, "value");
                    }
                    else
                        duk_push_string(mLow->duk_ctx, ent->h_name);
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mHostFree = NULL;
                ares_free_hostent(ent);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_a_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_mx || mDNSType == ns_t_any)
        {
            struct ares_mx_reply *result;
            int status = ares_parse_mx_reply(mData, mLen, &result);
            if(status == ARES_SUCCESS)
            {
                mAresFree = result;
                for(struct ares_mx_reply *entry = result; entry;
                    entry = entry->next)
                {
                    duk_push_object(mLow->duk_ctx);
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_string(mLow->duk_ctx, "MX");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                    }
                    duk_push_string(mLow->duk_ctx, entry->host);
                    duk_put_prop_string(mLow->duk_ctx, -2, "exchange");
                    duk_push_int(mLow->duk_ctx, entry->priority);
                    duk_put_prop_string(mLow->duk_ctx, -2, "priority");
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mAresFree = NULL;
                ares_free_data(result);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_mx_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_naptr || mDNSType == ns_t_any)
        {
            struct ares_naptr_reply *result;
            int status = ares_parse_naptr_reply(mData, mLen, &result);
            if(status == ARES_SUCCESS)
            {
                mAresFree = result;
                for(struct ares_naptr_reply *entry = result; entry;
                    entry = entry->next)
                {
                    duk_push_object(mLow->duk_ctx);
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_string(mLow->duk_ctx, "NAPTR");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                    }
                    duk_push_string(mLow->duk_ctx, (char *)entry->flags);
                    duk_put_prop_string(mLow->duk_ctx, -2, "flags");
                    duk_push_string(mLow->duk_ctx, (char *)entry->service);
                    duk_put_prop_string(mLow->duk_ctx, -2, "service");
                    duk_push_string(mLow->duk_ctx, (char *)entry->regexp);
                    duk_put_prop_string(mLow->duk_ctx, -2, "regexp");
                    duk_push_string(mLow->duk_ctx, entry->replacement);
                    duk_put_prop_string(mLow->duk_ctx, -2, "replacement");
                    duk_push_int(mLow->duk_ctx, entry->order);
                    duk_put_prop_string(mLow->duk_ctx, -2, "order");
                    duk_push_int(mLow->duk_ctx, entry->preference);
                    duk_put_prop_string(mLow->duk_ctx, -2, "preference");
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mAresFree = NULL;
                ares_free_data(result);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_naptr_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_ns || mDNSType == ns_t_any)
        {
            struct hostent *ent;
            int status = ares_parse_ns_reply(mData, mLen, &ent);
            if(status == ARES_SUCCESS)
            {
                mHostFree = ent;
                for(int i = 0; ent->h_aliases[i]; i++)
                {
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        duk_push_string(mLow->duk_ctx, "NS");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        duk_push_string(mLow->duk_ctx, ent->h_aliases[i]);
                        duk_put_prop_string(mLow->duk_ctx, -2, "value");
                    }
                    else
                        duk_push_string(mLow->duk_ctx, ent->h_aliases[i]);
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mHostFree = NULL;
                ares_free_hostent(ent);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_ns_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_ptr || mDNSType == ns_t_any)
        {
            struct hostent *ent;
            int status =
                ares_parse_ptr_reply(mData, mLen, NULL, 0, AF_INET, &ent);
            if(status == ARES_SUCCESS)
            {
                mHostFree = ent;
                for(int i = 0; ent->h_aliases[i]; i++)
                {
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        duk_push_string(mLow->duk_ctx, "PTR");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        duk_push_string(mLow->duk_ctx, ent->h_aliases[i]);
                        duk_put_prop_string(mLow->duk_ctx, -2, "value");
                    }
                    else
                        duk_push_string(mLow->duk_ctx, ent->h_aliases[i]);
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mHostFree = NULL;
                ares_free_hostent(ent);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_ptr_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_soa || mDNSType == ns_t_any)
        {
            // Can't use ares_parse_soa_reply() here which can only parse single
            // record
            unsigned int ancount = cares_get_16bit(mData + 6);
            unsigned char *ptr = mData + NS_HFIXEDSZ;
            int rr_type, rr_len;
            char *name;
            char *rr_name;
            long temp_len; // NOLINT(runtime/int)
            int status = ares_expand_name(ptr, mData, mLen, &name, &temp_len);
            if(status != ARES_SUCCESS)
            {
                /* returns EBADRESP in case of invalid input */
                status = status == ARES_EBADNAME ? ARES_EBADRESP : status;
            }

            if(ptr + temp_len + NS_QFIXEDSZ > mData + mLen)
            {
                low_free(name);
                status = ARES_EBADRESP;
            }

            if(status == ARES_SUCCESS)
            {
                ptr += temp_len + NS_QFIXEDSZ;
                for(unsigned int i = 0; i < ancount; i++)
                {
                    status =
                        ares_expand_name(ptr, mData, mLen, &rr_name, &temp_len);
                    if(status != ARES_SUCCESS)
                        break;

                    ptr += temp_len;
                    if(ptr + NS_RRFIXEDSZ > mData + mLen)
                    {
                        low_free(rr_name);
                        status = ARES_EBADRESP;
                        break;
                    }

                    rr_type = cares_get_16bit(ptr);
                    rr_len = cares_get_16bit(ptr + 8);
                    ptr += NS_RRFIXEDSZ;

                    /* only need SOA */
                    if(rr_type == ns_t_soa)
                    {
                        ares_soa_reply soa;

                        status = ares_expand_name(ptr, mData, mLen, &soa.nsname,
                                                  &temp_len);
                        if(status != ARES_SUCCESS)
                        {
                            low_free(rr_name);
                            break;
                        }
                        ptr += temp_len;

                        status = ares_expand_name(ptr, mData, mLen,
                                                  &soa.hostmaster, &temp_len);
                        if(status != ARES_SUCCESS)
                        {
                            low_free(rr_name);
                            low_free(soa.nsname);
                            break;
                        }
                        ptr += temp_len;

                        if(ptr + 5 * 4 > mData + mLen)
                        {
                            low_free(rr_name);
                            low_free(soa.nsname);
                            low_free(soa.hostmaster);
                            status = ARES_EBADRESP;
                            break;
                        }

                        duk_push_object(mLow->duk_ctx);
                        if(mDNSType == ns_t_any)
                        {
                            duk_push_string(mLow->duk_ctx, "SOA");
                            duk_put_prop_string(mLow->duk_ctx, -2, "type");
                        }
                        duk_push_string(mLow->duk_ctx, soa.nsname);
                        duk_put_prop_string(mLow->duk_ctx, -2, "nsname");
                        duk_push_string(mLow->duk_ctx, soa.hostmaster);
                        duk_put_prop_string(mLow->duk_ctx, -2, "hostmaster");
                        duk_push_int(mLow->duk_ctx,
                                     cares_get_32bit(ptr + 0 * 4));
                        duk_put_prop_string(mLow->duk_ctx, -2, "serial");
                        duk_push_int(mLow->duk_ctx,
                                     cares_get_32bit(ptr + 1 * 4));
                        duk_put_prop_string(mLow->duk_ctx, -2, "refresh");
                        duk_push_int(mLow->duk_ctx,
                                     cares_get_32bit(ptr + 2 * 4));
                        duk_put_prop_string(mLow->duk_ctx, -2, "retry");
                        duk_push_int(mLow->duk_ctx,
                                     cares_get_32bit(ptr + 3 * 4));
                        duk_put_prop_string(mLow->duk_ctx, -2, "expire");
                        duk_push_int(mLow->duk_ctx,
                                     cares_get_32bit(ptr + 4 * 4));
                        duk_put_prop_string(mLow->duk_ctx, -2, "minttl");
                        if(mDNSType == ns_t_any)
                            duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);

                        low_free(soa.nsname);
                        low_free(soa.hostmaster);
                        break;
                    }

                    low_free(rr_name);
                    ptr += rr_len;
                }

                low_free(name);
            }

            if(status != ARES_SUCCESS &&
               (status != ARES_ENODATA || mDNSType != ns_t_any))
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_soa_reply (modified)");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_srv || mDNSType == ns_t_any)
        {
            struct ares_srv_reply *result;
            int status = ares_parse_srv_reply(mData, mLen, &result);
            if(status == ARES_SUCCESS)
            {
                mAresFree = result;
                for(struct ares_srv_reply *entry = result; entry;
                    entry = entry->next)
                {
                    duk_push_object(mLow->duk_ctx);
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_string(mLow->duk_ctx, "SRV");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                    }
                    duk_push_string(mLow->duk_ctx, entry->host);
                    duk_put_prop_string(mLow->duk_ctx, -2, "name");
                    duk_push_int(mLow->duk_ctx, entry->priority);
                    duk_put_prop_string(mLow->duk_ctx, -2, "priority");
                    duk_push_int(mLow->duk_ctx, entry->weight);
                    duk_put_prop_string(mLow->duk_ctx, -2, "weight");
                    duk_push_int(mLow->duk_ctx, entry->port);
                    duk_put_prop_string(mLow->duk_ctx, -2, "port");
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);
                }
                mAresFree = NULL;
                ares_free_data(result);
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_srv_reply");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        if(mDNSType == ns_t_txt || mDNSType == ns_t_any)
        {
            struct ares_txt_ext *result;
            int status = ares_parse_txt_reply_ext(mData, mLen, &result);
            if(status == ARES_SUCCESS)
            {
                if(result)
                {
                    mAresFree = result;
                    if(mDNSType == ns_t_any)
                    {
                        duk_push_object(mLow->duk_ctx);
                        duk_push_string(mLow->duk_ctx, "TXT");
                        duk_put_prop_string(mLow->duk_ctx, -2, "type");
                    }
                    int arr_len2 = 0;
                    duk_push_array(mLow->duk_ctx);

                    for(struct ares_txt_ext *entry = result; entry;
                        entry = entry->next)
                    {
                        if(entry->record_start && arr_len2)
                        {
                            if(mDNSType == ns_t_any)
                                duk_put_prop_string(mLow->duk_ctx, -2,
                                                    "entries");
                            duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);

                            if(mDNSType == ns_t_any)
                            {
                                duk_push_object(mLow->duk_ctx);
                                duk_push_string(mLow->duk_ctx, "TXT");
                                duk_put_prop_string(mLow->duk_ctx, -2, "type");
                            }
                            arr_len2 = 0;
                            duk_push_array(mLow->duk_ctx);
                        }
                        duk_push_lstring(mLow->duk_ctx, (char *)entry->txt,
                                         entry->length);
                        duk_put_prop_index(mLow->duk_ctx, -2, arr_len2++);
                    }

                    if(mDNSType == ns_t_any)
                        duk_put_prop_string(mLow->duk_ctx, -2, "entries");
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_len++);

                    mAresFree = NULL;
                    ares_free_data(result);
                }
            }
            else if(status != ARES_ENODATA || mDNSType != ns_t_any)
            {
                low_push_stash(mLow->duk_ctx, mCallID, false);
                low_push_error(mLow->duk_ctx, LowDNSResolver::AresErr(status),
                               "ares_parse_txt_reply_ext");
                duk_call(mLow->duk_ctx, 1);
                return false;
            }
        }
        duk_call(mLow->duk_ctx, 2);
    }

    return false;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_GetHostByAddr::LowDNSResolver_GetHostByAddr
// -----------------------------------------------------------------------------

LowDNSResolver_GetHostByAddr::LowDNSResolver_GetHostByAddr(
    LowDNSResolver *resolver)
    : LowLoopCallback(resolver->mLow), mResolver(resolver),
      mLow(resolver->mLow), mChannel(resolver->mChannel), mRefID(0), mCallID(0),
      mResult(NULL)
{
    resolver->mActiveQueries++;
    mLow->run_ref++;
    mLow->resolvers_active++;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_GetHostByAddr::~LowDNSResolver_GetHostByAddr
// -----------------------------------------------------------------------------

LowDNSResolver_GetHostByAddr::~LowDNSResolver_GetHostByAddr()
{
    if(mResult)
    {
        for(int i = 0; mResult[i]; i++)
            low_free(mResult[i]);
        low_free(mResult);
        mResult = NULL;
    }

    mResolver->mActiveQueries--;
    mLow->run_ref--;
    mLow->resolvers_active--;

    if(mRefID)
        low_remove_stash(mLow->duk_ctx, mRefID);
    if(mCallID)
        low_remove_stash(mLow->duk_ctx, mCallID);
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_GetHostByAddr::Resolve
// -----------------------------------------------------------------------------

int LowDNSResolver_GetHostByAddr::Resolve(const char *hostname, int refIndex,
                                          int callIndex)
{
    unsigned char addr[16];
    int addrLen;

    int family = AF_INET6;
    for(int i = 0; hostname[i]; i++)
    {
        if(hostname[i] == '.')
        {
            family = AF_INET;
            break;
        }
    }

    if(inet_pton(family, hostname, addr) != 1)
        return errno;

    mRefID = low_add_stash(mLow->duk_ctx, refIndex); // put Resolver object on stash so
                                            // it does not get garbage collected
    mCallID = low_add_stash(mLow->duk_ctx, callIndex);

    ares_gethostbyaddr(mChannel, addr, family == AF_INET ? 4 : 16, family,
                       CallbackStatic, this);
    // Make web thread start checking c-ares FDs
    low_web_thread_break(mLow);

    return 0;
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_GetHostByAddr::Callback
// -----------------------------------------------------------------------------

void LowDNSResolver_GetHostByAddr::Callback(int status, int timeouts,
                                            struct hostent *hostent)
{
    if(status != ARES_SUCCESS || !hostent)
    {
        mError = LowDNSResolver::AresErr(status);
        mSyscall = "ares_gethostbyaddr";
        low_loop_set_callback(mLow, this);
        return;
    }
    mError = 0;

    int i;
    for(i = 1; hostent->h_aliases && hostent->h_aliases[i - 1]; i++)
    {
    }
    mResult = (char **)low_alloc(sizeof(char *) * (i + 1));
    if(!mResult)
    {
        mError = LOW_ENOMEM;
        mSyscall = "malloc";
        low_loop_set_callback(mLow, this);
        return;
    }
    mResult[0] = low_strdup(hostent->h_name);
    if(!mResult[0])
    {
        low_free(mResult);
        mResult = NULL;

        mError = LOW_ENOMEM;
        mSyscall = "malloc";
        low_loop_set_callback(mLow, this);
        return;
    }
    for(i = 1; hostent->h_aliases && hostent->h_aliases[i - 1]; i++)
    {
        mResult[i] = low_strdup(hostent->h_aliases[i - 1]);
        if(!mResult[i])
        {
            for(int j = 0; j < i; j++)
                low_free(mResult[j]);
            low_free(mResult);
            mResult = NULL;

            mError = LOW_ENOMEM;
            mSyscall = "malloc";
            low_loop_set_callback(mLow, this);
            return;
        }
    }
    mResult[i] = NULL;

    low_loop_set_callback(mLow, this);
}

// -----------------------------------------------------------------------------
//  LowDNSResolver_GetHostByAddr::OnLoop
// -----------------------------------------------------------------------------

bool LowDNSResolver_GetHostByAddr::OnLoop()
{
    if(mError)
    {
        low_push_stash(mLow->duk_ctx, mCallID, true);
        low_push_error(mLow->duk_ctx, mError, mSyscall);
        duk_call(mLow->duk_ctx, 1);
    }
    else
    {
        low_push_stash(mLow->duk_ctx, mCallID, false);
        duk_push_null(mLow->duk_ctx);
        duk_push_array(mLow->duk_ctx);
        for(int i = 0; mResult[i]; i++)
        {
            duk_push_string(mLow->duk_ctx, mResult[i]);
            duk_put_prop_index(mLow->duk_ctx, -2, i);
        }
        duk_call(mLow->duk_ctx, 2);
    }

    return false;
}

#endif /* LOW_INCLUDE_CARES_RESOLVER */
