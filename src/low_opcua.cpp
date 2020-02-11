// -----------------------------------------------------------------------------
//  low_opcua.c
// -----------------------------------------------------------------------------

#include "low_opcua.h"
#include "low_module.h"
#include "low_system.h"

#include <pthread.h>

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel_async.h>
#include <ua_client_internal.h>


int (*gLowOPCUASend)(void *data, unsigned char *buf, int size) = LowOPCUA::OnSend;


// -----------------------------------------------------------------------------
//  register_opcua - registers the module "opc-ua"
// -----------------------------------------------------------------------------

static void setup_module_safe(low_t *low, void *data)
{
    duk_context *ctx = low_get_duk_context(low);

    // DukTape stack is [module] [exports]

    duk_push_c_function(ctx, opcua_uaclient_constructor, 1);

    duk_push_string(ctx, "name");
    duk_push_string(ctx, "UAClient");
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);

    low_load_module(ctx, "lib:events", false);
    duk_get_prop_string(ctx, -1, "exports");
    duk_get_prop_string(ctx, -1, "EventEmitter");
    duk_set_prototype(ctx, -4);
    duk_pop_2(ctx);

    duk_push_int(ctx, 1); duk_put_prop_string(ctx, 1, "ATTRIBUTE_NODEID");
    duk_push_int(ctx, 2); duk_put_prop_string(ctx, 1, "ATTRIBUTE_NODECLASS");
    duk_push_int(ctx, 3); duk_put_prop_string(ctx, 1, "ATTRIBUTE_BROWSENAME");
    duk_push_int(ctx, 4); duk_put_prop_string(ctx, 1, "ATTRIBUTE_DISPLAYNAME");
    duk_push_int(ctx, 5); duk_put_prop_string(ctx, 1, "ATTRIBUTE_DESCRIPTION");
    duk_push_int(ctx, 6); duk_put_prop_string(ctx, 1, "ATTRIBUTE_WRITEMASK");
    duk_push_int(ctx, 7); duk_put_prop_string(ctx, 1, "ATTRIBUTE_USERWRITEMASK");
    duk_push_int(ctx, 8); duk_put_prop_string(ctx, 1, "ATTRIBUTE_ISABSTRACT");
    duk_push_int(ctx, 9); duk_put_prop_string(ctx, 1, "ATTRIBUTE_SYMMETRIC");
    duk_push_int(ctx, 10); duk_put_prop_string(ctx, 1, "ATTRIBUTE_INVERSENAME");
    duk_push_int(ctx, 11); duk_put_prop_string(ctx, 1, "ATTRIBUTE_CONTAINSNOLOOPS");
    duk_push_int(ctx, 12); duk_put_prop_string(ctx, 1, "ATTRIBUTE_EVENTNOTIFIER");
    duk_push_int(ctx, 13); duk_put_prop_string(ctx, 1, "ATTRIBUTE_VALUE");
    duk_push_int(ctx, 14); duk_put_prop_string(ctx, 1, "ATTRIBUTE_DATATYPE");
    duk_push_int(ctx, 15); duk_put_prop_string(ctx, 1, "ATTRIBUTE_VALUERANK");
    duk_push_int(ctx, 16); duk_put_prop_string(ctx, 1, "ATTRIBUTE_ARRAYDIMENSIONS");
    duk_push_int(ctx, 17); duk_put_prop_string(ctx, 1, "ATTRIBUTE_ACCESSLEVEL");
    duk_push_int(ctx, 18); duk_put_prop_string(ctx, 1, "ATTRIBUTE_USERACCESSLEVEL");
    duk_push_int(ctx, 19); duk_put_prop_string(ctx, 1, "ATTRIBUTE_MINIMUMSAMPLINGINTERVAL");
    duk_push_int(ctx, 20); duk_put_prop_string(ctx, 1, "ATTRIBUTE_HISTORIZING");
    duk_push_int(ctx, 21); duk_put_prop_string(ctx, 1, "ATTRIBUTE_EXECUTABLE");
    duk_push_int(ctx, 22); duk_put_prop_string(ctx, 1, "ATTRIBUTE_USEREXECUTABLE");

    duk_push_int(ctx, 0); duk_put_prop_string(ctx, 1, "TYPE_BOOLEAN");
    duk_push_int(ctx, 1); duk_put_prop_string(ctx, 1, "TYPE_SBYTE");
    duk_push_int(ctx, 2); duk_put_prop_string(ctx, 1, "TYPE_BYTE");
    duk_push_int(ctx, 3); duk_put_prop_string(ctx, 1, "TYPE_INT16");
    duk_push_int(ctx, 4); duk_put_prop_string(ctx, 1, "TYPE_UINT16");
    duk_push_int(ctx, 5); duk_put_prop_string(ctx, 1, "TYPE_INT32");
    duk_push_int(ctx, 6); duk_put_prop_string(ctx, 1, "TYPE_UINT32");
    duk_push_int(ctx, 9); duk_put_prop_string(ctx, 1, "TYPE_FLOAT");
    duk_push_int(ctx, 10); duk_put_prop_string(ctx, 1, "TYPE_DOUBLE");
    duk_push_int(ctx, 11); duk_put_prop_string(ctx, 1, "TYPE_STRING");
    duk_push_int(ctx, 14); duk_put_prop_string(ctx, 1, "TYPE_BYTESTRING");
    duk_push_int(ctx, 19); duk_put_prop_string(ctx, 1, "TYPE_QUALIFIEDNAME");
    duk_push_int(ctx, 20); duk_put_prop_string(ctx, 1, "TYPE_LOCALIZEDTEXT");

    duk_put_prop_string(ctx, 1, "UAClient");    // add to 1 = exports
}

