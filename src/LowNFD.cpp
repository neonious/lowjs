// -----------------------------------------------------------------------------
//  LowNFD.cpp
// -----------------------------------------------------------------------------


#include "low_main.h"
#include "LowNFD.h"
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

// -----------------------------------------------------------------------------
//  LowNFD::LowNFD
// -----------------------------------------------------------------------------

LowNFD::LowNFD(low_main_t *low, const char *old_name, const char *new_name, int callID)
    : LowDataCallback(low), LowLoopCallback(low),
      mLow(low)
{
    if (callID)
    {
        mCallID = low_add_stash(mLow, callID);
        printf("REturn callindex %d", mCallID);
        if (mCallID)
            mLow->run_ref++;
    }
    else
        mCallID = 0;

    mold_name = low_strdup(old_name);
    mnew_name = low_strdup(new_name);;
    mPhase = LOWNFD_PHASE_RENAME;
    low_data_set_callback(mLow, this, LOW_DATA_THREAD_PRIORITY_READ);
    
  
}

LowNFD::~LowNFD()
{
    low_data_clear_callback(mLow, this);

    if (mold_name)
        low_free(mold_name); 
    if (mnew_name)
        low_free(mnew_name);
    if (mCallID)
    {
        low_remove_stash(mLow, mCallID);
        mLow->run_ref--;
    }
}


// -----------------------------------------------------------------------------
//  LowNFD::OnData
// -----------------------------------------------------------------------------

bool LowNFD::OnData()
{
    switch (mPhase)
    {
    case LOWNFD_PHASE_RENAME:
        if (rename(mold_name, mnew_name) != 0){
            mError = errno;
            printf("Performed Rename\n");
        }
            

        mSyscall = "rename";

        low_free(mold_name);
        mold_name = NULL;     
            
        low_free(mnew_name);
        mnew_name = NULL;

        //printf("Performed Rename %s oldie , %s newie\n", mold_name, mnew_name);
        mDataDone = true;
        low_loop_set_callback(mLow, this);
        break;
    }
}


bool LowNFD::OnLoop()
{
    if (mCallID)
    {
        int callID = mCallID;
        mCallID = 0;
        mLow->run_ref--;

        switch (mPhase)
        {
        case LOWNFD_PHASE_RENAME:
            if (mError)
            {
                low_push_stash(mLow, callID, true);
                low_push_error(mLow, mError, mSyscall);
                duk_call(mLow->duk_ctx, 1);
                return true;
            }
            break;
        }
    return true;
    }
}