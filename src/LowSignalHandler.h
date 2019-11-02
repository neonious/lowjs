// -----------------------------------------------------------------------------
//  LowSignalHandler.h
// -----------------------------------------------------------------------------

#ifndef __LOWSIGNALHANDLER_H__
#define __LOWSIGNALHANDLER_H__

#include "LowLoopCallback.h"

class LowSignalHandler : public LowLoopCallback
{
  public:
    LowSignalHandler(low_t *low, int signal);
    LowSignalHandler(low_t *low, const char *name);

  protected:
    virtual bool OnLoop();

  private:
    low_t *mLow;
    const char *mName;
    int mSignal;
};

#endif /* __LOWSIGNALHANDLER_H__ */