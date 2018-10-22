// -----------------------------------------------------------------------------
//  LowFSMisc.cpp
// -----------------------------------------------------------------------------


#include "low_main.h"
#include "LowFSMisc.h"
#include "low_loop.h"
#include "low_data_thread.h"

#include "low_alloc.h"
#include "low_system.h"
#include "low_config.h"

#include "duktape.h"

#include <fcntl.h>
#include <cstdlib>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
//  LowFSMisc::LowFSMisc
// -----------------------------------------------------------------------------

LowFSMisc::LowFSMisc(low_main_t *low)
    : LowDataCallback(low), LowLoopCallback(low),
      mLow(low)
{
      
}

// -----------------------------------------------------------------------------
//  LowFSMisc::ReName
// -----------------------------------------------------------------------------

void LowFSMisc::ReName(const char *old_name, const char *new_name, int callID)
{
    if (callID)
    {
        mCallID = low_add_stash(mLow, callID);
        if (mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    
    mOldName = low_strdup(old_name);
    mNewName = low_strdup(new_name);
    mPhase = LOWFSMISC_PHASE_RENAME;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
    
}

// -----------------------------------------------------------------------------
//  LowFSMisc::UnLink
// -----------------------------------------------------------------------------

void LowFSMisc::UnLink(const char *file_name, int callID)
{
    if (callID)
    {
        mCallID = low_add_stash(mLow, callID);
        if (mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;
    
    mNewName = NULL;
    mOldName = low_strdup(file_name);
    mPhase = LOWFSMISC_PHASE_UNLINK;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_MODIFY);
    
}



LowFSMisc::~LowFSMisc()
{
    low_data_clear_callback(mLow, this);

    if (mOldName)
        low_free(mOldName); 
    if (mNewName)
        low_free(mNewName);
    if (mCallID)
    {
        low_remove_stash(mLow, mCallID);
        mLow->run_ref--;
    }
}


// -----------------------------------------------------------------------------
//  LowFSMisc::OnData
// -----------------------------------------------------------------------------

bool LowFSMisc::OnData()
{
    switch (mPhase)
    {
    case LOWFSMISC_PHASE_RENAME:
        mError = 0;
        if (rename(mOldName, mNewName) != 0){
            mError = errno;
        }
            
        mSyscall = "rename";

        low_free(mOldName);
        mOldName = NULL;     
            
        low_free(mNewName);
        mNewName = NULL;

        low_loop_set_callback(mLow, this);
        break;
    
    case LOWFSMISC_PHASE_UNLINK:
        mError = 0;
        if (unlink(mOldName) != 0){
            mError = errno;
        }
        
        mSyscall = "unlink";

        low_free(mOldName);
        mOldName = NULL;               

        low_loop_set_callback(mLow, this);
        break;
    }
    return true;
}

//--------------------
//  LowFSMisc::OnLoop
// -----------------------------------------------------------------------------
    
bool LowFSMisc::OnLoop()
{
    if (mCallID)
    {
        int callID = mCallID;
        mCallID = 0;
        mLow->run_ref--;
        
        switch (mPhase)
        {
        case LOWFSMISC_PHASE_RENAME:
            if (mError)
            {
                low_push_stash(mLow, callID, true);
                low_push_error(mLow, mError, mSyscall);
                duk_call(mLow->duk_ctx, 1);
                return true;
            }
            break;
        case LOWFSMISC_PHASE_UNLINK:
            if (mError)
            {
                low_push_stash(mLow, callID, true);
                low_push_error(mLow, mError, mSyscall);
                duk_call(mLow->duk_ctx, 1);
                return true;
            }
            break;
        }       
    
    }
    return true;
}