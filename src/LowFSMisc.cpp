// -----------------------------------------------------------------------------
//  LowFSMisc.cpp
// -----------------------------------------------------------------------------


#include "LowFSMisc.h"
#include "low_alloc.h"
#include "low_config.h"
#include "low_data_thread.h"
#include "low_fs.h"
#include "low_loop.h"
#include "low_main.h"
#include "low_system.h"

#include "duktape.h"

#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>


// -----------------------------------------------------------------------------
//  LowFSMisc::LowFSMisc
// -----------------------------------------------------------------------------

LowFSMisc::LowFSMisc(low_main_t *low) :
    LowDataCallback(low), LowLoopCallback(low), mLow(low), mCallID(0),
    mOldName(NULL), mNewName(NULL)
{
}


// -----------------------------------------------------------------------------
//  LowFSMisc::~LowFSMisc
// -----------------------------------------------------------------------------

LowFSMisc::~LowFSMisc()
{
    low_data_clear_callback(mLow, this);

    if(mOldName)
        low_free(mOldName);
    if(mNewName)
        low_free(mNewName);
    if(mCallID)
    {
        low_remove_stash(mLow, mCallID);
        mLow->run_ref--;
    }
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Rename
// -----------------------------------------------------------------------------

void LowFSMisc::Rename(const char *old_name, const char *new_name, int callID)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(old_name) + strlen(mLow->cwd);

    mOldName = (char *)low_alloc(len);
    if(mOldName)
        if(!low_fs_resolve(mOldName, len, mLow->cwd, old_name))
        {
            low_free(mOldName);
            mOldName = NULL;

            duk_generic_error(mLow->duk_ctx, "fs resolve error!");
        }

    len = 32 + strlen(new_name) + strlen(mLow->cwd);

    mNewName = (char *)low_alloc(len);
    if(mNewName)
        if(!low_fs_resolve(mNewName, len, mLow->cwd, new_name))
        {
            low_free(mNewName);
            mNewName = NULL;

            duk_generic_error(mLow->duk_ctx, "fs resolve error");
        }
#else
    mOldName = low_strdup(old_name);
    mNewName = low_strdup(new_name);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(!mOldName || !mNewName)
    {
        duk_dup(mLow->duk_ctx, callID);
        low_push_error(mLow, ENOMEM, "rename");
        duk_call(mLow->duk_ctx, 1);

        return;
    }

    mCallID = low_add_stash(mLow, callID);
    mLow->run_ref++;

    mPhase = LOWFSMISC_PHASE_RENAME;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Unlink
// -----------------------------------------------------------------------------

void LowFSMisc::Unlink(const char *file_name, int callID)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(file_name) + strlen(mLow->cwd);

    mOldName = (char *)low_alloc(len);
    if(mOldName)
        if(!low_fs_resolve(mOldName, len, mLow->cwd, file_name))
        {
            low_free(mOldName);
            mOldName = NULL;

            duk_generic_error(mLow->duk_ctx, "fs resolve error");
        }
#else
    mOldName = low_strdup(file_name);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(!mOldName)
    {
        duk_dup(mLow->duk_ctx, callID);
        low_push_error(mLow, ENOMEM, "rename");
        duk_call(mLow->duk_ctx, 1);

        return;
    }

    mCallID = low_add_stash(mLow, callID);
    mLow->run_ref++;

    mPhase = LOWFSMISC_PHASE_UNLINK;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Stat
// -----------------------------------------------------------------------------

void LowFSMisc::Stat(const char *file_name, int callID)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(file_name) + strlen(mLow->cwd);

    mOldName = (char *)low_alloc(len);
    if(mOldName)
        if(!low_fs_resolve(mOldName, len, mLow->cwd, file_name))
        {
            low_free(mOldName);
            mOldName = NULL;

            duk_generic_error(mLow->duk_ctx, "fs resolve error");
        }
#else
    mOldName = low_strdup(file_name);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(!mOldName)
    {
        duk_dup(mLow->duk_ctx, callID);
        low_push_error(mLow, ENOMEM, "rename");
        duk_call(mLow->duk_ctx, 1);

        return;
    }

    mCallID = low_add_stash(mLow, callID);
    mLow->run_ref++;

    mPhase = LOWFSMISC_PHASE_STAT;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::OnData
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES
int data_unlink(char *filename);
int data_rename(char *file_old, char *file_new, bool copy, bool overwrite);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

bool LowFSMisc::OnData()
{
    switch(mPhase)
    {
        case LOWFSMISC_PHASE_RENAME:
            mError = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(data_rename(mOldName, mNewName, false, true) != 0)
                mError = errno;
#else
            if(rename(mOldName, mNewName) != 0)
                mError = errno;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            low_free(mOldName);
            mOldName = NULL;

            low_free(mNewName);
            mNewName = NULL;

            low_loop_set_callback(mLow, this);
            break;

        case LOWFSMISC_PHASE_UNLINK:
            mError = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(data_unlink(mOldName) != 0)
                mError = errno;
#else
            if(unlink(mOldName) != 0)
                mError = errno;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            low_free(mOldName);
            mOldName = NULL;

            low_loop_set_callback(mLow, this);
            break;

        case LOWFSMISC_PHASE_STAT:
            mError = 0;
            if(stat(mOldName, &mStat) != 0)
                mError = errno;

            low_free(mOldName);
            mOldName = NULL;

            low_loop_set_callback(mLow, this);
            break;
    }
    return true;
}

// -----------------------------------------------------------------------------
//  LowFSMisc::OnLoop
// -----------------------------------------------------------------------------

bool LowFSMisc::OnLoop()
{
    int callID = mCallID;
    mCallID = 0;
    mLow->run_ref--;

    low_push_stash(mLow, callID, true);
    if(mError)
    {
        if(mPhase == LOWFSMISC_PHASE_RENAME)
            low_push_error(mLow, mError, "rename");
        else if(mPhase == LOWFSMISC_PHASE_UNLINK)
            low_push_error(mLow, mError, "unlink");
        else
            low_push_error(mLow, mError, "stat");
    }
    else if(mPhase == LOWFSMISC_PHASE_STAT)
    {
        duk_push_null(mLow->duk_ctx);
        duk_push_object(mLow->duk_ctx);

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

        duk_put_number_list(mLow->duk_ctx, -1, numberList);
        duk_call(mLow->duk_ctx, 2);
    }
    else
        duk_push_null(mLow->duk_ctx);
    duk_call(mLow->duk_ctx, 1);

    return false;
}
