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
#include <sys/types.h>
#include <dirent.h>


// -----------------------------------------------------------------------------
//  LowFSMisc::LowFSMisc
// -----------------------------------------------------------------------------

LowFSMisc::LowFSMisc(low_t *low) :
    LowDataCallback(low), LowLoopCallback(low), mLow(low),
    mOldName(NULL), mNewName(NULL), mCallID(0), mFileEntries(NULL)
{
    pthread_mutex_init(&mMutex, NULL);
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
    while(mFileEntries)
    {
        char *entry = *(char **)mFileEntries;
        free(mFileEntries);
        mFileEntries = entry;
    }

    if(mCallID)
    {
        low_remove_stash(mLow->duk_ctx, mCallID);
        mLow->run_ref--;
    }
    pthread_mutex_destroy(&mMutex);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Rename
// -----------------------------------------------------------------------------

void LowFSMisc::Rename(const char *old_name, const char *new_name)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "rename");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_RENAME;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Unlink
// -----------------------------------------------------------------------------

void LowFSMisc::Unlink(const char *file_name)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "rename");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_UNLINK;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Stat
// -----------------------------------------------------------------------------

void LowFSMisc::Stat(const char *file_name)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "rename");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_STAT;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Access
// -----------------------------------------------------------------------------

void LowFSMisc::Access(const char *file_name, int mode)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "mkdir");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_ACCESS;
    mMode = mode;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::ReadDir
// -----------------------------------------------------------------------------

void LowFSMisc::ReadDir(const char *file_name, bool withFileTypes)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "mkdir");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_READDIR;
    mWithFileTypes = withFileTypes;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::MkDir
// -----------------------------------------------------------------------------

void LowFSMisc::MkDir(const char *file_name, bool recursive, int mode)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "mkdir");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_MKDIR;
    mRecursive = recursive;
    mMode = mode;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::RmDir
// -----------------------------------------------------------------------------

void LowFSMisc::RmDir(const char *file_name)
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
        low_push_error(mLow->duk_ctx, ENOMEM, "rmdir");
        duk_throw(mLow->duk_ctx);

        return;
    }

    mPhase = LOWFSMISC_PHASE_RMDIR;
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Run
// -----------------------------------------------------------------------------

void LowFSMisc::Run(int callIndex)
{
    if(callIndex)
    {
        mCallID = low_add_stash(mLow->duk_ctx, callIndex);
        mLow->run_ref++;
    }
    else
        pthread_mutex_lock(&mMutex);

    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);

    if(!callIndex)
    {
        pthread_mutex_lock(&mMutex);
        OnLoop();
        pthread_mutex_unlock(&mMutex);
    }
}

// -----------------------------------------------------------------------------
//  LowFSMisc::ReadDir
// -----------------------------------------------------------------------------

