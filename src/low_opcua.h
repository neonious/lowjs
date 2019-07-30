
// -----------------------------------------------------------------------------
//  low_opcua.h
// -----------------------------------------------------------------------------

#ifndef __LOW_OPCUA_H__
#define __LOW_OPCUA_H__

#include "duktape.h"

#include "low_main.h"
#include "LowLoopCallback.h"
#include "LowFD.h"

bool low_register_opcua(low_main_t *low);

int opcua_uaclient_constructor(duk_context *ctx);
int opcua_uaclient_destroy(duk_context *ctx);

struct UA_Client;
class LowOPCUA : public LowLoopCallback, public LowFD
{
public:
    LowOPCUA(low_main_t *low, struct UA_Client *client, int thisIndex, const char *url);
    virtual ~LowOPCUA();

    // Not used (we do not advertise the FD, only used in web_thread)
    virtual void Read(int pos, unsigned char *data, int len, int callIndex) {}
    virtual void Write(int pos, unsigned char *data, int len,
                       int callIndex) {}
    virtual bool Close(int callIndex) { return true; }

protected:
    static void OnConnect(struct UA_Client *client, void *userdata, uint32_t requestId, void *data);

    static void OnTimeout(void *data);

    virtual bool OnLoop();
    virtual bool OnEvents(short events);

private:
    low_main_t *mLow;
    struct UA_Client *mClient;
    int mThisIndex;

    pthread_mutex_t mMutex;
    int mChoreIndex;

    int mConnectState, mDisabledState, mError;
};

#endif /* __LOW_OPCUA_H__ */
