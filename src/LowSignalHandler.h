// -----------------------------------------------------------------------------
//  LowSignalHandler.h
// -----------------------------------------------------------------------------

#ifndef __LOWSIGNALHANDLER_H__
#define __LOWSIGNALHANDLER_H__

#include "LowLoopCallback.h"

class LowSignalHandler : public LowLoopCallback
{
public:
  LowSignalHandler(low_main_t *low, int signal);

protected:
  virtual bool OnLoop();

private:
  low_main_t *mLow;
  int mSignal;
};

#endif /* __LOWSIGNALHANDLER_H__ */