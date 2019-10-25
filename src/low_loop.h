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
    unsigned char oneshot, ref;

    // If oneshot == 2, C version
    void (*call)(void *data);
    void *data;
};

class LowLoopCallback;

extern "C" bool low_loop_run(low_main_t *low);
duk_ret_t low_loop_run_safe(duk_context *ctx, void *udata);

duk_ret_t low_loop_chore_ref(duk_context *ctx);
duk_ret_t low_loop_run_ref(duk_context *ctx);

duk_ret_t low_loop_set_chore(duk_context *ctx);
duk_ret_t low_loop_clear_chore(duk_context *ctx);

int low_set_timeout(duk_context *ctx, int index, int delay, void (*call)(void *data), void *data);
void low_clear_timeout(duk_context *ctx, int index);

void low_loop_set_callback(
    low_main_t *low,
    LowLoopCallback *callback); // may be called from other thread
void low_loop_clear_callback(
    low_main_t *low,
    LowLoopCallback *callback); // must be called from main thread

void low_call_next_tick(duk_context *ctx, int num_args);
int low_call_next_tick_js(duk_context *ctx);

#endif /* __LOW_LOOP_H__ */