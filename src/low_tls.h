// -----------------------------------------------------------------------------
//  low_tls.h
// -----------------------------------------------------------------------------

#ifndef __LOW_TLS_H__
#define __LOW_TLS_H__

#include "duktape.h"

duk_ret_t low_tls_create_context(duk_context *ctx);

duk_ret_t low_tls_context_finalizer(duk_context *ctx);

#endif /* __LOW_TLS_H__ */