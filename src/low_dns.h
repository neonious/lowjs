// -----------------------------------------------------------------------------
//  low_dns.h
// -----------------------------------------------------------------------------

#ifndef __LOW_DNS_H__
#define __LOW_DNS_H__

#include "duktape.h"

duk_ret_t low_dns_lookup(duk_context *ctx);
duk_ret_t low_dns_lookup_service(duk_context *ctx);

duk_ret_t low_dns_new_resolver(duk_context *ctx);
duk_ret_t low_dns_resolver_cancel(duk_context *ctx);
duk_ret_t low_dns_resolver_get_servers(duk_context *ctx);
duk_ret_t low_dns_resolver_set_servers(duk_context *ctx);
duk_ret_t low_dns_resolver_resolve(duk_context *ctx);
duk_ret_t low_dns_resolver_gethostbyaddr(duk_context *ctx);

duk_ret_t low_dns_resolver_finalizer(duk_context *ctx);

#endif /* __LOW_DNS_H__ */