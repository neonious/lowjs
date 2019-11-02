// -----------------------------------------------------------------------------
//  LowFile.cpp
// -----------------------------------------------------------------------------

#include "LowFile.h"

#include "low_data_thread.h"
#include "low_fs.h"
#include "low_loop.h"
#include "low_main.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_system.h"

#include "duktape.h"

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>

// -----------------------------------------------------------------------------
//  LowFile::LowFile
// -----------------------------------------------------------------------------

LowFile::LowFile(low_t *low, const char *path, int flags, int callID) :
    LowFD(low, LOWFD_TYPE_FILE), LowDataCallback(low), LowLoopCallback(low),
    mLow(low), mClose(false)
{
    if(callID)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callID);
        if(mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(path) + strlen(low->cwd);

    mPath = (char *)low_alloc(len);
    if(mPath)
        if(!low_fs_resolve(mPath, len, low->cwd, path))
        {
            low_free(mPath);
            mPath = NULL;

            duk_generic_error(mLow->duk_ctx, "fs resolve error");
        }
#else
    mPath = low_strdup(path);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    mFlags = flags;
    mPhase = LOWFILE_PHASE_OPENING;
    mDataDone = false;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);
}

// -----------------------------------------------------------------------------
//  LowFile::~LowFile
// -----------------------------------------------------------------------------

LowFile::~LowFile()
{
    low_data_clear_callback(mLow, this);

    if(FD() >= 0)
        close(FD());
    if(mPath)
        low_free(mPath);
    if(mCallID)
    {
        low_remove_stash(mLow->duk_ctx, mCallID);
        mLow->run_ref--;
    }
}

// -----------------------------------------------------------------------------
//  LowFile::Read
// -----------------------------------------------------------------------------

void LowFile::Read(int pos, unsigned char *data, int len, int callIndex)
{
    if(mClose)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EBADF, "read");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }
    if(mPhase != LOWFILE_PHASE_READY)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EALREADY, "read");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }

    if(callIndex != -1)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callIndex);
        if(mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    mPos = pos;
    mData = data;
    mLen = len;
    mPhase = LOWFILE_PHASE_READING;
    mDataDone = false;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);
}

// -----------------------------------------------------------------------------
//  LowFile::Write
// -----------------------------------------------------------------------------

void LowFile::Write(int pos, unsigned char *data, int len, int callIndex)
{
    if(mClose)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EBADF, "write");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }
    if(mPhase != LOWFILE_PHASE_READY)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EALREADY, "write");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }

    if(callIndex != -1)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callIndex);
        if(mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    mPos = pos;
    mData = data;
    mLen = len;
    mPhase = LOWFILE_PHASE_WRITING;
    mDataDone = false;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
}

// -----------------------------------------------------------------------------
//  LowFile::FStat
// -----------------------------------------------------------------------------

void LowFile::FStat(int callIndex)
{
    if(mClose)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EBADF, "fstat");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }
    if(mPhase != LOWFILE_PHASE_READY)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EALREADY, "fstat");
        low_call_next_tick(mLow->duk_ctx, 1);
        return;
    }

    if(callIndex != -1)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callIndex);
        if(mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    mPhase = LOWFILE_PHASE_FSTAT;
    mDataDone = false;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);
}

// -----------------------------------------------------------------------------
//  LowFile::Close
// -----------------------------------------------------------------------------

bool LowFile::Close(int callIndex)
{
    if(mClose)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EBADF, "close");
        low_call_next_tick(mLow->duk_ctx, 1);
        return true;
    }
    if(mPhase != LOWFILE_PHASE_READY)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow->duk_ctx, EALREADY, "close");
        low_call_next_tick(mLow->duk_ctx, 1);
        return true;
    }

    // We must no longer advertise, so we do not remove other persons FD later
    // on
    int fd = FD();
    SetFD(-1);
    AdvertiseFD();
    SetFD(fd);

    if(callIndex != -1)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callIndex);
        if(mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    mPhase = LOWFILE_PHASE_CLOSING;
    mDataDone = false;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);

    return true;
}

// -----------------------------------------------------------------------------
//  LowFile::OnData
// -----------------------------------------------------------------------------

