// -----------------------------------------------------------------------------
//  low_dns.cpp
// -----------------------------------------------------------------------------

#include "low_dns.h"
#include "LowDNSWorker.h"

#include "low_alloc.h"
#include "low_main.h"
#include "low_system.h"

#include "low_config.h"
#if LOW_INCLUDE_CARES_RESOLVER
#include "LowDNSResolver.h"
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

// -----------------------------------------------------------------------------
//  low_dns_lookup
// -----------------------------------------------------------------------------

duk_ret_t low_dns_lookup(duk_context *ctx)
{
    const char *address = duk_require_string(ctx, 0);
    int family = duk_require_int(ctx, 1);
    int hints = duk_require_int(ctx, 2);

    switch(family)
    {
        case 4:
            family = AF_INET;
            break;
        case 6:
            family = AF_INET6;
            break;
        case 0:
            family = AF_UNSPEC;
            break;
        default:
            duk_type_error(ctx, "unknown family");
            return 0;
    }
    hints = (hints & 1) ? AI_ADDRCONFIG : 0 | (hints & 2) ? AI_V4MAPPED : 0;

    LowDNSWorker *worker = new(low_new) LowDNSWorker(duk_get_low_context(ctx));
    if(!worker->Lookup(address, family, hints, 3))
        delete worker;

    return 0;
}

// -----------------------------------------------------------------------------
//  low_dns_lookup_service
// -----------------------------------------------------------------------------

duk_ret_t low_dns_lookup_service(duk_context *ctx)
{
    const char *address = duk_require_string(ctx, 0);
    int port = duk_require_int(ctx, 1);

    LowDNSWorker *worker = new(low_new) LowDNSWorker(duk_get_low_context(ctx));
    if(!worker->LookupService(address, port, 2))
        delete worker;

    return 0;
}

// -----------------------------------------------------------------------------
//  low_dns_new_resolver
// -----------------------------------------------------------------------------

duk_ret_t low_dns_new_resolver(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);

    LowDNSResolver *resolver = new(low_new) LowDNSResolver(low);
    if(!resolver)
        duk_generic_error(ctx, "out of memory");

    if(!resolver->Init())
    {
        delete resolver;
        duk_throw(ctx);
    }

    duk_push_c_function(ctx, low_dns_resolver_finalizer, 1);
    duk_set_finalizer(ctx, 0);
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */

    return 1;
}

// -----------------------------------------------------------------------------
//  low_dns_resolver_cancel
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_cancel(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);
    int index = duk_require_int(ctx, 0);

    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    pthread_mutex_lock(&low->resolvers_mutex);
    low->resolvers[index]->Cancel();
    pthread_mutex_unlock(&low->resolvers_mutex);
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    return 0;
}

// -----------------------------------------------------------------------------
//  low_dns_resolver_get_servers
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_get_servers(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);
    int index = duk_require_int(ctx, 0);

    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    struct ares_addr_port_node *list, *info;
    int err;

    pthread_mutex_lock(&low->resolvers_mutex);
    list = low->resolvers[index]->GetServers(err);
    pthread_mutex_unlock(&low->resolvers_mutex);

    if(err)
    {
        low_push_error(low, err, "ares_get_servers_ports");
        duk_throw(ctx);
    }

    duk_push_array(ctx);
    int arr_len = 0;

    info = list;
    while(info)
    {
        char host[80] = "[";
        if((info->family == AF_INET || info->family == AF_INET6) &&
           inet_ntop(info->family,
                     info->family == AF_INET ? (void *)&info->addr.addr4
                                             : (void *)&info->addr.addr6,
                     host + 1,
                     64) != NULL)
        {
            int len = strlen(host);
            if(info->udp_port && info->udp_port != 53)
            {
                if(info->family == AF_INET)
                {
                    sprintf(host + len, ":%d", info->udp_port);
                    duk_push_string(ctx, host + 1);
                }
                else
                {
                    sprintf(host + len, "]:%d", info->udp_port);
                    duk_push_string(ctx, host);
                }
            }
            else
                duk_push_string(ctx, host + 1);
            duk_put_prop_index(ctx, -2, arr_len++);

            if(info->tcp_port && info->udp_port &&
               info->tcp_port != info->udp_port)
            {
                if(info->udp_port && info->udp_port != 53)
                {
                    if(info->family == AF_INET)
                    {
                        sprintf(host + len, ":%d", info->udp_port);
                        duk_push_string(ctx, host + 1);
                    }
                    else
                    {
                        sprintf(host + len, "]:%d", info->udp_port);
                        duk_push_string(ctx, host);
                    }
                }
                else
                {
                    host[len] = '\0';
                    duk_push_string(ctx, host + 1);
                }
                duk_put_prop_index(ctx, -2, arr_len++);
            }
        }

        info = info->next;
    }
    ares_free_data(list);
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    return 1;
}

