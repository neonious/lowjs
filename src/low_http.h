
// -----------------------------------------------------------------------------
//  low_http.h
// -----------------------------------------------------------------------------

#ifndef __LOW_HTTP_H__
#define __LOW_HTTP_H__

#include "duktape.h"

duk_ret_t low_http_get_request(duk_context *ctx);

duk_ret_t low_http_read(duk_context *ctx);

duk_ret_t low_http_write(duk_context *ctx);
duk_ret_t low_http_write_head(duk_context *ctx);

#endif /* __LOW_HTTP_H__ */