bool low_register_opcua(low_t *low)
{
    return low_module_make_native(low, "opc-ua", setup_module_safe, NULL);
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_constructor
// -----------------------------------------------------------------------------

int opcua_uaclient_constructor(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

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

    int thisIndex = low_add_stash(low->duk_ctx, 1);

    duk_function_list_entry methods[] = {{"node", opcua_uaclient_node, 2},
                                         {"createSubscription", opcua_uaclient_create_subscription, 1},
                                         {"destroy", opcua_uaclient_destroy, 1},
                                         {NULL, NULL, 0}};
    duk_put_function_list(low_get_duk_context(low), 1, methods);

    LowOPCUA **opcua = (LowOPCUA **)duk_push_fixed_buffer(ctx, sizeof(LowOPCUA *));
    *opcua = NULL;
    duk_dup(ctx, -1);       // for root and object nodes
    duk_put_prop_string(ctx, 1, "\xff""nativeObj");

    // Add root and objects nodes
    duk_push_object(ctx);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "\xff""nativeObj");

    opcua_fill_node(ctx);

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "namespace");
    duk_push_int(ctx, 84);
    duk_put_prop_string(ctx, -2, "node");

    duk_put_prop_string(ctx, 1, "root");

    // Add root and objects nodes -- objects
    duk_push_object(ctx);
    duk_dup(ctx, -2);
    duk_put_prop_string(ctx, -2, "\xff""nativeObj");

    opcua_fill_node(ctx);

    duk_push_int(ctx, 0);
    duk_put_prop_string(ctx, -2, "namespace");
    duk_push_int(ctx, 85);
    duk_put_prop_string(ctx, -2, "node");

    duk_put_prop_string(ctx, 1, "objects");
    // Add root and objects nodes end

    UA_Client *client = UA_Client_new();
    if(!client)
    {
        low_remove_stash(low->duk_ctx, thisIndex);

        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }

    // Setup config
    auto config = UA_Client_getConfig(client);
    if(UA_ClientConfig_setDefault(config) != UA_STATUSCODE_GOOD)
    {
        low_remove_stash(low->duk_ctx, thisIndex);
        UA_Client_delete(client);

        low_push_error(ctx, ENOMEM, "malloc");
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
            low_remove_stash(low->duk_ctx, thisIndex);
            UA_Client_delete(client);

            low_push_error(ctx, ENOMEM, "malloc");
            duk_throw(ctx);
        }

        identityToken->userName = UA_STRING_ALLOC(username);
        identityToken->password = UA_STRING_ALLOC(password);
        UA_ExtensionObject_deleteMembers(&client->config.userIdentityToken);
        client->config.userIdentityToken.encoding = UA_EXTENSIONOBJECT_DECODED;
        client->config.userIdentityToken.content.decoded.type = &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN];
        client->config.userIdentityToken.content.decoded.data = identityToken;
    }
    client->publishNotificationCallback = NULL;

    // Start the connection
    *opcua = new LowOPCUA(low, client, thisIndex, config->timeout, url);
    if(!*opcua)
    {
        low_remove_stash(low->duk_ctx, thisIndex);
        UA_Client_delete(client);

        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_destroy
// -----------------------------------------------------------------------------

int opcua_uaclient_destroy(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA **opcua = (LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!*opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    LowOPCUA *obj = *opcua;
    *opcua = NULL;

    obj->DisconnectAndDetach(low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_fill_node
// -----------------------------------------------------------------------------

void opcua_fill_node(duk_context *ctx)
{
    duk_push_string(ctx, "name");
    duk_push_string(ctx, "UAClientNode");
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);

    duk_function_list_entry methods[] = {{"lookupProps", opcua_uaclient_lookup_props, 1},
                                        {"subNode", opcua_uaclient_subnode, 2},
                                        {"children", opcua_uaclient_children, 1},
                                        {"read", opcua_uaclient_read, 2},
                                        {"write", opcua_uaclient_write, 4},
                                        {"call", opcua_uaclient_call, 1},
                                        {NULL, NULL, 0}};
    duk_put_function_list(ctx, -1, methods);
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_node
// -----------------------------------------------------------------------------

int opcua_uaclient_node(duk_context *ctx)
{
    // Make sure they are of right type
    int namespac = duk_require_int(ctx, 0);
    int node = duk_require_int(ctx, 1);

    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_pop(ctx);
    duk_push_object(ctx);

    duk_get_prop_string(ctx, -2, "\xff""nativeObj");
    duk_put_prop_string(ctx, -2, "\xff""nativeObj");

    opcua_fill_node(ctx);

    duk_dup(ctx, 0);
    duk_put_prop_string(ctx, -2, "namespace");
    duk_dup(ctx, 1);
    duk_put_prop_string(ctx, -2, "node");
    return 1;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_create_subscription
// -----------------------------------------------------------------------------

int opcua_uaclient_create_subscription(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    UA_UInt32 reqID = 0;
    UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_CREATESUBSCRIPTIONREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_CREATESUBSCRIPTIONRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_CreateSubscriptionRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_CREATE_SUBSCRIPTION, reqID, &UA_TYPES[UA_TYPES_CREATESUBSCRIPTIONRESPONSE], low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_destroy_subscription
// -----------------------------------------------------------------------------

int opcua_uaclient_destroy_subscription(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "\xff""id");
    if(duk_is_undefined(ctx, -1))
        duk_reference_error(ctx, "subscription is already destroyed");
    UA_UInt32 subscriptionId = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "\xff""itemCount");
    if(duk_require_int(ctx, -1))
        duk_reference_error(ctx, "subscription still holds monitored items");

    UA_UInt32 reqID = 0;
    UA_DeleteSubscriptionsRequest request;
    UA_DeleteSubscriptionsRequest_init(&request);
    request.subscriptionIds = &subscriptionId;
    request.subscriptionIdsSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_DELETESUBSCRIPTIONSREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_DELETESUBSCRIPTIONSRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    request.subscriptionIds = NULL;
    request.subscriptionIdsSize = 0;
    UA_DeleteSubscriptionsRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_DESTROY_SUBSCRIPTION, reqID, &UA_TYPES[UA_TYPES_DELETESUBSCRIPTIONSRESPONSE], low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_subscription_add
// -----------------------------------------------------------------------------

int opcua_uaclient_subscription_add(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, 2, "\xff""id");
    if(duk_is_undefined(ctx, -1))
        duk_reference_error(ctx, "subscription is already destroyed");
    int subscriptionID = duk_require_int(ctx, -1);

    duk_get_prop_string(ctx, 2, "\xff""itemCount");
    duk_push_int(ctx, duk_require_int(ctx, -1) + 1);
    duk_put_prop_string(ctx, 2, "\xff""itemCount");

    duk_get_prop_string(ctx, 0, "\xff""monitoredItemID");
    if(!duk_is_undefined(ctx, -1))
        duk_reference_error(ctx, "node is already monitored");

    duk_get_prop_string(ctx, 0, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, 0, "node");
    int node = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_MonitoredItemCreateRequest item =
        UA_MonitoredItemCreateRequest_default(UA_NODEID_NUMERIC(namespac, node));
    int clientHandle = ++opcua->mLastClientHandle;
    item.requestedParameters.clientHandle = clientHandle;
    UA_CreateMonitoredItemsRequest request;
    UA_CreateMonitoredItemsRequest_init(&request);
    request.subscriptionId = subscriptionID;
    request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
    request.itemsToCreate = &item;
    request.itemsToCreateSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_CREATEMONITOREDITEMSREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_CREATEMONITOREDITEMSRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_MonitoredItemCreateRequest_deleteMembers(&item);
    request.itemsToCreate = NULL;
    request.itemsToCreateSize = 0;
    UA_CreateMonitoredItemsRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    // client , callback , node
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_SUBSCRIPTION_ADD, reqID, &UA_TYPES[UA_TYPES_CREATEMONITOREDITEMSRESPONSE], low_add_stash(low->duk_ctx, 2), low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 0), clientHandle);
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_subscription_remove
// -----------------------------------------------------------------------------

int opcua_uaclient_subscription_remove(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, 2, "\xff""id");
    if(duk_is_undefined(ctx, -1))
        duk_reference_error(ctx, "subscription is already destroyed");
    int subscriptionID = duk_require_int(ctx, -1);

    duk_get_prop_string(ctx, 0, "\xff""monitoredItemID");
    if(duk_is_undefined(ctx, -1))
        duk_reference_error(ctx, "node is not monitored");
    UA_UInt32 monitoredItemID = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_DeleteMonitoredItemsRequest request;
    UA_DeleteMonitoredItemsRequest_init(&request);
    request.subscriptionId = subscriptionID;
    request.monitoredItemIds = &monitoredItemID;
    request.monitoredItemIdsSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_DELETEMONITOREDITEMSREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_DELETEMONITOREDITEMSRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    request.monitoredItemIds = NULL;
    request.monitoredItemIdsSize = 0;
    UA_DeleteMonitoredItemsRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }

    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_SUBSCRIPTION_REMOVE, reqID, &UA_TYPES[UA_TYPES_DELETEMONITOREDITEMSRESPONSE], low_add_stash(low->duk_ctx, 0), low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 2));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_lookup_props
// -----------------------------------------------------------------------------

