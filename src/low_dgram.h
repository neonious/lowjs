// -----------------------------------------------------------------------------
//  low_dgram.h
// -----------------------------------------------------------------------------

#ifndef __LOW_DGRAM_H__
#define __LOW_DGRAM_H__

#include "duktape.h"

duk_ret_t low_dgram_bind(duk_context *ctx);
duk_ret_t low_dgram_send(duk_context *ctx);

#endif /* __LOW_DGRAM_H__ */