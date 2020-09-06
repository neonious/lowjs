// -----------------------------------------------------------------------------
//  low_promise.h
// -----------------------------------------------------------------------------

#ifndef __LOW_PROMISE_H__
#define __LOW_PROMISE_H__

#include "duktape.h"
#include "low_main.h"

bool low_register_promise(low_t *low);

int promise_constructor(duk_context *ctx);

int promise_param_resolve(duk_context *ctx);
int promise_param_reject(duk_context *ctx);
int promise_handle_thens(duk_context *ctx);

int promise_all(duk_context *ctx);
int promise_all_resolved(duk_context *ctx);
int promise_all_rejected(duk_context *ctx);
int promise_race(duk_context *ctx);

int promise_resolve(duk_context *ctx);
int promise_reject(duk_context *ctx);

int promise_catch(duk_context *ctx);
int promise_then(duk_context *ctx);
void promise_then_catch(duk_context *ctx, bool isCatch);

#endif /* __LOW_PROMISE_H__ */