#if !LOW_ESP32_LWIP_SPECIALITIES
void LowFSMisc::ReadDir()
{
    DIR *dir = opendir(mOldName);
    if(!dir)
    {
        mError = errno;
        return;
    }

    struct dirent dirData, *ent;
    while(true)
    {
        if(readdir_r(dir, &dirData, &ent) != 0 || !ent)
            break;
        if(ent->d_name[0] == '.' &&
            (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;

        int len = strlen(ent->d_name);
        char *entry = (char *)low_alloc(sizeof(char *) + len + 1);
        if(!entry)
        {
            closedir(dir);
            errno = ENOMEM;
            return;
        }

        *(char **)entry = mFileEntries;
        memcpy(entry + sizeof(char *), ent->d_name, len + 1);
        mFileEntries = entry;
    }
    closedir(dir);
}
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

// -----------------------------------------------------------------------------
//  LowFSMisc::OnData
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES
int data_unlink(char *filename, bool recursive, int isDir);
int data_mkdir(char *filename, bool recursive);
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

            low_free(mNewName);
            mNewName = NULL;
            break;

        case LOWFSMISC_PHASE_UNLINK:
            mError = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(data_unlink(mOldName, false, 0) != 0)
                mError = errno;
#else
            if(unlink(mOldName) != 0)
                mError = errno;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            break;

        case LOWFSMISC_PHASE_RMDIR:
            mError = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(data_unlink(mOldName, false, 1) != 0)
                mError = errno;
#else
            if(rmdir(mOldName) != 0)
                mError = errno;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            break;

        case LOWFSMISC_PHASE_READDIR:
            mError = 0;
            ReadDir();
            break;

        case LOWFSMISC_PHASE_MKDIR:
            mError = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(data_mkdir(mOldName, mRecursive) != 0)
                mError = errno;
#else
            if(mRecursive)
            {
                for(int i = 0; mOldName[i]; i++)
                {
                    if(mOldName[i] == '/')
                    {
                        mOldName[i] = '\0';
                        mkdir(mOldName, mMode);
                        mOldName[i] = '/';
                    }
                }
            }
            if(mkdir(mOldName, mMode) != 0)
                mError = errno;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            break;

        case LOWFSMISC_PHASE_STAT:
            mError = 0;
            if(stat(mOldName, &mStat) != 0)
                mError = errno;
            break;

        case LOWFSMISC_PHASE_ACCESS:
            mError = 0;
            if(access(mOldName, mMode) != 0)
                mError = errno;
            break;
    }

    low_free(mOldName);
    mOldName = NULL;

    if(mCallID)
        low_loop_set_callback(mLow, this);
    else
        pthread_mutex_unlock(&mMutex);
    return true;
}

// -----------------------------------------------------------------------------
//  LowFSMisc::OnLoop
// -----------------------------------------------------------------------------

bool LowFSMisc::OnLoop()
{
    bool isAsync = mCallID != 0;
    if(isAsync)
    {
        int callID = mCallID;
        mCallID = 0;
        mLow->run_ref--;

        low_push_stash(mLow->duk_ctx, callID, true);
    }
    if(mError)
    {
        if(mPhase == LOWFSMISC_PHASE_RENAME)
            low_push_error(mLow->duk_ctx, mError, "rename");
        else if(mPhase == LOWFSMISC_PHASE_UNLINK)
            low_push_error(mLow->duk_ctx, mError, "unlink");
        else if(mPhase == LOWFSMISC_PHASE_STAT)
            low_push_error(mLow->duk_ctx, mError, "stat");
        else if(mPhase == LOWFSMISC_PHASE_ACCESS)
            low_push_error(mLow->duk_ctx, mError, "access");
        else if(mPhase == LOWFSMISC_PHASE_READDIR)
            low_push_error(mLow->duk_ctx, mError, "readdir");
        else if(mPhase == LOWFSMISC_PHASE_MKDIR)
            low_push_error(mLow->duk_ctx, mError, "mkdir");
        else if(mPhase == LOWFSMISC_PHASE_RMDIR)
            low_push_error(mLow->duk_ctx, mError, "rmdir");
        else
            low_push_error(mLow->duk_ctx, mError, "stat");

        if(!isAsync)
            duk_throw(mLow->duk_ctx);
    }
    else if(mPhase == LOWFSMISC_PHASE_STAT)
    {
        if(isAsync)
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
        if(isAsync)
            duk_call(mLow->duk_ctx, 2);
        return false;
    }
    else if(mPhase == LOWFSMISC_PHASE_READDIR)
    {
        if(isAsync)
            duk_push_null(mLow->duk_ctx);
        duk_push_array(mLow->duk_ctx);

        int i = 0;
        while(mFileEntries)
        {
            char *entry = *(char **)mFileEntries;

            duk_push_string(mLow->duk_ctx, mFileEntries + sizeof(char *));
            duk_put_prop_index(mLow->duk_ctx, -2, i++);
            mFileEntries = entry;
        }

        if(isAsync)
            duk_call(mLow->duk_ctx, 2);
        return false;
    }
    else if(isAsync)
        duk_push_null(mLow->duk_ctx);
    if(isAsync)
        duk_call(mLow->duk_ctx, 1);

    return false;
}
