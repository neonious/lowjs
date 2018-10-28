// -----------------------------------------------------------------------------
//  LowFSMisc.h
// -----------------------------------------------------------------------------

#ifndef __LOWFSMisc_H__
#define __LOWFSMisc_H__


#include "LowDataCallback.h"
#include "LowLoopCallback.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct low_main_t;

enum
{

    LOWFSMISC_PHASE_RENAME,
    LOWFSMISC_PHASE_UNLINK
};

class LowFSMisc : public LowDataCallback, public LowLoopCallback
{
  public:
    LowFSMisc(low_main_t *low);
    virtual ~LowFSMisc();
    void ReName( const char *old_name, const char *new_name, int callIndex);
    void UnLink( const char *file_name, int callIndex);


  protected:
    virtual bool OnData();
    virtual bool OnLoop();

  private:
    low_main_t *mLow;
    char *mOldName;
    char *mNewName;

    int mCallID;

    int mPhase, mError;
    const char *mSyscall;
    bool mDataDone;
};

#endif /* __LOWFILE_H__ */