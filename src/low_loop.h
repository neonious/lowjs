// -----------------------------------------------------------------------------
//  low_loop.h
// -----------------------------------------------------------------------------

#ifndef __LOW_LOOP_H__
#define __LOW_LOOP_H__

#include "duktape.h"

struct low_main_t;

struct low_chore_t
{
  int stamp, interval;
  bool oneshot, ref;
};

class LowLoopCallback;

extern "C" bool low_loop_run(low_main_t *low);

duk_ret_t low_loop_call_chore_safe(duk_context *ctx, void *udata);

duk_ret_t low_loop_call_callback_safe(duk_context *ctx, void *udata);

duk_ret_t low_loop_chore_ref(duk_context *ctx);

duk_ret_t low_loop_run_ref(duk_context *ctx);

duk_ret_t low_loop_set_chore(duk_context *ctx);

duk_ret_t low_loop_clear_chore(duk_context *ctx);

void low_loop_set_callback(low_main_t *low, LowLoopCallback *callback); // may be called from other thread
void low_loop_clear_callback(low_main_t *low, LowLoopCallback *callback); // must be called from main thread

#endif /* __LOW_LOOP_H__ */