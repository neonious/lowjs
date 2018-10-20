// -----------------------------------------------------------------------------
//  LowNFD.h
// -----------------------------------------------------------------------------

#ifndef __LOWNFD_H__
#define __LOWNFD_H__


#include "LowDataCallback.h"
#include "LowLoopCallback.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct low_main_t;

enum
{

    LOWNFD_PHASE_RENAME
};

class LowNFD : public LowDataCallback, public LowLoopCallback
{
  public:
    LowNFD(low_main_t *low, const char *old_name, const char *new_name, int callIndex);
    virtual ~LowNFD();


  protected:
    virtual bool OnData();
    virtual bool OnLoop();

  private:
    low_main_t *mLow;
    char *mold_name;
    char *mnew_name;

    int mCallID;

    int mPhase, mError;
    const char *mSyscall;
    bool mDataDone;
};

#endif /* __LOWFILE_H__ */