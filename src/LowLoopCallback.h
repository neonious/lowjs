// -----------------------------------------------------------------------------
//  LowLoopCallback.h
// -----------------------------------------------------------------------------

#ifndef __LOWLOOPCALLBACK_H__
#define __LOWLOOPCALLBACK_H__

#include "low_loop.h"

class LowLoopCallback
{
  friend bool low_loop_run(low_main_t *low);

  friend bool low_reset(low_main_t *low);

  friend duk_ret_t low_loop_call_callback_safe(duk_context *ctx, void *udata);

  friend void low_loop_set_callback(low_main_t *low, LowLoopCallback *callback);

  friend void low_loop_clear_callback(low_main_t *low, LowLoopCallback *callback);

  friend duk_ret_t low_fs_open_sync(duk_context *ctx);

  friend duk_ret_t low_fs_close_sync(duk_context *ctx);

  friend duk_ret_t low_fs_waitdone(duk_context *ctx);

public:
  LowLoopCallback(low_main_t *low) : mLow(low), mNext(nullptr), mLoopClearOnReset(true)
  {
  }

  virtual ~LowLoopCallback()
  { low_loop_clear_callback(mLow, this); }

protected:
  virtual bool OnLoop() = 0;

private:
  low_main_t *mLow;
  LowLoopCallback *mNext;

protected:
  bool mLoopClearOnReset;
};

#endif /* __LOWLOOPCALLBACK_H__ */
