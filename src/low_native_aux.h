
// -----------------------------------------------------------------------------
//  low_native_aux.cpp
// -----------------------------------------------------------------------------

#ifndef __LOW_NATIVE_AUX_H__
#define __LOW_NATIVE_AUX_H__

#include "duktape.h"

duk_ret_t low_compare(duk_context *ctx);
duk_ret_t low_is_ip(duk_context *ctx);

duk_ret_t low_compile(duk_context *ctx);
duk_ret_t low_run_in_context(duk_context *ctx);

#endif /* __LOW_NATIVE_AUX_H__ */