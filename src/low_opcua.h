
// -----------------------------------------------------------------------------
//  low_opcua.h
// -----------------------------------------------------------------------------

#ifndef __LOW_OPCUA_H__
#define __LOW_OPCUA_H__

#include "duktape.h"

#include "low_main.h"
#include "LowLoopCallback.h"
#include "LowFD.h"

#include <open62541/types.h>
#include <open62541/types_generated.h>

#include <map>
#include <queue>

#define LOW_OPCUA_WRITEBUFFER_SIZE  1024
bool low_register_opcua(low_main_t *low);

int opcua_uaclient_constructor(duk_context *ctx);
int opcua_uaclient_destroy(duk_context *ctx);
int opcua_uaclient_node(duk_context *ctx);

int opcua_uaclient_create_subscription(duk_context *ctx);
int opcua_uaclient_destroy_subscription(duk_context *ctx);
int opcua_uaclient_subscription_add(duk_context *ctx);
int opcua_uaclient_subscription_remove(duk_context *ctx);

void opcua_fill_node(duk_context *ctx);

int opcua_uaclient_lookup_props(duk_context *ctx);
int opcua_uaclient_subnode(duk_context *ctx);
int opcua_uaclient_children(duk_context *ctx);

int opcua_uaclient_read(duk_context *ctx);
int opcua_uaclient_write(duk_context *ctx);
int opcua_uaclient_call(duk_context *ctx);

#define LOWOPCTASK_TYPE_DESTROY                 0
#define LOWOPCTASK_TYPE_LOOKUP_PROPS            1
#define LOWOPCTASK_TYPE_SUBNODE                 2
#define LOWOPCTASK_TYPE_CHILDREN                3
#define LOWOPCTASK_TYPE_READ                    4
#define LOWOPCTASK_TYPE_WRITE                   5
#define LOWOPCTASK_TYPE_CALL                    6
#define LOWOPCTASK_TYPE_CREATE_SUBSCRIPTION     7
#define LOWOPCTASK_TYPE_DESTROY_SUBSCRIPTION    8
#define LOWOPCTASK_TYPE_SUBSCRIPTION_ADD        9
#define LOWOPCTASK_TYPE_SUBSCRIPTION_REMOVE     10

struct LowOPCUATask
{
    class LowOPCUA *opcua;
    int id, type;
    int objStashIndex, objStashIndex2;
    int callbackStashIndex, timeoutChoreIndex;
    void *result;
    const UA_DataType *resultType;
};

struct UA_Client;
class LowOPCUA : public LowLoopCallback, public LowFD
{
public:
    LowOPCUA(low_main_t *low, struct UA_Client *client, int thisIndex, int timeoutMS, const char *url);
    virtual ~LowOPCUA();

    void DisconnectAndDetach(int callbackStashIndex);
    void AddAsyncRequestAndUnlock(int type, unsigned int reqID, const UA_DataType *resultType, int objStaskIndex, int callbackStashIndex, int objStashIndex2 = 0);

    // Not used (we do not advertise the FD, only used in web_thread)
    virtual void Read(int pos, unsigned char *data, int len, int callIndex) {}
    virtual void Write(int pos, unsigned char *data, int len,
                       int callIndex) {}
    virtual bool Close(int callIndex) { return true; }

    static int OnSend(void *data, unsigned char *buf, int size);

protected:
    static void OnConnect(struct UA_Client *client, void *userdata, uint32_t requestId, void *data);

    static void OnTimeout(void *data);

    static void OnTaskTimeout(void *data);
    static UA_StatusCode OnPublishNotification(struct UA_Client *client, UA_ExtensionObject *msg, void *data);

    virtual bool OnLoop();
    virtual bool OnEvents(short events);

    void PushVariant(UA_Variant *variant);

private:
    low_main_t *mLow;
    int mTimeoutMS;
    int mThisIndex;

    int mChoreIndex;

    unsigned char *mWriteBuffer, *mWriteBufferLast;
    int mWriteBufferRead, mWriteBufferLastLen;

    bool mGoToLoop;

    std::map<UA_UInt32, std::pair<int, int> > mMonitoredItems;
    std::queue<UA_MonitoredItemNotification *> mDataChangeNotifications;

public:
    static void OnTaskCallback(
        UA_Client *client, void *userdata, unsigned int reqID, void *responsedata);

    struct UA_Client *mClient;
    std::map<int, LowOPCUATask> mTasks;
    pthread_mutex_t mMutex;
    int mConnectState, mDisabledState, mDetachedState, mError;
};

#endif /* __LOW_OPCUA_H__ */