int opcua_uaclient_lookup_props(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_ReadValueId item[3];
    UA_ReadValueId_init(&item[0]);
    item[0].nodeId = UA_NODEID_NUMERIC(namespac, node);
    item[0].attributeId = UA_ATTRIBUTEID_BROWSENAME;
    UA_ReadValueId_init(&item[1]);
    item[1].nodeId = UA_NODEID_NUMERIC(namespac, node);
    item[1].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
    UA_ReadValueId_init(&item[2]);
    item[2].nodeId = UA_NODEID_NUMERIC(namespac, node);
    item[2].attributeId = UA_ATTRIBUTEID_DESCRIPTION;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToRead = item;
    request.nodesToReadSize = 3;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_READREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_READRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_ReadValueId_deleteMembers(&item[0]);
    UA_ReadValueId_deleteMembers(&item[1]);
    UA_ReadValueId_deleteMembers(&item[2]);
    request.nodesToRead = NULL;
    request.nodesToReadSize = 0;
    UA_ReadRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_LOOKUP_PROPS, reqID, &UA_TYPES[UA_TYPES_READRESPONSE], low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_subnode
// -----------------------------------------------------------------------------

int opcua_uaclient_subnode(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    char *path = (char *)duk_require_string(ctx, 0);

    int pathSize = 0;
    bool isEmpty = true;
    for(int i = 0; path[i]; i++)
    {
        if(path[i] == '/')
            isEmpty = true;
        else if(!isspace((unsigned)path[i]) && isEmpty)
        {
            pathSize++;
            isEmpty = false;
        }
    }

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_BrowsePath browsePath;
    UA_BrowsePath_init(&browsePath);
    browsePath.startingNode = UA_NODEID_NUMERIC(namespac, node);
    browsePath.relativePath.elements = (UA_RelativePathElement *)UA_Array_new(
            pathSize, &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]);
    if(!browsePath.relativePath.elements)
    {
        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    browsePath.relativePath.elementsSize = pathSize;

    int elem = 0;
    for(int j = 0; j < pathSize; j++)
    {
        int namespac = 0;
        for(int i = 0; path[i] && path[i] != '/'; i++)
            if(path[i] == ':')
            {
                path[i] = '\0';
                namespac = atoi(path);
                path[i] = ':';
                path += i + 1;
                break;
            }

        while(*path != '/' && isspace((unsigned)*path))
            path++;
        if(*path == '/')
        {
            j--;
            path++;
            continue;
        }

        int len;
        for(len = 0; path[len] && path[len] != '/'; len++) {}
        while(len > 0 && isspace((unsigned)path[len - 1]))
            len--;

        UA_RelativePathElement_init(&browsePath.relativePath.elements[j]);
        browsePath.relativePath.elements[j].isInverse = false;
        browsePath.relativePath.elements[j].includeSubtypes = true;
        char c = path[len];
        path[len] = '\0';
        browsePath.relativePath.elements[j].targetName = UA_QUALIFIEDNAME_ALLOC(namespac, path);
        path[len] = c;
        path += len + 1;
    }

    UA_TranslateBrowsePathsToNodeIdsRequest request;
    UA_TranslateBrowsePathsToNodeIdsRequest_init(&request);
    request.browsePaths = &browsePath;
    request.browsePathsSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_TRANSLATEBROWSEPATHSTONODEIDSREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_Array_delete(browsePath.relativePath.elements, pathSize, &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]);
    browsePath.relativePath.elements = NULL;
    browsePath.relativePath.elementsSize = 0;
    UA_BrowsePath_deleteMembers(&browsePath);
    request.browsePaths = NULL;
    request.browsePathsSize = 0;
    UA_TranslateBrowsePathsToNodeIdsRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_SUBNODE, reqID, &UA_TYPES[UA_TYPES_TRANSLATEBROWSEPATHSTONODEIDSRESPONSE], low_add_stash(low->duk_ctx, 3), low_add_stash(low->duk_ctx, 1));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_children
// -----------------------------------------------------------------------------