bool LowFile::OnData()
{
    switch(mPhase)
    {
        case LOWFILE_PHASE_OPENING:
            SetFD(open(mPath, mFlags, 0666));
            mError = FD() < 0 ? errno : 0;
            mSyscall = "open";

            low_free(mPath);
            mPath = NULL;

            mDataDone = true;
            low_loop_set_callback(mLow, this);
            break;

        case LOWFILE_PHASE_READING:
            mError = 0;
            if(mPos != -1)
            {
                if(lseek(FD(), mPos, SEEK_SET) != mPos)
                {
                    mError = errno;
                    mSyscall = "lseek";
                }
            }
            if(!mError)
            {
                mLen = read(FD(), mData, mLen);
                if(mLen < 0)
                {
                    mError = errno;
                    mSyscall = "read";
                }
            }

            mDataDone = true;
            low_loop_set_callback(mLow, this);
            break;

        case LOWFILE_PHASE_WRITING:
            mError = 0;
            if(mPos != -1)
            {
                if(lseek(FD(), mPos, SEEK_SET) != mPos)
                {
                    mError = errno;
                    mSyscall = "lseek";
                }
            }
            if(!mError)
            {
                mLen = write(FD(), mData, mLen);
                if(mLen < 0)
                {
                    mError = errno;
                    mSyscall = "write";
                }
            }

            mDataDone = true;
            low_loop_set_callback(mLow, this);
            break;

        case LOWFILE_PHASE_FSTAT:
            mError = fstat(FD(), &mStat) < 0 ? errno : 0;
            mSyscall = "fstat";

            mDataDone = true;
            low_loop_set_callback(mLow, this);
            break;

        case LOWFILE_PHASE_CLOSING:
            if(FD() >= 0)
            {
                mError = close(FD()) < 0 ? errno : 0;
                mSyscall = "close";
            }
            SetFD(-1);

            mDataDone = true;
            low_loop_set_callback(mLow, this);
            break;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  LowFile::OnLoop
// -----------------------------------------------------------------------------

bool LowFile::OnLoop()
{
    if(mClose)
        return false;

    bool potentiallyClose = mPhase == LOWFILE_PHASE_CLOSING;
    if(FinishPhase())
        return !potentiallyClose;

    return true;
}

// -----------------------------------------------------------------------------
//  LowFile::FinishPhase
// -----------------------------------------------------------------------------

bool LowFile::FinishPhase()
{
    duk_context *ctx = mLow->duk_ctx;

    if(mPhase == LOWFILE_PHASE_READY)
        return true;
    if(!mDataDone)
        return false;

    int phase = mPhase;
    mPhase = LOWFILE_PHASE_READY;

    if(phase == LOWFILE_PHASE_OPENING && !mError)
        AdvertiseFD();
    if((phase == LOWFILE_PHASE_OPENING && mError) ||
       phase == LOWFILE_PHASE_CLOSING)
        mClose = true;

    if(mCallID)
    {
        int callID = mCallID;
        mCallID = 0;
        mLow->run_ref--;

        switch(phase)
        {
            case LOWFILE_PHASE_OPENING:
                if(mError)
                {
                    low_push_stash(ctx, callID, true);
                    low_push_error(mLow->duk_ctx, mError, mSyscall);
                    duk_call(ctx, 1);
                }
                else
                {
                    low_push_stash(ctx, callID, true);
                    duk_push_null(ctx);
                    duk_push_int(ctx, FD());
                    duk_call(ctx, 2);
                }
                break;

            case LOWFILE_PHASE_READING:
            case LOWFILE_PHASE_WRITING:
                low_push_stash(ctx, callID, true);
                if(mError)
                {
                    low_push_error(mLow->duk_ctx, mError, mSyscall);
                    duk_call(ctx, 1);
                }
                else
                {
                    duk_push_null(ctx);
                    duk_push_int(ctx, mLen);
                    duk_call(ctx, 2);
                }
                break;

            case LOWFILE_PHASE_FSTAT:
                low_push_stash(ctx, callID, true);
                if(mError)
                {
                    low_push_error(mLow->duk_ctx, mError, mSyscall);
                    duk_call(ctx, 1);
                }
                else
                {
                    duk_push_null(ctx);
                    duk_push_object(ctx);

#define applyStat(name) {#name, (double)mStat.st_##name}
                    duk_number_list_entry numberList[] = {
                      applyStat(dev),
                      applyStat(ino),
                      applyStat(mode),
                      applyStat(nlink),
                      applyStat(uid),
                      applyStat(gid),
                      applyStat(rdev),
                      applyStat(blksize),
                      applyStat(blocks),
                      applyStat(size),
                      {"atimeMs", mStat.st_atime * 1000.0},
                      {"mtimeMs", mStat.st_mtime * 1000.0},
                      {"ctimeMs", mStat.st_ctime * 1000.0},
                      {NULL, 0.0}};

                    duk_put_number_list(ctx, -1, numberList);
                    duk_call(ctx, 2);
                }
                break;

            case LOWFILE_PHASE_CLOSING:
                low_push_stash(ctx, callID, true);
                if(mError)
                    low_push_error(mLow->duk_ctx, mError, mSyscall);
                else
                    duk_push_null(ctx);
                duk_call(ctx, 1);
                break;
        }
    }
    else if(mError)
    {
        low_push_error(mLow->duk_ctx, mError, mSyscall);
        duk_throw(ctx);
    }

    return true;
}
