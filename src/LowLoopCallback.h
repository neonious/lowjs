// -----------------------------------------------------------------------------
//  LowLoopCallback.h
// -----------------------------------------------------------------------------

#ifndef __LOWLOOPCALLBACK_H__
#define __LOWLOOPCALLBACK_H__

#include "low_loop.h"

class LowLoopCallback
{
    friend duk_ret_t low_loop_run_safe(duk_context *ctx, void *udata);
    friend bool low_reset(low_t *low);
    friend void low_loop_set_callback(low_t *low,
                                      LowLoopCallback *callback);
    friend void low_loop_clear_callback(low_t *low,
                                        LowLoopCallback *callback);
    friend duk_ret_t low_fs_open_sync(duk_context *ctx);
    friend duk_ret_t low_fs_close_sync(duk_context *ctx);
    friend duk_ret_t low_fs_waitdone(duk_context *ctx);

  public:
    LowLoopCallback(low_t *low)
        : mLow(low), mNext(nullptr), mLoopClearOnReset(true)
    {
    }
    virtual ~LowLoopCallback() { low_loop_clear_callback(mLow, this); }

  protected:
    virtual bool OnLoop() = 0;

  private:
    low_t *mLow;
    LowLoopCallback *mNext;

  protected:
    bool mLoopClearOnReset;
};

#endif /* __LOWLOOPCALLBACK_H__ */
