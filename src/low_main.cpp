// -----------------------------------------------------------------------------
//  low_main.cpp
// -----------------------------------------------------------------------------

#include "low_main.h"
#include "low_module.h"

#include "low_data_thread.h"
#include "low_web_thread.h"

#include "LowCryptoHash.h"
#include "LowDataCallback.h"
#include "LowFD.h"
#include "LowLoopCallback.h"
#include "LowSocket.h"
#include "LowTLSContext.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_system.h"

#include "low_native_api.h"
#include "low_promise.h"
#include "low_opcua.h"

#include "duktape.h"
#if LOW_INCLUDE_CARES_RESOLVER
#include "../deps/c-ares/ares.h"
#include "LowDNSResolver.h"
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if LOW_ESP32_LWIP_SPECIALITIES
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif /* LOW_ESP32_LWIP_SPECIALITIES */


// Global variables
extern low_system_t g_low_system;


// -----------------------------------------------------------------------------
//  low_init
// -----------------------------------------------------------------------------

low_t *low_init()
{
#if LOW_INCLUDE_CARES_RESOLVER
    int err = ares_library_init_mem(
      ARES_LIB_INIT_ALL, low_alloc, low_free, low_realloc);
    if(err)
    {
        fprintf(stderr, "C-ares error: %s\n", ares_strerror(err));
        return NULL;
    }
#endif /* LOW_INCLUDE_CARES_RESOLVER */

    low_t *low = new low_t();
    if(!low)
    {
        fprintf(stderr, "Memory full\n");

#if LOW_INCLUDE_CARES_RESOLVER
        ares_library_cleanup();
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        return NULL;
    }

#if !LOW_ESP32_LWIP_SPECIALITIES
    low->heap_size = 0;
    low->max_heap_size = 512 * 1024 * 1024;
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    low->in_gc = false;
    low->disallow_native = false;

    low->web_thread = NULL;
    for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
        low->data_thread[i] = NULL;

    low->destroying = false;
    low->duk_flag_stop = 0;
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    // we are doing this on reset
    low->duk_ctx = NULL;
#else
    low->duk_ctx = duk_create_heap(
      low_duk_alloc, low_duk_realloc, low_duk_free, low, NULL);
    if(!low->duk_ctx)
    {
        fprintf(stderr, "Cannot initialize Duktape heap\n");

        low_free(low);
#if LOW_INCLUDE_CARES_RESOLVER
        ares_library_cleanup();
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        return NULL;
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    low->run_ref = 0;
    low->last_stash_index = 0;
    low->signal_call_id = 0;
    low->web_thread_done = false;
    low->data_thread_done = false;
    low->last_chore_time = low_tick_count();
    low->module_transpile_hook = NULL;

    if(pthread_mutex_init(&low->ref_mutex, NULL) != 0)
        goto err;

#if LOW_INCLUDE_CARES_RESOLVER
    if(pthread_mutex_init(&low->resolvers_mutex, NULL) != 0)
    {
        pthread_mutex_destroy(&low->ref_mutex);
        goto err;
    }
    low->resolvers_active = 0;
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#if LOW_ESP32_LWIP_SPECIALITIES
    low->loop_thread_sema = xSemaphoreCreateBinary();
    if(!low->loop_thread_sema)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        goto err;
    }
#else
    if(pthread_cond_init(&low->loop_thread_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        goto err;
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(pthread_mutex_init(&low->loop_thread_mutex, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        goto err;
    }
    low->loop_callback_first = low->loop_callback_last = NULL;

    if(pthread_mutex_init(&low->data_thread_mutex, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);

        goto err;
    }
    if(pthread_cond_init(&low->data_thread_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);

        goto err;
    }

    if(pthread_cond_init(&low->data_thread_done_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);

        goto err;
    }

    low->data_callback_first[0] = low->data_callback_last[0] = NULL;
    low->data_callback_first[1] = low->data_callback_last[1] = NULL;
    for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
    {
#if LOW_ESP32_LWIP_SPECIALITIES
        err = xTaskCreatePinnedToCore((void (*)(void *))low_data_thread_main,
                                      "data",
                                      CONFIG_DATA_THREAD_STACK_SIZE,
                                      low,
                                      CONFIG_DATA_PRIORITY,
                                      &low->data_thread[i], 0);
        if(err != pdPASS)
        {
            fprintf(
              stderr, "failed to create data task, error code: %d\n", err);
#else
        if(pthread_create(
             &low->data_thread[i], NULL, low_data_thread_main, low) != 0)
        {
            pthread_mutex_lock(&low->data_thread_mutex);
            low->destroying = 1;
            pthread_cond_broadcast(&low->data_thread_cond);
            pthread_mutex_unlock(&low->data_thread_mutex);
            for(int j = 0; j < i; j++)
                pthread_join(low->data_thread[j], NULL);

#if LOW_INCLUDE_CARES_RESOLVER
            pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
            pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
            vSemaphoreDelete(low->loop_thread_sema);
#else
            pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            pthread_mutex_destroy(&low->loop_thread_mutex);
            pthread_mutex_destroy(&low->data_thread_mutex);
            pthread_cond_destroy(&low->data_thread_cond);
            pthread_cond_destroy(&low->data_thread_done_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            goto err;
        }
    }

    low->web_thread_done = false;
    low->reset_accepts = false;
#if !LOW_ESP32_LWIP_SPECIALITIES
    if(pipe(low->web_thread_pipe) < 0)
    {
        pthread_mutex_lock(&low->data_thread_mutex);
        low->destroying = 1;
        pthread_cond_broadcast(&low->data_thread_cond);
        pthread_mutex_unlock(&low->data_thread_mutex);
        for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
            pthread_join(low->data_thread[i], NULL);

#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);
        pthread_cond_destroy(&low->data_thread_done_cond);

        goto err;
    }
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    if(pthread_mutex_init(&low->web_thread_mutex, NULL) != 0)
    {
#if !LOW_ESP32_LWIP_SPECIALITIES
        close(low->web_thread_pipe[0]);
        close(low->web_thread_pipe[1]);

        pthread_mutex_lock(&low->data_thread_mutex);
        low->destroying = 1;
        pthread_cond_broadcast(&low->data_thread_cond);
        pthread_mutex_unlock(&low->data_thread_mutex);
        for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
            pthread_join(low->data_thread[i], NULL);

#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);
        pthread_cond_destroy(&low->data_thread_done_cond);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

        goto err;
    }
    if(pthread_cond_init(&low->web_thread_done_cond, NULL) != 0)
    {
#if !LOW_ESP32_LWIP_SPECIALITIES
        close(low->web_thread_pipe[0]);
        close(low->web_thread_pipe[1]);

        pthread_mutex_lock(&low->data_thread_mutex);
        low->destroying = 1;
        pthread_cond_broadcast(&low->data_thread_cond);
        pthread_mutex_unlock(&low->data_thread_mutex);
        for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
            pthread_join(low->data_thread[i], NULL);

#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);
        pthread_cond_destroy(&low->data_thread_done_cond);
        pthread_mutex_destroy(&low->web_thread_mutex);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

        goto err;
    }
#if LOW_ESP32_LWIP_SPECIALITIES
    err = xTaskCreatePinnedToCore((void (*)(void *))low_web_thread_main,
                                  "web",
                                  CONFIG_WEB_THREAD_STACK_SIZE,
                                  low,
                                  CONFIG_WEB_PRIORITY,
                                  &low->web_thread, 0);
    if(err != pdPASS)
    {
        fprintf(stderr, "failed to create web task, error code: %d\n", err);
#else
    if(pthread_create(&low->web_thread, NULL, low_web_thread_main, low) != 0)
    {
        close(low->web_thread_pipe[0]);
        close(low->web_thread_pipe[1]);

        pthread_mutex_lock(&low->data_thread_mutex);
        low->destroying = 1;
        pthread_cond_broadcast(&low->data_thread_cond);
        pthread_mutex_unlock(&low->data_thread_mutex);
        for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
            pthread_join(low->data_thread[i], NULL);

#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
        vSemaphoreDelete(low->loop_thread_sema);
#else
        pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);
        pthread_cond_destroy(&low->data_thread_done_cond);
        pthread_mutex_destroy(&low->web_thread_mutex);
        pthread_cond_destroy(&low->web_thread_done_cond);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

        goto err;
    }

    new LowSocket(low, 0);
    new LowSocket(low, 1);
    new LowSocket(low, 2);
    if(!low->fds[0] || !low->fds[1] || !low->fds[2])
    {
        low_destroy(low);
        return NULL;
    }

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    low->cwd = low_strdup("/");
    if(!low->cwd)
        return NULL;
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
#if !LOW_ESP32_LWIP_SPECIALITIES
    g_low_system.signal_pipe_fd = low->web_thread_pipe[1];
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    low->in_uncaught_exception = false;

    return low;

err:
#if !LOW_ESP32_LWIP_SPECIALITIES
    duk_destroy_heap(low->duk_ctx);
    low_free(low);
#if LOW_INCLUDE_CARES_RESOLVER
    ares_library_cleanup();
#endif /* LOW_INCLUDE_CARES_RESOLVER */
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return NULL;
}

// -----------------------------------------------------------------------------
//  duk_get_low_context
// -----------------------------------------------------------------------------

low_t *duk_get_low_context(duk_context *ctx)
{
    duk_memory_functions funcs;
    duk_get_memory_functions(ctx, &funcs);
    return (low_t *)funcs.udata;
}


// -----------------------------------------------------------------------------
//  low_get_duk_context
// -----------------------------------------------------------------------------

duk_context *low_get_duk_context(low_t *low)
{
    // Used from applications, as low_t is not available there
    return low->duk_ctx;
}


#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)

// -----------------------------------------------------------------------------
//  low_reset
// -----------------------------------------------------------------------------

// Part of lowjs_esp32 source
void duk_copy_breakpoints(duk_context *from, duk_context *to);
extern "C" void alloc_use_fund();

bool low_reset(low_t *low)
{
    low->duk_flag_stop = 1;

    low->destroying = true;
    low_web_thread_break(low);

    // Finish up threads
    pthread_mutex_lock(&low->web_thread_mutex);
    while(!low->web_thread_done)
        pthread_cond_wait(&low->web_thread_done_cond, &low->web_thread_mutex);
    pthread_mutex_unlock(&low->web_thread_mutex);

    pthread_mutex_lock(&low->data_thread_mutex);
    pthread_cond_broadcast(&low->data_thread_cond);
    while(!low->data_thread_done)
        pthread_cond_wait(&low->data_thread_done_cond, &low->data_thread_mutex);
    pthread_mutex_unlock(&low->data_thread_mutex);

    bool hasOne;
    do
    {
        hasOne = false;
        {
            auto elem = low->loop_callback_first;
            while(elem) // before FDs important for
                        // LowDNSResolver!
            {
                if(elem->mLoopClearOnReset)
                {
                    hasOne = true;
                    auto elem2 = elem->mNext;
                    delete elem;
                    elem = elem2;
                    break;
                }
                else
                    elem = elem->mNext;
            }
        }
        {
            auto elem = low->data_callback_first[0];
            while(elem) // before FDs important for
                        // LowDNSResolver!
            {
                if(elem->mDataClearOnReset)
                {
                    hasOne = true;
                    auto elem2 = elem->mNext;
                    delete elem;
                    elem = elem2;
                    break;
                }
                else
                    elem = elem->mNext;
            } 
        }
        {
            auto elem = low->data_callback_first[1];
            while(elem) // before FDs important for
                        // LowDNSResolver!
            {
                if(elem->mDataClearOnReset)
                {
                    hasOne = true;
                    auto elem2 = elem->mNext;
                    delete elem;
                    elem = elem2;
                    break;
                }
                else
                    elem = elem->mNext;
            }
        }
        {
            for(auto iter = low->fds.begin(); iter != low->fds.end();)
            {
                // Web thread already removed them from FD list
                auto iter2 = iter;
                iter++;
                if(iter2->second->mFDClearOnReset)
                {
                    hasOne = true;
                    delete iter2->second;

                    break;
                }
            }
        }
    } while(hasOne);

    low->duk_flag_stop = 0;
    low->destroying = false;

    pthread_mutex_lock(&low->data_thread_mutex);
    low->data_thread_done = false;
    pthread_cond_broadcast(&low->data_thread_cond);
    pthread_mutex_unlock(&low->data_thread_mutex);

    pthread_mutex_lock(&low->web_thread_mutex);
    low->web_thread_done = false;
    pthread_cond_broadcast(&low->web_thread_done_cond);
    pthread_mutex_unlock(&low->web_thread_mutex);

    duk_context *new_ctx = duk_create_heap(
      low_duk_alloc, low_duk_realloc, low_duk_free, low, NULL);
    if(!new_ctx && low->duk_ctx)
    {
        fprintf(stderr, "Cannot create Duktape heap, trying after free\n");
        alloc_use_fund();

        duk_destroy_heap(low->duk_ctx);
        low->duk_ctx = NULL;

        new_ctx = duk_create_heap(
          low_duk_alloc, low_duk_realloc, low_duk_free, low, NULL);
    }
    if(!new_ctx)
    {
        fprintf(stderr, "Cannot create Duktape heap\n");
        return false;
    }

    if(low->duk_ctx)
    {
        duk_copy_breakpoints(low->duk_ctx, new_ctx);
        duk_destroy_heap(low->duk_ctx);
    }

    low_set_raw_mode(false);

    // After finalizers.. they must not use DukTape heap!
#if LOW_INCLUDE_CARES_RESOLVER
    for(int i = 0; i < low->resolvers.size(); i++)
        if(low->resolvers[i])
            delete low->resolvers[i];
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    for(int i = 0; i < low->tlsContexts.size(); i++)
        if(low->tlsContexts[i])
            delete low->tlsContexts[i];
    for(int i = 0; i < low->cryptoHashes.size(); i++)
        if(low->cryptoHashes[i])
            delete low->cryptoHashes[i];

    low->duk_ctx = new_ctx;

    low->chores.clear();
    low->chore_times.clear();
    low->last_chore_time = low_tick_count();

    low->run_ref = 0;
    auto elem = low->loop_callback_first;
    while(elem) // before FDs important for
                // LowDNSResolver!
    {
        low->run_ref++;
        elem = elem->mNext;
    }

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    low_free(low->cwd);
    low->cwd = low_strdup("/");
    if(!low->cwd)
        return false;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    low->in_uncaught_exception = false;

    new LowSocket(low, 0);
    new LowSocket(low, 1);
    new LowSocket(low, 2);
    if(!low->fds[0] || !low->fds[1] || !low->fds[2])
        return false;

    return true;
}

#endif /* LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV) */


// -----------------------------------------------------------------------------
//  low_lib_init
// -----------------------------------------------------------------------------

static duk_ret_t low_lib_init_safe(duk_context *ctx, void *udata)
{
    low_t *low = duk_get_low_context(ctx);

    duk_push_heap_stash(ctx);
    low->next_tick_ctx = duk_get_context(ctx, duk_push_thread(ctx));
    duk_put_prop_string(ctx, -2, "next_tick_ctx");

    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, "low");
    duk_pop(ctx);

    low_register_promise(low);

    low_module_init(ctx);
    low_load_module(ctx, "lib:init", false);

    low_register_opcua(low);

    return 0;
}

bool low_lib_init(low_t *low)
{
    try
    {
        if(duk_safe_call(low->duk_ctx, low_lib_init_safe, NULL, 0, 1) !=
        DUK_EXEC_SUCCESS)
        {
            if(!low->duk_flag_stop)
                low_duk_print_error(low->duk_ctx);
            duk_pop(low->duk_ctx);
            return false;
        }
        duk_pop(low->duk_ctx);

        return true;
    }
    catch(std::exception &e)
    {
        fprintf(stderr, "Fatal exception: %s\n", e.what());
    }
    catch(...)
    {
        fprintf(stderr, "Fatal exception\n");
    }

    return false;
}


// -----------------------------------------------------------------------------
//  low_call_thread
// -----------------------------------------------------------------------------

class LowCallLoopCallback : public LowLoopCallback
{
public:
    LowCallLoopCallback(low_t *low, void (*func)(duk_context *ctx, void *userdata), void *userdata)
        : LowLoopCallback(low), func(func), ctx(low->duk_ctx), userdata(userdata)
    {
    }

    bool OnLoop()
    {
        func(ctx, userdata);
        return false;
    }

    void (*func)(duk_context *ctx, void *userdata);

    duk_context *ctx;
    void *userdata;
};

class LowCallDataCallback : public LowDataCallback
{
public:
    LowCallDataCallback(low_t *low, void (*func)(duk_context *ctx, void *userdata), void *userdata)
        : LowDataCallback(low), func(func), ctx(low->duk_ctx), userdata(userdata)
    {
    }

    bool OnData()
    {
        func(ctx, userdata);
        return false;
    }

    void (*func)(duk_context *ctx, void *userdata);

    duk_context *ctx;
    void *userdata;
};

void low_call_thread(duk_context *ctx, low_thread thread, int priority,
                     void (*func)(duk_context *ctx, void *userdata), void *userdata)
{
    low_t *low = duk_get_low_context(ctx);
    switch(thread)
    {
    case LOW_THREAD_CODE:
        low_loop_set_callback(low, new LowCallLoopCallback(low, func, userdata));
        break;

    case LOW_THREAD_WORKER:
        low_data_set_callback(low, new LowCallDataCallback(low, func, userdata),
            priority ? LOW_DATA_THREAD_PRIORITY_MODIFY : LOW_DATA_THREAD_PRIORITY_READ);
        break;

    default:
        duk_type_error(ctx, "Native API: Cannot create a thread of type %d", thread);
    }
}


// -----------------------------------------------------------------------------
//  low_get_current_thread
// -----------------------------------------------------------------------------

low_thread low_get_current_thread(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);

#if LOW_ESP32_LWIP_SPECIALITIES
    auto thread = xTaskGetCurrentTaskHandle();
#else
    pthread_t thread = pthread_self();
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    if(thread == low->web_thread)
        return LOW_THREAD_IMMEDIATE;

    for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
        if(thread == low->data_thread[i])
            return LOW_THREAD_WORKER;

    return LOW_THREAD_CODE;
}


// -----------------------------------------------------------------------------
//  low_destroy
// -----------------------------------------------------------------------------

void low_destroy(low_t *low)
{
#if !LOW_ESP32_LWIP_SPECIALITIES
    g_low_system.signal_pipe_fd = low->web_thread_pipe[1];
    if(g_low_system.signal_pipe_fd == low->web_thread_pipe[1])
        g_low_system.signal_pipe_fd = -1;

    low->destroying = true;
    low_web_thread_break(low);

    pthread_join(low->web_thread, NULL);

    pthread_mutex_lock(&low->web_thread_mutex);
    pthread_cond_broadcast(&low->web_thread_done_cond);
    pthread_mutex_unlock(&low->web_thread_mutex);

    // Finish up data threads
    pthread_mutex_lock(&low->data_thread_mutex);
    pthread_cond_broadcast(&low->data_thread_cond);
    pthread_mutex_unlock(&low->data_thread_mutex);

    for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
        pthread_join(low->data_thread[i], NULL);

    try
    {
        // Then we close all FDs and delete all classes behind the callbacks
        while(low->loop_callback_first) // before FDs important for LowDNSResolver!
            delete low->loop_callback_first;
        while(low->data_callback_first[0])
            delete low->data_callback_first[0];
        while(low->data_callback_first[1])
            delete low->data_callback_first[1];
        for(auto iter = low->fds.begin(); iter != low->fds.end();)
        {
            auto iter2 = iter;
            iter++;
            delete iter2->second;
        }
    }
    catch(std::exception &e)
    {
        fprintf(stderr, "Fatal exception: %s\n", e.what());
    }
    catch(...)
    {
        fprintf(stderr, "Fatal exception\n");
    }

    close(low->web_thread_pipe[0]);
    close(low->web_thread_pipe[1]);

    pthread_mutex_destroy(&low->data_thread_mutex);
    pthread_cond_destroy(&low->data_thread_cond);
    pthread_cond_destroy(&low->data_thread_done_cond);

    pthread_mutex_destroy(&low->web_thread_mutex);
    pthread_cond_destroy(&low->web_thread_done_cond);

#if LOW_ESP32_LWIP_SPECIALITIES
    vSemaphoreDelete(low->loop_thread_sema);
#else
    pthread_cond_destroy(&low->loop_thread_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    pthread_mutex_destroy(&low->loop_thread_mutex);

    if(low->duk_ctx)
        duk_destroy_heap(low->duk_ctx);

        // After finalizers.. they must not use DukTape heap!
#if LOW_INCLUDE_CARES_RESOLVER
    for(int i = 0; i < low->resolvers.size(); i++)
        if(low->resolvers[i])
            delete low->resolvers[i];
    ares_library_cleanup();

    pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    for(int i = 0; i < low->tlsContexts.size(); i++)
        if(low->tlsContexts[i])
            delete low->tlsContexts[i];
    for(int i = 0; i < low->cryptoHashes.size(); i++)
        if(low->cryptoHashes[i])
            delete low->cryptoHashes[i]; // TODO: also needed in restart?

    pthread_mutex_destroy(&low->ref_mutex);
    low_free(low);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
}

// -----------------------------------------------------------------------------
//  low_add_stash
// -----------------------------------------------------------------------------

int low_add_stash(duk_context *ctx, int index)
{
    low_t *low = duk_get_low_context(ctx);
    if(low->duk_flag_stop)
        return 0;

    if(index < 0)
        index += duk_get_top(ctx);
    if(duk_is_undefined(ctx, index))
        return 0;

    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");

    int stashIndex;
    while(true)
    {
        stashIndex = ++low->last_stash_index;
        if(stashIndex <= 0)
            stashIndex = low->last_stash_index = 1;

        if(!duk_get_prop_index(ctx, -1, stashIndex))
        {
            duk_pop(ctx);
            break;
        }
        duk_pop(ctx);
    }

    duk_dup(ctx, index);
    duk_put_prop_index(ctx, -2, stashIndex);
    duk_pop_2(ctx);

    return stashIndex;
}

// -----------------------------------------------------------------------------
//  low_remove_stash
// -----------------------------------------------------------------------------

void low_remove_stash(duk_context *ctx, int index)
{
    if(!index)
        return;

    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");
    duk_del_prop_index(ctx, -1, index);
    duk_pop_2(ctx);
}

// -----------------------------------------------------------------------------
//  low_push_stash
// -----------------------------------------------------------------------------

void low_push_stash(duk_context *ctx, int index, bool remove)
{
    duk_push_heap_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");
    duk_get_prop_index(ctx, -1, index);
    if(remove)
        duk_del_prop_index(ctx, -2, index);
    duk_replace(ctx, -3);
    duk_pop(ctx);
}


// -----------------------------------------------------------------------------
//  low_push_buffer
// -----------------------------------------------------------------------------

void *low_push_buffer(duk_context *ctx, int len)
{
    void *data = duk_push_fixed_buffer(ctx, len);
    duk_push_buffer_object(ctx, -1, 0, len, DUK_BUFOBJ_NODEJS_BUFFER);
    duk_remove(ctx, -2);
    return data;
}


// -----------------------------------------------------------------------------
//  low_duk_print_error
// -----------------------------------------------------------------------------

void low_duk_print_error(duk_context *ctx)
{
    if(duk_is_error(ctx, -1))
    {
        if(duk_get_prop_string(ctx, -1, "stack"))
            low_error(duk_safe_to_string(ctx, -1));
        else
            low_error("JavaScript error without stack");
        duk_pop(ctx);
    }
    else
        low_error(duk_safe_to_string(ctx, -1));
}
