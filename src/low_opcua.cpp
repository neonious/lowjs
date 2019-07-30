// -----------------------------------------------------------------------------
//  low_opcua.c
// -----------------------------------------------------------------------------

#include "low_opcua.h"
#include "low_module.h"
#include "low_system.h"

#include <pthread.h>

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <ua_client_internal.h>


// -----------------------------------------------------------------------------
//  register_opcua - registers the module "opc-ua"
// -----------------------------------------------------------------------------

static void setup_module_safe(low_main_t *low, void *data)
{
    duk_context *ctx = low_get_duk_context(low);

    // DukTape stack is [module] [exports]

    duk_push_c_function(ctx, opcua_uaclient_constructor, 1);

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "UAClient");
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);

    low_module_require_c(ctx, "lib:events", 0);
    duk_get_prop_string(ctx, -1, "exports");
    duk_get_prop_string(ctx, -1, "EventEmitter");
    duk_set_prototype(ctx, -4);
    duk_pop_2(ctx);

    duk_put_prop_string(ctx, 1, "UAClient");    // add to 1 = exports
}

bool low_register_opcua(low_main_t *low)
{
    return low_module_make_native(low, "opc-ua", setup_module_safe, NULL);
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_constructor
// -----------------------------------------------------------------------------

int opcua_uaclient_constructor(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);

    duk_get_prop_string(ctx, 0, "url");
    const char *url = duk_require_string(ctx, -1);

    bool a = duk_get_prop_string(ctx, 0, "username");
    bool b = duk_get_prop_string(ctx, 0, "password");
    const char *username, *password;
    if(a || b)
    {
        username = duk_require_string(ctx, -2);
        password = duk_require_string(ctx, -1);
    }

    int timeout = duk_get_prop_string(ctx, 0, "timeout_ms") ? duk_require_int(ctx, -1) : 30000;

    int thisIndex = low_add_stash(low, 1);

    duk_function_list_entry methods[] = {{"destroy", opcua_uaclient_destroy, 0},
                                         {NULL, NULL, 0}};
    duk_put_function_list(low_get_duk_context(low), 1, methods);

    LowOPCUA **opcua = (LowOPCUA **)duk_push_fixed_buffer(ctx, sizeof(LowOPCUA *));
    *opcua = NULL;
    duk_put_prop_string(ctx, 1, "\xffnativeObj");

    UA_Client *client = UA_Client_new();
    if(!client)
    {
        low_remove_stash(low, thisIndex);

        low_push_error(low, ENOMEM, "malloc");
        duk_throw(ctx);
    }

    // Setup config
    auto config = UA_Client_getConfig(client);
    if(UA_ClientConfig_setDefault(config) != UA_STATUSCODE_GOOD)
    {
        low_remove_stash(low, thisIndex);
        UA_Client_delete(client);

        low_push_error(low, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    config->timeout = timeout;
    config->logger.log = NULL;
    config->logger.clear = NULL;
    if(a || b)
    {
        UA_UserNameIdentityToken* identityToken = UA_UserNameIdentityToken_new();
        if(!identityToken)
        {
            low_remove_stash(low, thisIndex);
            UA_Client_delete(client);

            low_push_error(low, ENOMEM, "malloc");
            duk_throw(ctx);
        }

        identityToken->userName = UA_STRING_ALLOC(username);
        identityToken->password = UA_STRING_ALLOC(password);
        UA_ExtensionObject_deleteMembers(&client->config.userIdentityToken);
        client->config.userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
        client->config.userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN];
        client->config.userIdentityToken.content.decoded.data = identityToken;
    }

    // Start the connection
    *opcua = new(low_new) LowOPCUA(low, client, thisIndex, url);
    if(!*opcua)
    {
        low_remove_stash(low, thisIndex);
        UA_Client_delete(client);

        low_push_error(low, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_destroy
// -----------------------------------------------------------------------------

int opcua_uaclient_destroy(duk_context *ctx)
{
    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, 0, "\xffnativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA **opcua = (LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!*opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    LowOPCUA *obj = *opcua;
    duk_del_prop_string(ctx, 0, "\xffnativeObj");

    delete obj;
    return 0;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::LowOPCUA
// -----------------------------------------------------------------------------

static void clientExecuteRepeatedCallback(UA_Client *client, UA_ApplicationCallback cb, void *callbackApplication, void *data)
{
    cb(callbackApplication, data);
}

LowOPCUA::LowOPCUA(low_main_t *low, UA_Client *client, int thisIndex, const char *url)
    : LowLoopCallback(low), LowFD(low, LOWFD_TYPE_SOCKET, client->connection.sockfd), mLow(low), mClient(client), mThisIndex(thisIndex), mMutex(PTHREAD_MUTEX_INITIALIZER), mConnectState(0), mDisabledState(0)
{
    low->run_ref++;
    if(client->connection.sockfd > 0)
        low_web_set_poll_events(low, this, POLLIN | POLLOUT | POLLERR);

    mError = UA_Client_connect_async(client, url, OnConnect, this);
    if(mError != UA_STATUSCODE_GOOD)
    {
        mDisabledState = 1;
        low_loop_set_callback(mLow, this);
        return;
    }

    UA_DateTime now = UA_DateTime_nowMonotonic();
    UA_DateTime next = UA_Timer_process(&client->timer, now, (UA_TimerExecutionCallback)clientExecuteRepeatedCallback, client);

    if((signed)next != -1)
        mChoreIndex = low_loop_set_chore_c(mLow, 0, (next - now) / 1000000, OnTimeout, this);
}


// -----------------------------------------------------------------------------
//  LowOPCUA::~LowOPCUA
// -----------------------------------------------------------------------------

LowOPCUA::~LowOPCUA()
{
    low_web_clear_poll(mLow, this);
    low_loop_clear_chore_c(mLow, mChoreIndex);

    UA_Client_delete(mClient);
    low_remove_stash(mLow, mThisIndex);
    mLow->run_ref--;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnConnect
// -----------------------------------------------------------------------------

void LowOPCUA::OnConnect(UA_Client *client, void *userdata, UA_UInt32 requestId, void *data)
{
    LowOPCUA *opcua = (LowOPCUA *)userdata;
    if(opcua->mDisabledState || opcua->mConnectState)
        return;

    UA_StatusCode state = *(UA_StatusCode *)data;
    opcua->mError = state;
    opcua->mConnectState = 1;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnTimeout
// -----------------------------------------------------------------------------

void LowOPCUA::OnTimeout(void *data)
{
    LowOPCUA *opcua = (LowOPCUA *)data;

    if(opcua->mDisabledState)
        return;
    pthread_mutex_lock(&opcua->mMutex);
    if(opcua->mDisabledState)
    {
        pthread_mutex_unlock(&opcua->mMutex);
        return;
    }

    int sockfd = opcua->mClient->connection.sockfd;
    UA_DateTime now = UA_DateTime_nowMonotonic();
    UA_DateTime next = UA_Timer_process(&opcua->mClient->timer, now, (UA_TimerExecutionCallback)clientExecuteRepeatedCallback, opcua->mClient);
    if(sockfd != opcua->mClient->connection.sockfd)
    {
        sockfd = opcua->mClient->connection.sockfd;
        opcua->SetFD(sockfd > 0 ? sockfd : -1);
        low_web_set_poll_events(opcua->mLow, opcua, sockfd > 0 ? POLLIN | POLLOUT | POLLERR : 0);
    }

    if((signed)next != -1)
        opcua->mChoreIndex = low_loop_set_chore_c(opcua->mLow, opcua->mChoreIndex, (next - now) / 1000000, OnTimeout, data);

    pthread_mutex_unlock(&opcua->mMutex);
    if(opcua->mConnectState == 1 || opcua->mDisabledState == 1)
        low_loop_set_callback(opcua->mLow, opcua);
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnLoop
// -----------------------------------------------------------------------------

bool LowOPCUA::OnLoop()
{
    if(mDisabledState == 2)
        return true;

    if((mConnectState == 1 && (mError & 0x80000000)) || mDisabledState == 1)
    {
        duk_context *ctx = low_get_duk_context(mLow);
        low_push_stash(mLow, mThisIndex, false);
        duk_push_string(ctx, "emit");
        duk_push_string(ctx, "error");

        duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(mError));
        duk_push_int(mLow->duk_ctx, mError);
        duk_put_prop_string(mLow->duk_ctx, -2, "open62541_statuscode");

        duk_call_prop(ctx, -4, 2);
        mDisabledState = 2;
    }
    else if(mConnectState == 1)
    {
        duk_context *ctx = low_get_duk_context(mLow);
        low_push_stash(mLow, mThisIndex, false);
        duk_push_string(ctx, "emit");
        duk_push_string(ctx, "connect");
        duk_call_prop(ctx, -3, 1);
        mConnectState = 2;
    }

    return true;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnEvents
// -----------------------------------------------------------------------------

bool LowOPCUA::OnEvents(short events)
{
    low_web_set_poll_events(mLow, this, mDisabledState ? 0 : POLLIN | POLLERR);

    if(mDisabledState)
        return true;
    pthread_mutex_lock(&mMutex);
    if(mDisabledState)
    {
        pthread_mutex_unlock(&mMutex);
        return true;
    }

    int sockfd = mClient->connection.sockfd;
    int state = UA_Client_run_iterate(mClient, 0);
    if(state & 0x80000000)
    {
        mError = state;
        mDisabledState = 1;
        pthread_mutex_unlock(&mMutex);

        low_web_set_poll_events(mLow, this, 0);
        low_loop_set_callback(mLow, this);
        return true;
    }
    if(sockfd != mClient->connection.sockfd)
    {
        sockfd = mClient->connection.sockfd;
        SetFD(sockfd > 0 ? sockfd : -1);
        low_web_set_poll_events(mLow, this, sockfd > 0 ? POLLIN | POLLOUT | POLLERR : 0);
    }

    pthread_mutex_unlock(&mMutex);
    if(mConnectState == 1 || mDisabledState == 1)
        low_loop_set_callback(mLow, this);

    OnTimeout(this);
    return true;
}