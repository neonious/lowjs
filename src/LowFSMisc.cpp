// -----------------------------------------------------------------------------
//  LowFSMisc.cpp
// -----------------------------------------------------------------------------


#include "LowFSMisc.h"
#include "low_data_thread.h"
#include "low_loop.h"
#include "low_main.h"

#include "low_alloc.h"
#include "low_config.h"
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
    mCallID = low_add_stash(mLow, callID);
    mLow->run_ref++;

    mOldName = low_strdup(old_name);
    mNewName = low_strdup(new_name);
    mPhase = LOWFSMISC_PHASE_RENAME;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::Unlink
// -----------------------------------------------------------------------------

void LowFSMisc::Unlink(const char *file_name, int callID)
{
    mCallID = low_add_stash(mLow, callID);
    mLow->run_ref++;

    mOldName = low_strdup(file_name);
    mPhase = LOWFSMISC_PHASE_UNLINK;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
}


// -----------------------------------------------------------------------------
//  LowFSMisc::OnData
// -----------------------------------------------------------------------------

bool LowFSMisc::OnData()
{
    switch(mPhase)
    {
        case LOWFSMISC_PHASE_RENAME:
            mError = 0;
            if(rename(mOldName, mNewName) != 0)
                mError = errno;

            low_free(mOldName);
            mOldName = NULL;

            low_free(mNewName);
            mNewName = NULL;

            low_loop_set_callback(mLow, this);
            break;

        case LOWFSMISC_PHASE_UNLINK:
            mError = 0;
            if(unlink(mOldName) != 0)
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
        else
            low_push_error(mLow, mError, "unlink");
    }
    else
        duk_push_null(mLow->duk_ctx);
    duk_call(mLow->duk_ctx, 1);

    return false;
}
