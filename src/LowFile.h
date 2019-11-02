// -----------------------------------------------------------------------------
//  LowFile.h
// -----------------------------------------------------------------------------

#ifndef __LOWFILE_H__
#define __LOWFILE_H__

#include "LowDataCallback.h"
#include "LowFD.h"
#include "LowLoopCallback.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct low_t;

enum
{
    LOWFILE_PHASE_OPENING,
    LOWFILE_PHASE_READY,
    LOWFILE_PHASE_READING,
    LOWFILE_PHASE_WRITING,
    LOWFILE_PHASE_FSTAT,
    LOWFILE_PHASE_CLOSING
};

class LowFile
    : public LowFD
    , public LowDataCallback
    , public LowLoopCallback
{
  public:
    LowFile(low_t *low, const char *path, int flags, int callIndex);
    virtual ~LowFile();

    void Read(int pos, unsigned char *data, int len, int callIndex);
    void Write(int pos, unsigned char *data, int len, int callIndex);
    void FStat(int callIndex);
    bool Close(int callIndex);

    bool FinishPhase();

  protected:
    virtual bool OnData();
    virtual bool OnLoop();

  private:
    low_t *mLow;
    char *mPath;
    int mFlags;

    unsigned char *mData;
    int mPos, mLen;
    struct stat mStat;
    int mCallID;

    int mPhase, mError;
    const char *mSyscall;
    bool mDataDone, mClose;
};

#endif /* __LOWFILE_H__ */