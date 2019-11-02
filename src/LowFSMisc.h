// -----------------------------------------------------------------------------
//  LowFSMisc.h
// -----------------------------------------------------------------------------

#ifndef __LOWFSMisc_H__
#define __LOWFSMisc_H__

#include "LowDataCallback.h"
#include "LowLoopCallback.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

struct low_t;

enum
{

    LOWFSMISC_PHASE_RENAME,
    LOWFSMISC_PHASE_UNLINK,
    LOWFSMISC_PHASE_STAT,
    LOWFSMISC_PHASE_ACCESS,
    LOWFSMISC_PHASE_READDIR,
    LOWFSMISC_PHASE_MKDIR,
    LOWFSMISC_PHASE_RMDIR
};

class LowFSMisc
    : public LowDataCallback
    , public LowLoopCallback
{
  public:
    LowFSMisc(low_t *low);
    virtual ~LowFSMisc();

    void Rename(const char *old_name, const char *new_name);
    void Unlink(const char *file_name);
    void Stat(const char *file_name);

    void Access(const char *file_name, int mode);
    void ReadDir(const char *file_name, bool withFileTypes);
    void MkDir(const char *file_name, bool recursive, int mode);
    void RmDir(const char *file_name);

    void Run(int callIndex = 0);

  protected:
    void ReadDir();

    virtual bool OnData();
    virtual bool OnLoop();

  private:
    low_t *mLow;
    char *mOldName;
    char *mNewName;

    struct stat mStat;
    bool mWithFileTypes, mRecursive;
    int mMode;

    int mCallID;
    pthread_mutex_t mMutex;

    int mPhase, mError;
    char *mFileEntries;
};

#endif /* __LOWFSMISC_H__ */