// -----------------------------------------------------------------------------
//  low_dns_resolver_set_servers
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_set_servers(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);
    int index = duk_require_int(ctx, 0);

    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    struct ares_addr_port_node *list = NULL, *info, *next;
    int i, n;

    n = duk_get_length(ctx, 1);
    for(i = 0; i < n; i++)
    {
        duk_get_prop_index(ctx, 1, i);
        duk_get_prop_index(ctx, -1, 0);
        int family = duk_require_int(ctx, -1);
        duk_pop(ctx);
        duk_get_prop_index(ctx, -1, 2);
        int port = duk_require_int(ctx, -1);
        duk_pop(ctx);
        duk_get_prop_index(ctx, -1, 1);
        const char *ip = duk_get_string(ctx, -1);

        next = (ares_addr_port_node *)low_alloc(sizeof(ares_addr_port_node));
        if(!list)
            list = next;
        else
            info->next = next;
        info = next;
        info->next = NULL;

        if(family == 4)
        {
            info->family = AF_INET;
            if(inet_pton(AF_INET, ip, &info->addr.addr4) != 1)
            {
                if(list)
                    ares_free_data(list);
                low_push_error(low, errno, "inet_pton");
                duk_throw(ctx);
            }
        }
        else
        {
            info->family = AF_INET6;
            if(inet_pton(AF_INET6, ip, &info->addr.addr6) != 1)
            {
                if(list)
                    ares_free_data(list);
                low_push_error(low, errno, "inet_pton");
                duk_throw(ctx);
            }
        }
        info->tcp_port = info->udp_port = port;

        duk_pop_2(ctx);
    }

    pthread_mutex_lock(&low->resolvers_mutex);
    int err = low->resolvers[index]->SetServers(list);
    pthread_mutex_unlock(&low->resolvers_mutex);
    if(err)
    {
        low_push_error(low, err, "ares_set_servers_ports");
        duk_throw(ctx);
    }
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    return 0;
}

// -----------------------------------------------------------------------------
//  low_dns_resolver_resolve
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_resolve(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);

    duk_get_prop_string(ctx, 0, "_handle");
    int index = duk_require_int(ctx, -1);

    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    const char *hostname = duk_require_string(ctx, 1);
    const char *type = duk_require_string(ctx, 2);
    bool ttl = duk_require_boolean(ctx, 3);

    pthread_mutex_lock(&low->resolvers_mutex);
    LowDNSResolver_Query *query =
      new(low_new) LowDNSResolver_Query(low->resolvers[index]);
    if(!query)
    {
        pthread_mutex_unlock(&low->resolvers_mutex);

        duk_dup(ctx, 4);
        low_push_error(low, ENOMEM, "malloc");
        duk_call(ctx, 1);
        return 0;
    }

    query->Resolve(hostname, type, ttl, 0, 4);
    pthread_mutex_unlock(&low->resolvers_mutex);
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    return 0;
}

// -----------------------------------------------------------------------------
//  low_dns_resolver_gethostbyaddr
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_gethostbyaddr(duk_context *ctx)
{
#if LOW_INCLUDE_CARES_RESOLVER
    low_main_t *low = duk_get_low_context(ctx);

    duk_get_prop_string(ctx, 0, "_handle");
    int index = duk_require_int(ctx, -1);

    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    const char *hostname = duk_require_string(ctx, 1);

    pthread_mutex_lock(&low->resolvers_mutex);
    LowDNSResolver_GetHostByAddr *query =
      new(low_new) LowDNSResolver_GetHostByAddr(low->resolvers[index]);
    if(!query)
    {
        pthread_mutex_unlock(&low->resolvers_mutex);

        duk_dup(ctx, 2);
        low_push_error(low, ENOMEM, "malloc");
        duk_call(ctx, 1);
        return 0;
    }

    int error = query->Resolve(hostname, 0, 2);
    pthread_mutex_unlock(&low->resolvers_mutex);
    if(error)
    {
        duk_dup(ctx, 2);
        low_push_error(low, error, "inet_pton");
        duk_call(ctx, 1);
        return 0;
    }
#else
    duk_reference_error(ctx,
                        "low.js was compiled without c-ares. Recompile or "
                        "use dns.lookup instead.");
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    return 0;
}

#if LOW_INCLUDE_CARES_RESOLVER

// -----------------------------------------------------------------------------
//  low_dns_resolver_finalizer
// -----------------------------------------------------------------------------

duk_ret_t low_dns_resolver_finalizer(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);

    duk_get_prop_string(ctx, 0, "_handle");
    int index = duk_require_int(ctx, -1);
    if(index < 0 || index >= low->resolvers.size())
        duk_reference_error(ctx, "resolver not found");

    pthread_mutex_lock(&low->resolvers_mutex);
    delete low->resolvers[index];
    pthread_mutex_unlock(&low->resolvers_mutex);

    return 0;
}

#endif /* LOW_INCLUDE_CARES_RESOLVER */
