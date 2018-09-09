// -----------------------------------------------------------------------------
//  low_net.h
// -----------------------------------------------------------------------------

#ifndef __LOW_NET_H__
#define __LOW_NET_H__

#include "duktape.h"

duk_ret_t low_net_listen(duk_context *ctx);
duk_ret_t low_net_connect(duk_context *ctx);
duk_ret_t low_net_setsockopt(duk_context *ctx);
duk_ret_t low_net_shutdown(duk_context *ctx);

#endif /* __LOW_NET_H__ */