int opcua_uaclient_children(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_BrowseRequest request;
    UA_BrowseRequest_init(&request);
    request.requestedMaxReferencesPerNode = 0;
    UA_BrowseDescription desc;
    UA_BrowseDescription_init(&desc);
    request.nodesToBrowse = &desc;
    request.nodesToBrowseSize = 1;
    desc.nodeId = UA_NODEID_NUMERIC(namespac, node);
    desc.resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_BROWSEREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_BROWSERESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_BrowseDescription_deleteMembers(&desc);
    request.nodesToBrowse = NULL;
    request.nodesToBrowseSize = 0;
    UA_BrowseRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_CHILDREN, reqID, &UA_TYPES[UA_TYPES_BROWSERESPONSE], low_add_stash(low->duk_ctx, 2), low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_read
// -----------------------------------------------------------------------------

int opcua_uaclient_read(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    bool hasAttribute = !duk_is_undefined(ctx, 1);

    UA_UInt32 reqID = 0;
    UA_ReadValueId item;
    UA_ReadValueId_init(&item);
    item.nodeId = UA_NODEID_NUMERIC(namespac, node);
    item.attributeId = hasAttribute ? duk_require_int(ctx, 0) : UA_ATTRIBUTEID_VALUE;
    UA_ReadRequest request;
    UA_ReadRequest_init(&request);
    request.nodesToRead = &item;
    request.nodesToReadSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_READREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_READRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_ReadValueId_deleteMembers(&item);
    request.nodesToRead = NULL;
    request.nodesToReadSize = 0;
    UA_ReadRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_READ, reqID, &UA_TYPES[UA_TYPES_READRESPONSE], low_add_stash(low->duk_ctx, 2), low_add_stash(low->duk_ctx, hasAttribute ? 1 : 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_write
// -----------------------------------------------------------------------------

int opcua_uaclient_write(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    bool hasAttribute = !duk_is_undefined(ctx, 3);
    int type = duk_require_int(ctx, hasAttribute ? 2 : 1);

    UA_UInt32 reqID = 0;
    UA_WriteValue item;
    UA_WriteValue_init(&item);
    item.nodeId = UA_NODEID_NUMERIC(namespac, node);
    item.attributeId = hasAttribute ? duk_require_int(ctx, 0) : UA_ATTRIBUTEID_VALUE;
    item.value.value.type = &UA_TYPES[type];
    // Not sure which is bigger
    unsigned char data[sizeof(UA_QualifiedName) > sizeof(UA_QualifiedName) ? sizeof(UA_QualifiedName) : sizeof(UA_QualifiedName)];
    item.value.value.data = data;
    if(type == UA_TYPES_BOOLEAN)
        *(UA_Boolean *)data = duk_require_boolean(ctx, hasAttribute ? 1 : 0) ? true : false;
    else if(type == UA_TYPES_SBYTE)
        *(UA_SByte *)data = duk_require_int(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_BYTE)
        *(UA_Byte *)data = duk_require_uint(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_INT16)
        *(UA_Int16 *)data = duk_require_int(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_UINT16)
        *(UA_UInt16 *)data = duk_require_uint(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_INT32)
        *(UA_Int32 *)data = duk_require_int(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_UINT32)
        *(UA_UInt32 *)data = duk_require_int(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_FLOAT)
        *(UA_Float *)data = duk_require_number(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_DOUBLE)
        *(UA_Double *)data = duk_require_number(ctx, hasAttribute ? 1 : 0);
    else if(type == UA_TYPES_QUALIFIEDNAME)
    {
        duk_size_t len;
        const char *str = duk_require_lstring(ctx, hasAttribute ? 1 : 0, &len);
        ((UA_QualifiedName *)data)->name.data = (unsigned char *)str;
        ((UA_QualifiedName *)data)->name.length = len;
    }
    else if(type == UA_TYPES_LOCALIZEDTEXT)
    {
        duk_size_t len;
        const char *str = duk_require_lstring(ctx, hasAttribute ? 1 : 0, &len);
        ((UA_LocalizedText *)data)->text.data = (unsigned char *)str;
        ((UA_LocalizedText *)data)->text.length = len;
    }
    else if(type == UA_TYPES_STRING)
    {
        duk_size_t len;
        const char *str = duk_require_lstring(ctx, hasAttribute ? 1 : 0, &len);
        ((UA_String *)data)->data = (unsigned char *)str;
        ((UA_String *)data)->length = len;
    }
    else if(type == UA_TYPES_BYTESTRING)
    {
        duk_size_t len;
        void *data = duk_require_buffer_data(ctx, hasAttribute ? 1 : 0, &len);
        ((UA_ByteString *)data)->data = (UA_Byte *)data;
        ((UA_ByteString *)data)->length = len;
    }
    else
        duk_reference_error(ctx, "unknown value type");

    item.value.hasValue = true;
    UA_WriteRequest request;
    UA_WriteRequest_init(&request);
    request.nodesToWrite = &item;
    request.nodesToWriteSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_WRITEREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_WRITERESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    item.value.value.data = NULL;
    item.value.hasValue = false;
    UA_WriteValue_deleteMembers(&item);
    request.nodesToWrite = NULL;
    request.nodesToWriteSize = 0;
    UA_WriteRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_WRITE, reqID, &UA_TYPES[UA_TYPES_WRITERESPONSE], 0, low_add_stash(low->duk_ctx, hasAttribute ? 3 : 2));
    return 0;
}


// -----------------------------------------------------------------------------
//  opcua_uaclient_call
// -----------------------------------------------------------------------------

int opcua_uaclient_call(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_this(ctx);
    if(!duk_get_prop_string(ctx, -1, "\xff""nativeObj"))
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    duk_size_t buf_len;
    LowOPCUA *opcua = *(LowOPCUA **)duk_require_buffer_data(ctx, -1, &buf_len);
    if(!opcua)
        duk_reference_error(ctx, "OPC-UA object is already destroyed");

    if(opcua->mConnectState != 2)
        duk_reference_error(ctx, "OPC-UA object is not connected yet");
    if(opcua->mDisabledState == 2)
        duk_reference_error(ctx, "OPC-UA object threw an error, please destroy it");

    duk_get_prop_string(ctx, -2, "namespace");
    int namespac = duk_require_int(ctx, -1);
    duk_get_prop_string(ctx, -3, "node");
    int node = duk_require_int(ctx, -1);

    UA_UInt32 reqID = 0;
    UA_CallMethodRequest item;
    UA_CallMethodRequest_init(&item);
    item.objectId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    item.methodId = UA_NODEID_NUMERIC(namespac, node);
    item.inputArgumentsSize = 0;
    UA_CallRequest request;
    UA_CallRequest_init(&request);
    request.methodsToCall = &item;
    request.methodsToCallSize = 1;

    pthread_mutex_lock(&opcua->mMutex);
    int state = __UA_Client_AsyncServiceEx(opcua->mClient, &request, &UA_TYPES[UA_TYPES_CALLREQUEST],
                             opcua->OnTaskCallback, &UA_TYPES[UA_TYPES_CALLRESPONSE],
                             opcua, &reqID, 0/* timeout handled by us */, 0/* do not delete data */);
    UA_CallMethodRequest_deleteMembers(&item);
    request.methodsToCall = NULL;
    request.methodsToCallSize = 0;
    UA_CallRequest_deleteMembers(&request);
    if((state & 0x80000000) || !reqID)
    {
        opcua->mError = state;
        if(opcua->mDisabledState == 0)
            opcua->mDisabledState = 1;

        pthread_mutex_unlock(&opcua->mMutex);
        low_web_set_poll_events(low, opcua, 0);
        low_loop_set_callback(low, opcua);
        return 0;
    }
    opcua->AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_CALL, reqID, &UA_TYPES[UA_TYPES_CALLRESPONSE], low_add_stash(low->duk_ctx, 1), low_add_stash(low->duk_ctx, 0));
    return 0;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::LowOPCUA
// -----------------------------------------------------------------------------

static void clientExecuteRepeatedCallback(LowOPCUA *opcua, UA_ApplicationCallback cb, void *callbackApplication, void *data)
{
    int state = cb(callbackApplication, data);
    if(opcua->mConnectState == 0 && (state & 0x80000000))
    {
        opcua->mError = state;
        opcua->mConnectState = 1;
    }
}

LowOPCUA::LowOPCUA(low_t *low, UA_Client *client, int thisIndex, int timeoutMS, const char *url)
    : LowLoopCallback(low), LowFD(low, LOWFD_TYPE_SOCKET, client->connection.sockfd), mLow(low), mTimeoutMS(timeoutMS), mThisIndex(thisIndex), mWriteBuffer(NULL), mClient(client), mMutex(PTHREAD_MUTEX_INITIALIZER), mConnectState(0), mDisabledState(0), mDetachedState(0), mLastClientHandle(0)
{
    low->run_ref++;
    client->connection.lowOPCUAData = this;

    mError = UA_Client_connect_async(client, url, OnConnect, this);
    if(mError != UA_STATUSCODE_GOOD || mClient->connection.sockfd <= 0)
    {
        mDisabledState = 1;
        low_loop_set_callback(mLow, this);
        return;
    }
    SetFD(mClient->connection.sockfd);
    AdvertiseFD();  // so it gets removed on reset

    UA_DateTime now = UA_DateTime_nowMonotonic();
    UA_DateTime next = UA_Timer_process(&client->timer, now, (UA_TimerExecutionCallback)clientExecuteRepeatedCallback, this);
    if((signed)next != -1)
        mChoreIndex = low_set_timeout(mLow->duk_ctx, 0, (next - now) / 1000000, OnTimeout, this);
    if(mConnectState == 1)
        low_loop_set_callback(mLow, this);
}


// -----------------------------------------------------------------------------
//  LowOPCUA::~LowOPCUA
// -----------------------------------------------------------------------------

LowOPCUA::~LowOPCUA()
{
    low_web_clear_poll(mLow, this);
    low_clear_timeout(mLow->duk_ctx, mChoreIndex);

    UA_Client_delete(mClient);
    while(mWriteBuffer)
    {
        unsigned char *next = *(unsigned char **)mWriteBuffer;
        low_free(mWriteBuffer);
        mWriteBuffer = next;
    }

    int callDestroy = 0;
    for(auto iter = mTasks.begin(); iter != mTasks.end(); iter++)
    {
        if(iter->second.result)
        {
            UA_deleteMembers(iter->second.result, iter->second.resultType);
            low_free(iter->second.result);
        }
        if(iter->second.type == LOWOPCTASK_TYPE_DESTROY && iter->second.callbackStashIndex)
            callDestroy = iter->second.callbackStashIndex;
        else
            low_remove_stash(mLow->duk_ctx, iter->second.callbackStashIndex);
        low_remove_stash(mLow->duk_ctx, iter->second.objStashIndex);
        low_remove_stash(mLow->duk_ctx, iter->second.objStashIndex2);
        low_clear_timeout(mLow->duk_ctx, iter->second.timeoutChoreIndex);
    }
    for(auto iter = mMonitoredItems.begin(); iter != mMonitoredItems.end(); iter++)
    {
        low_remove_stash(mLow->duk_ctx, iter->second.first);
        low_remove_stash(mLow->duk_ctx, iter->second.second);
    }

    low_remove_stash(mLow->duk_ctx, mThisIndex);
    mLow->run_ref--;

    if(callDestroy)
    {
        low_push_stash(mLow->duk_ctx, callDestroy, true);
        duk_push_null(mLow->duk_ctx);
        duk_call(mLow->duk_ctx, 1);
    }
}


// -----------------------------------------------------------------------------
//  LowOPCUA::DisconnectAndDetach
// -----------------------------------------------------------------------------

void LowOPCUA::DisconnectAndDetach(int callbackStashIndex)
{
    mClient->publishNotificationCallback = NULL;

    pthread_mutex_lock(&mMutex);
    if(mConnectState != 0 && mDisabledState == 0)
    {
        UA_UInt32 reqID = 0;
        int state = UA_Client_disconnect_async(mClient, &reqID);
        if(!(state & 0x80000000) && reqID)
        {
            // With communication
            mDetachedState = 1;
            AddAsyncRequestAndUnlock(LOWOPCTASK_TYPE_DESTROY, reqID, NULL, 0, callbackStashIndex);
            return;
        }
    }

    // Without communication
    mDetachedState = 2;
    pthread_mutex_unlock(&mMutex);
    low_loop_set_callback(mLow, this);
}


// -----------------------------------------------------------------------------
//  LowOPCUA::AddAsyncRequestAndUnlock
// -----------------------------------------------------------------------------

void LowOPCUA::AddAsyncRequestAndUnlock(int type, unsigned int reqID, const UA_DataType *resultType, int objStashIndex, int callbackStashIndex, int objStashIndex2, int clientHandle)
{
    LowOPCUATask &task = mTasks[reqID];
    task.opcua = this;
    task.result = NULL;
    task.id = reqID;
    task.type = type;
    task.resultType = resultType;
    task.objStashIndex = objStashIndex;
    task.objStashIndex2 = objStashIndex2;
    task.clientHandle = clientHandle;
    task.callbackStashIndex = callbackStashIndex;
    task.timeoutChoreIndex = low_set_timeout(mLow->duk_ctx, 0, mTimeoutMS, OnTaskTimeout, &task);
    pthread_mutex_unlock(&mMutex);
    if(FD() >= 0)
        low_web_set_poll_events(mLow, this, mDisabledState || mDetachedState == 2 ? 0 : (mWriteBuffer || mConnectState == 0 ? POLLOUT | POLLIN | POLLERR : POLLIN | POLLERR));
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
    if(state & 0x80000000)
        opcua->mError = state;
    opcua->mConnectState = 1;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnTimeout
// -----------------------------------------------------------------------------

void LowOPCUA::OnTimeout(duk_context *ctx, void *data)
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

    UA_DateTime now = UA_DateTime_nowMonotonic();
    UA_DateTime next = UA_Timer_process(&opcua->mClient->timer, now, (UA_TimerExecutionCallback)clientExecuteRepeatedCallback, opcua);
    pthread_mutex_unlock(&opcua->mMutex);

    if((signed)next != -1)
    {
        int ms = (next - now) / 1000000;
        if(ms < 20)
            ms = 20;
        opcua->mChoreIndex = low_set_timeout(opcua->mLow->duk_ctx, opcua->mChoreIndex, ms, OnTimeout, data);
    }
    if(opcua->mConnectState == 1 || opcua->mDisabledState == 1)
        low_loop_set_callback(opcua->mLow, opcua);
    if(opcua->FD() >= 0)
        low_web_set_poll_events(opcua->mLow, opcua, opcua->mDisabledState || opcua->mDetachedState == 2 ? 0 : (opcua->mWriteBuffer || opcua->mConnectState == 0 ? POLLOUT | POLLIN | POLLERR : POLLIN | POLLERR));
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnTaskCallback
// -----------------------------------------------------------------------------

void LowOPCUA::OnTaskCallback(
    UA_Client *client, void *userdata, unsigned int reqID, void *responsedata)
{
    LowOPCUA *opcua = (LowOPCUA *)userdata;

    auto iter = opcua->mTasks.find(reqID);
    if(iter != opcua->mTasks.end() && !iter->second.result)
    {
        iter->second.result = responsedata;
        opcua->mGoToLoop = true;
    }
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnTaskTimeout
// -----------------------------------------------------------------------------

void LowOPCUA::OnTaskTimeout(duk_context *ctx, void *data)
{
    LowOPCUATask *task = (LowOPCUATask *)data;
    LowOPCUA *opcua = task->opcua;

    if(task->result || opcua->mDisabledState)
        return;
    pthread_mutex_lock(&opcua->mMutex);
    if(task->result || opcua->mDisabledState)
    {
        pthread_mutex_unlock(&opcua->mMutex);
        return;
    }

    if(task->type == LOWOPCTASK_TYPE_DESTROY)
        opcua->mDetachedState = 2;

    int callback = task->callbackStashIndex;
    low_clear_timeout(opcua->mLow->duk_ctx, task->timeoutChoreIndex);
    low_remove_stash(opcua->mLow->duk_ctx, task->objStashIndex);
    low_remove_stash(opcua->mLow->duk_ctx, task->objStashIndex2);
    opcua->mTasks.erase(task->id);

    pthread_mutex_unlock(&opcua->mMutex);
    if(callback)
    {
        low_push_stash(opcua->mLow->duk_ctx, task->callbackStashIndex, true);
        duk_push_error_object(opcua->mLow->duk_ctx, DUK_ERR_ERROR, "timeout");
        duk_call(opcua->mLow->duk_ctx, 1);
    }
    if(opcua->mDetachedState == 2)
        low_loop_set_callback(opcua->mLow, opcua);
    else
        opcua->OnLoop();
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnPublishNotification
// -----------------------------------------------------------------------------

UA_StatusCode LowOPCUA::OnPublishNotification(struct UA_Client *client, UA_ExtensionObject *msg, void *data)
{
    LowOPCUA *opcua = (LowOPCUA *)data;
    if(msg->encoding != UA_EXTENSIONOBJECT_DECODED)
        return UA_STATUSCODE_GOOD;

    /* Handle DataChangeNotification */
    if(msg->content.decoded.type == &UA_TYPES[UA_TYPES_DATACHANGENOTIFICATION] && ((UA_DataChangeNotification *)msg->content.decoded.data)->monitoredItemsSize) {
        UA_DataChangeNotification *dataChangeNotification =
            (UA_DataChangeNotification *)msg->content.decoded.data;
        for(size_t j = 0; j < dataChangeNotification->monitoredItemsSize; ++j) {
            UA_MonitoredItemNotification *min = &dataChangeNotification->monitoredItems[j];
            UA_MonitoredItemNotification *n2 = (UA_MonitoredItemNotification *)low_alloc(sizeof(UA_MonitoredItemNotification));
            if(n2)
            {
                UA_MonitoredItemNotification_copy(min, n2);
                opcua->mDataChangeNotifications.push(n2);
            }
        }
        low_loop_set_callback(opcua->mLow, opcua);
    }
/*
    * Handle EventNotification *
    if(msg->content.decoded.type == &UA_TYPES[UA_TYPES_EVENTNOTIFICATIONLIST]) {
        UA_EventNotificationList *eventNotificationList =
            (UA_EventNotificationList *)msg->content.decoded.data;
        processEventNotification(client, sub, eventNotificationList);
        return;
    }

    * Handle StatusChangeNotification *
    if(msg->content.decoded.type == &UA_TYPES[UA_TYPES_STATUSCHANGENOTIFICATION]) {
        if(sub->statusChangeCallback) {
            sub->statusChangeCallback(client, sub->subscriptionId, sub->context,
                                      (UA_StatusChangeNotification*)msg->content.decoded.data);
        } else {
            UA_LOG_WARNING(&client->config.logger, UA_LOGCATEGORY_CLIENT,
                           "Dropped a StatusChangeNotification since no callback is registered");
        }
        return;
    }

    UA_LOG_WARNING(&client->config.logger, UA_LOGCATEGORY_CLIENT,
                   "Unknown notification message type");
*/
    return UA_STATUSCODE_GOOD;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnLoop
// -----------------------------------------------------------------------------

bool LowOPCUA::OnLoop()
{
    if((mConnectState == 1 && (mError & 0x80000000) && mDisabledState != 2) || mDisabledState == 1)
    {
        duk_context *ctx = low_get_duk_context(mLow);
        low_push_stash(mLow->duk_ctx, mThisIndex, false);
        duk_push_string(ctx, "emit");
        duk_push_string(ctx, "error");

        duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(mError));
        duk_push_int(mLow->duk_ctx, mError);
        duk_put_prop_string(mLow->duk_ctx, -2, "open62541_statuscode");

        duk_call_prop(ctx, -4, 2);
        mDisabledState = 2;
        return false;
    }

    if(mDisabledState || mDetachedState)
        return !((mDetachedState && mDisabledState) || mDetachedState == 2);

    if(mConnectState == 1)
    {
        duk_context *ctx = low_get_duk_context(mLow);
        low_push_stash(mLow->duk_ctx, mThisIndex, false);
        duk_push_string(ctx, "emit");
        duk_push_string(ctx, "connect");
        mConnectState = 2;
        duk_call_prop(ctx, -3, 1);
    }

    pthread_mutex_lock(&mMutex);
    int max = mTasks.size();
    int n = 0;
    auto iter = mTasks.begin();
    for(; iter != mTasks.end() && n < max; n++)
    {
        if(iter->second.type == LOWOPCTASK_TYPE_DESTROY || !iter->second.result)
        {
            iter++;
            continue;
        }

        LowOPCUATask task = iter->second;

        auto iter2 = iter;
        iter++;
        mTasks.erase(iter2);

        pthread_mutex_unlock(&mMutex);
        while(duk_get_top(mLow->duk_ctx))
            duk_pop(mLow->duk_ctx);

        low_clear_timeout(mLow->duk_ctx, task.timeoutChoreIndex);
        if(task.type == LOWOPCTASK_TYPE_LOOKUP_PROPS)
        {
            UA_ReadResponse *response = (UA_ReadResponse *)task.result;
            if(response->resultsSize != 3)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                PushVariant(&response->results[0].value);
                duk_put_prop_string(mLow->duk_ctx, -2, "browseName");
                PushVariant(&response->results[1].value);
                duk_put_prop_string(mLow->duk_ctx, -2, "displayName");
                PushVariant(&response->results[2].value);
                duk_put_prop_string(mLow->duk_ctx, -2, "description");

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_null(mLow->duk_ctx);
                    duk_dup(mLow->duk_ctx, -3);
                    duk_call(mLow->duk_ctx, 2);
                }
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_SUBNODE && task.callbackStashIndex)
        {
            UA_TranslateBrowsePathsToNodeIdsResponse *response = (UA_TranslateBrowsePathsToNodeIdsResponse *)task.result;
            if(!response->resultsSize)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else if(!response->results[0].targetsSize)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0].statusCode));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_null(mLow->duk_ctx);

                duk_push_object(mLow->duk_ctx);

                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_put_prop_string(mLow->duk_ctx, -2, "\xff""nativeObj");

                opcua_fill_node(mLow->duk_ctx);

                duk_push_int(mLow->duk_ctx, response->results[0].targets[0].targetId.nodeId.namespaceIndex);
                duk_put_prop_string(mLow->duk_ctx, -2, "namespace");
                duk_push_int(mLow->duk_ctx, response->results[0].targets[0].targetId.nodeId.identifier.numeric);
                duk_put_prop_string(mLow->duk_ctx, -2, "node");

                duk_call(mLow->duk_ctx, 2);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_CHILDREN && task.callbackStashIndex)
        {
            duk_push_array(mLow->duk_ctx);
            int n = 0;
            UA_BrowseResponse *response = (UA_BrowseResponse *)task.result;
            for(size_t i = 0; i < response->resultsSize; ++i)
                for(size_t j = 0; j < response->results[i].referencesSize; ++j)
                {
                    UA_ReferenceDescription *ref = &(response->results[i].references[j]);
                    duk_push_object(mLow->duk_ctx);

                    low_push_stash(mLow->duk_ctx, task.objStashIndex, false);
                    duk_put_prop_string(mLow->duk_ctx, -2, "\xff""nativeObj");

                    opcua_fill_node(mLow->duk_ctx);

                    duk_push_int(mLow->duk_ctx, ref->nodeId.nodeId.namespaceIndex);
                    duk_put_prop_string(mLow->duk_ctx, -2, "namespace");
                    duk_push_int(mLow->duk_ctx, ref->nodeId.nodeId.identifier.numeric);
                    duk_put_prop_string(mLow->duk_ctx, -2, "node");
                    duk_push_lstring(mLow->duk_ctx, (char *)ref->browseName.name.data, ref->browseName.name.length);
                    duk_put_prop_string(mLow->duk_ctx, -2, "browseName");
                    duk_push_lstring(mLow->duk_ctx, (char *)ref->displayName.text.data, ref->displayName.text.length);
                    duk_put_prop_string(mLow->duk_ctx, -2, "displayName");

                    duk_put_prop_index(mLow->duk_ctx, -2, n++);
                }
            low_remove_stash(mLow->duk_ctx, task.objStashIndex);

            low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
            if(n == 0 && (response->responseHeader.serviceResult & 0x80000000))
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else if(n == 0 && response->resultsSize && (response->results[0].statusCode & 0x80000000))
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0].statusCode));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                duk_push_null(mLow->duk_ctx);
                duk_dup(mLow->duk_ctx, -3);
                duk_call(mLow->duk_ctx, 2);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_READ && task.callbackStashIndex)
        {
            low_remove_stash(mLow->duk_ctx, task.objStashIndex);
            low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);

            UA_ReadResponse *response = (UA_ReadResponse *)task.result;
            if(!response->resultsSize)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else if(response->results[0].hasStatus && (response->results[0].status & 0x80000000))
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0].status));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                duk_push_null(mLow->duk_ctx);
                PushVariant(&response->results[0].value);
                duk_call(mLow->duk_ctx, 2);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_WRITE && task.callbackStashIndex)
        {
            low_remove_stash(mLow->duk_ctx, task.objStashIndex);
            low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);

            UA_WriteResponse *response = (UA_WriteResponse *)task.result;
            if(!response->resultsSize)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else if(response->results[0] & 0x80000000)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0]));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                duk_push_null(mLow->duk_ctx);
                duk_call(mLow->duk_ctx, 1);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_CALL && task.callbackStashIndex)
        {
            low_remove_stash(mLow->duk_ctx, task.objStashIndex);
            low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);

            UA_CallResponse *response = (UA_CallResponse *)task.result;
            if(!response->resultsSize)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else if(response->results[0].statusCode & 0x80000000)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0].statusCode));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                duk_push_null(mLow->duk_ctx);
                duk_call(mLow->duk_ctx, 1);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_CREATE_SUBSCRIPTION && task.callbackStashIndex)
        {
            UA_CreateSubscriptionResponse *response = (UA_CreateSubscriptionResponse *)task.result;
            if(response->responseHeader.serviceResult & 0x80000000)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_null(mLow->duk_ctx);

                duk_push_object(mLow->duk_ctx);

                duk_push_int(mLow->duk_ctx, 0);
                duk_put_prop_string(mLow->duk_ctx, -2, "\xff""itemCount");

                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_dup(mLow->duk_ctx, -1);
                duk_put_prop_string(mLow->duk_ctx, -3, "\xff""client");
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""nativeObj");
                duk_put_prop_string(mLow->duk_ctx, -3, "\xff""nativeObj");
                duk_pop(mLow->duk_ctx);

                duk_push_string(mLow->duk_ctx, "name");
                duk_push_string(mLow->duk_ctx, "UAClientSubscription");
                duk_def_prop(mLow->duk_ctx, -3, DUK_DEFPROP_HAVE_VALUE);

                duk_function_list_entry methods[] = {{"destroy", opcua_uaclient_destroy_subscription, 1},
                                                    {"add", opcua_uaclient_subscription_add, 2},
                                                    {"remove", opcua_uaclient_subscription_remove, 2},
                                                    {NULL, NULL, 0}};
                duk_put_function_list(mLow->duk_ctx, -1, methods);

                duk_push_uint(mLow->duk_ctx, response->subscriptionId);
                duk_put_prop_string(mLow->duk_ctx, -2, "\xff""id");

                duk_call(mLow->duk_ctx, 2);
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_DESTROY_SUBSCRIPTION)
        {
            UA_DeleteSubscriptionsResponse *response = (UA_DeleteSubscriptionsResponse *)task.result;
            if(!response->resultsSize)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else if(response->results[0] & 0x80000000)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0]));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_del_prop_string(mLow->duk_ctx, -1, "\xff""id");

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_null(mLow->duk_ctx);
                    duk_call(mLow->duk_ctx, 1);
                }
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_SUBSCRIPTION_ADD)
        {
            UA_CreateMonitoredItemsResponse *response = (UA_CreateMonitoredItemsResponse *)task.result;
            if(!response->resultsSize)
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""itemCount");
                duk_push_int(mLow->duk_ctx, duk_require_int(mLow->duk_ctx, -1) - 1);
                duk_put_prop_string(mLow->duk_ctx, -3, "\xff""itemCount");
                low_remove_stash(mLow->duk_ctx, task.objStashIndex2);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else if(response->results[0].statusCode & 0x80000000)
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""itemCount");
                duk_push_int(mLow->duk_ctx, duk_require_int(mLow->duk_ctx, -1) - 1);
                duk_put_prop_string(mLow->duk_ctx, -3, "\xff""itemCount");
                low_remove_stash(mLow->duk_ctx, task.objStashIndex2);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0].statusCode));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""client");
                mMonitoredItems[task.clientHandle] = std::pair<int, int>(low_add_stash(mLow->duk_ctx, duk_get_top_index(mLow->duk_ctx)), task.objStashIndex2);
                if(!mClient->publishNotificationCallback && !mDetachedState)
                {
                    mClient->publishNotificationCallback = OnPublishNotification;
                    pthread_mutex_lock(&mMutex);
                    UA_Client_Subscriptions_backgroundPublish(mClient);
                    pthread_mutex_unlock(&mMutex);
                }
                low_push_stash(mLow->duk_ctx, task.objStashIndex2, false);
                duk_push_uint(mLow->duk_ctx, response->results[0].monitoredItemId);
                duk_put_prop_string(mLow->duk_ctx, -2, "\xff""monitoredItemID");
                duk_push_uint(mLow->duk_ctx, task.clientHandle);
                duk_put_prop_string(mLow->duk_ctx, -2, "\xff""clientHandle");

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_null(mLow->duk_ctx);
                    duk_call(mLow->duk_ctx, 1);
                }
            }
        }
        else if(task.type == LOWOPCTASK_TYPE_SUBSCRIPTION_REMOVE)
        {
            UA_DeleteMonitoredItemsResponse *response = (UA_DeleteMonitoredItemsResponse *)task.result;
            if(!response->resultsSize)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                low_remove_stash(mLow->duk_ctx, task.objStashIndex2);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->responseHeader.serviceResult));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else if(response->results[0] & 0x80000000)
            {
                low_remove_stash(mLow->duk_ctx, task.objStashIndex);
                low_remove_stash(mLow->duk_ctx, task.objStashIndex2);

                if(task.callbackStashIndex)
                {
                    low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                    duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, UA_StatusCode_name(response->results[0]));
                    duk_call(mLow->duk_ctx, 1);
                }
            }
            else
            {
                low_push_stash(mLow->duk_ctx, task.objStashIndex2, true);
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""itemCount");
                duk_push_int(mLow->duk_ctx, duk_require_int(mLow->duk_ctx, -1) - 1);
                duk_put_prop_string(mLow->duk_ctx, -3, "\xff""itemCount");

                low_push_stash(mLow->duk_ctx, task.objStashIndex, true);
                duk_get_prop_string(mLow->duk_ctx, -1, "\xff""monitoredItemID");
                int monitoredItemID = duk_require_uint(mLow->duk_ctx, -1);
                duk_get_prop_string(mLow->duk_ctx, -2, "\xff""clientHandle");
                int clientHandle = duk_require_uint(mLow->duk_ctx, -1);

                auto iter = mMonitoredItems.find(clientHandle);
                if(iter == mMonitoredItems.end())
                {
                    if(task.callbackStashIndex)
                    {
                        low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                        duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, "node property was tampered with");
                        duk_call(mLow->duk_ctx, 1);
                    }
                }
                else
                {
                    low_remove_stash(mLow->duk_ctx, iter->second.first);
                    low_remove_stash(mLow->duk_ctx, iter->second.second);
                    mMonitoredItems.erase(iter);
                    mClient->publishNotificationCallback = mDetachedState || mMonitoredItems.size() == 0 ? NULL : OnPublishNotification;
                    duk_del_prop_string(mLow->duk_ctx, -1, "\xff""monitoredItemID");

                    if(task.callbackStashIndex)
                    {
                        low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                        duk_push_null(mLow->duk_ctx);
                        duk_call(mLow->duk_ctx, 1);
                    }
                }
            }
        }
        else
        {
            low_remove_stash(mLow->duk_ctx, task.objStashIndex);
            low_remove_stash(mLow->duk_ctx, task.objStashIndex2);
            if(task.callbackStashIndex)
            {
                low_push_stash(mLow->duk_ctx, task.callbackStashIndex, true);
                duk_push_null(mLow->duk_ctx);
                duk_call(mLow->duk_ctx, 1);
            }
        }
        UA_deleteMembers(task.result, task.resultType);
        low_free(task.result);

        pthread_mutex_lock(&mMutex);
    }
    bool skipped = iter != mTasks.end();
    max = mDataChangeNotifications.size();
    for(int n = 0; n < max && mDataChangeNotifications.size(); n++)
    {
        UA_MonitoredItemNotification *min = mDataChangeNotifications.front();
        mDataChangeNotifications.pop();
        pthread_mutex_unlock(&mMutex);
        std::map<UA_UInt32, std::pair<int, int> >::iterator iter = mMonitoredItems.find(min->clientHandle);
        if(iter != mMonitoredItems.end())
        {
            while(duk_get_top(mLow->duk_ctx))
                duk_pop(mLow->duk_ctx);
            low_push_stash(mLow->duk_ctx, iter->second.first, false);
            duk_push_string(mLow->duk_ctx, "emit");
            duk_push_string(mLow->duk_ctx, "dataChanged");
            low_push_stash(mLow->duk_ctx, iter->second.second, false);
            PushVariant(&min->value.value);
            duk_call_prop(mLow->duk_ctx, -5, 3);
        }
        pthread_mutex_lock(&mMutex);
        UA_deleteMembers(min, &UA_TYPES[UA_TYPES_MONITOREDITEMNOTIFICATION]);
        low_free(min);
    }
    pthread_mutex_unlock(&mMutex);

    if(skipped || mDataChangeNotifications.size())
        low_loop_set_callback(mLow, this);
    return true;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnEvents
// -----------------------------------------------------------------------------

bool LowOPCUA::OnEvents(short events)
{
    if(mDisabledState)
    {
        low_web_set_poll_events(mLow, this, 0);
        return true;
    }

    pthread_mutex_lock(&mMutex);
    if(mDisabledState)
    {
        pthread_mutex_unlock(&mMutex);
        low_web_set_poll_events(mLow, this, 0);
        return true;
    }

    while(mWriteBuffer)
    {
        int len = (mWriteBuffer == mWriteBufferLast ? mWriteBufferLastLen : LOW_OPCUA_WRITEBUFFER_SIZE) - mWriteBufferRead;

        int size = send(FD(), mWriteBuffer + sizeof(void *), len, 0);
        if(size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if(size == -1)
        {
            mError = UA_STATUSCODE_BADNOTCONNECTED;
            if(mDisabledState == 0)
                mDisabledState = 1;
            pthread_mutex_unlock(&mMutex);

            low_web_set_poll_events(mLow, this, 0);
            low_loop_set_callback(mLow, this);
            return true;
        }

        if(size != len)
        {
            mWriteBufferRead += size;
            break;
        }

        unsigned char *next = *(unsigned char **)mWriteBuffer;
        low_free(mWriteBuffer);
        mWriteBuffer = next;
        mWriteBufferRead = 0;
    }

    mGoToLoop = false;
    if(mDetachedState != 2)
    {
        int state = UA_Client_run_iterate(mClient, 0);
        if(state & 0x80000000)
        {
            mError = state;
            if(mDisabledState == 0)
                mDisabledState = 1;

            pthread_mutex_unlock(&mMutex);
            low_web_set_poll_events(mLow, this, 0);
            low_loop_set_callback(mLow, this);
            return true;
        }
    }

    pthread_mutex_unlock(&mMutex);
    if(mDetachedState == 1 && !mWriteBuffer && mClient->state == UA_CLIENTSTATE_DISCONNECTED)
        mDetachedState = 2;
    if(mConnectState == 1 || mDisabledState == 1 || mDetachedState == 2 || mGoToLoop)
        low_loop_set_callback(mLow, this);
    if(FD() >= 0)
        low_web_set_poll_events(mLow, this, mDisabledState || mDetachedState == 2 ? 0 : (mWriteBuffer || mConnectState == 0 ? POLLOUT | POLLIN | POLLERR : POLLIN | POLLERR));

    return true;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::OnSend
// -----------------------------------------------------------------------------

int LowOPCUA::OnSend(void *data, unsigned char *buf, int len)
{
    LowOPCUA *opcua = (LowOPCUA *)data;

    if(!opcua->mWriteBuffer)
    {
        int size = send(opcua->FD(), buf, len, 0);
        if(size == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            return UA_STATUSCODE_BADNOTCONNECTED;

        if(size > 0)
        {
            buf += size;
            len -= size;
        }
    }

    while(len)
    {
        if(opcua->mWriteBuffer && opcua->mWriteBufferLastLen != LOW_OPCUA_WRITEBUFFER_SIZE)
        {
            int size = LOW_OPCUA_WRITEBUFFER_SIZE - opcua->mWriteBufferLastLen;
            if(size > len)
                size = len;

            memcpy(opcua->mWriteBufferLast + sizeof(void *) + opcua->mWriteBufferLastLen, buf, size);
            opcua->mWriteBufferLastLen += size;
            buf += size;
            len -= size;
        }
        else
        {
            int size = LOW_OPCUA_WRITEBUFFER_SIZE;
            if(size > len)
                size = len;

            unsigned char *buffer = (unsigned char *)low_alloc(sizeof(void *) + LOW_OPCUA_WRITEBUFFER_SIZE);
            if(!buffer)
                return UA_STATUSCODE_BADOUTOFMEMORY;
            *(unsigned char **)buffer = NULL;
            opcua->mWriteBufferLastLen = 0;
            if(opcua->mWriteBuffer)
            {
                *(unsigned char **)opcua->mWriteBufferLast = buffer;
                opcua->mWriteBufferLast = buffer;
            }
            else
            {
                opcua->mWriteBuffer = opcua->mWriteBufferLast = buffer;
                opcua->mWriteBufferRead = 0;
            }
            memcpy(buffer + sizeof(void *), buf, size);
            opcua->mWriteBufferLastLen = size;
            buf += size;
            len -= size;
        }
    }

    return 0;
}


// -----------------------------------------------------------------------------
//  LowOPCUA::PushVariant
// -----------------------------------------------------------------------------

void LowOPCUA::PushVariant(UA_Variant *variant)
{
    if(variant->type == NULL)
        duk_push_undefined(mLow->duk_ctx);
    else if(variant->type == &UA_TYPES[UA_TYPES_BOOLEAN])
    {
        UA_Boolean *type = (UA_Boolean *)variant->data;
        duk_push_boolean(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_SBYTE])
    {
        UA_SByte *type = (UA_SByte *)variant->data;
        duk_push_int(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_BYTE])
    {
        UA_Byte *type = (UA_Byte *)variant->data;
        duk_push_uint(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_INT16])
    {
        UA_Int16 *type = (UA_Int16 *)variant->data;
        duk_push_int(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_UINT16])
    {
        UA_UInt16 *type = (UA_UInt16 *)variant->data;
        duk_push_uint(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_INT32])
    {
        UA_Int32 *type = (UA_Int32 *)variant->data;
        duk_push_int(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_UINT32])
    {
        UA_UInt32 *type = (UA_UInt32 *)variant->data;
        duk_push_uint(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_FLOAT])
    {
        UA_Float *type = (UA_Float *)variant->data;
        duk_push_number(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_DOUBLE])
    {
        UA_Double *type = (UA_Double *)variant->data;
        duk_push_number(mLow->duk_ctx, *type);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_QUALIFIEDNAME])
    {
        UA_QualifiedName *name = (UA_QualifiedName *)variant->data;
        duk_push_lstring(mLow->duk_ctx, (char *)name->name.data, name->name.length);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])
    {
        UA_LocalizedText *text = (UA_LocalizedText *)variant->data;
        duk_push_lstring(mLow->duk_ctx, (char *)text->text.data, text->text.length);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_STRING])
    {
        UA_String *str = (UA_String *)variant->data;
        duk_push_lstring(mLow->duk_ctx, (char *)str->data, str->length);
    }
    else if(variant->type == &UA_TYPES[UA_TYPES_BYTESTRING])
    {
        UA_ByteString *type = (UA_ByteString *)variant->data;
        memcpy(low_push_buffer(mLow->duk_ctx, type->length), type->data, type->length);
    }
    else
        duk_push_null(mLow->duk_ctx);
}
