// -----------------------------------------------------------------------------
//  LowFD.cpp
// -----------------------------------------------------------------------------

#include "LowFD.h"
#include "LowServerSocket.h"


// -----------------------------------------------------------------------------
//  LowFD::~LowFD
// -----------------------------------------------------------------------------

LowFD::~LowFD()
{
    if(mAdvertisedFD >= 0)
        mLow->fds.erase(mAdvertisedFD);
    low_web_clear_poll(mLow, this);

    if(mLow->reset_accepts && mAdvertisedFD >= 0 /* make sure we are in code thread */)
    {
        mLow->reset_accepts = false;
        for(auto iter = mLow->fds.begin(); iter != mLow->fds.end(); iter++)
        {
            if(iter->second->FDType() == LOWFD_TYPE_SERVER && iter->second->FD() >= 0
            && !((LowServerSocket *)iter->second)->WaitForNotTooManyConnections())
                low_web_set_poll_events(mLow, iter->second, POLLIN);
        }
    }
}