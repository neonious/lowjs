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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "low_main"
#endif /* LOW_ESP32_LWIP_SPECIALITIES */


// Global variables
extern low_system_t g_low_system;


// -----------------------------------------------------------------------------
//  low_init
// -----------------------------------------------------------------------------

static void *low_duk_alloc(void *udata, duk_size_t size)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    auto data = low_alloc2(size);
    if(!data)
    {
        ESP_LOGE(TAG, "returning NULL ptr");
        low_main_t *low = (low_main_t *)udata;
        duk_generic_error(low->stash_ctx, "memory full");
    }
    return data;
#else
    return low_alloc(size);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}

static void *low_duk_realloc(void *udata, void *ptr, duk_size_t size)
{
    if(size == 0)
    {
        low_free(ptr);
        return NULL;
    }
#if LOW_ESP32_LWIP_SPECIALITIES
    auto data = low_realloc2(ptr, size);
    if(!data)
    {
        ESP_LOGE(TAG, "returning NULL ptr");
        low_main_t *low = (low_main_t *)udata;
        duk_generic_error(low->stash_ctx, "memory full");
    }
    return data;
#else
    return low_realloc(ptr, size);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}

static void low_duk_free(void *udata, void *ptr)
{
    low_free(ptr);
}

static void low_duk_fatal(void *udata, const char *msg)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    ESP_LOGE(TAG, "duk_fatal: %s", msg);
    vTaskDelay(5000);
    esp_restart();
#else
    low_error(msg);

    fprintf(stderr,
            "--- exiting as we reached fatal error handler, should not "
            "happen ---\n");
    exit(EXIT_FAILURE);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}

low_main_t *low_init()
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

    low_main_t *low = new(low_new) low_main_t();
    if(!low)
    {
        fprintf(stderr, "Memory full\n");

#if LOW_INCLUDE_CARES_RESOLVER
        ares_library_cleanup();
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        return NULL;
    }

    low->web_thread = NULL;
    for(int i = 0; i < LOW_NUM_DATA_THREADS; i++)
        low->data_thread[i] = NULL;

    low->destroying = false;
    low->duk_flag_stop = 0;
#if LOW_ESP32_LWIP_SPECIALITIES
    // we are doing this on reset
    low->stash_ctx = low->duk_ctx = NULL;
#else
    low->stash_ctx = low->duk_ctx = duk_create_heap(
      low_duk_alloc, low_duk_realloc, low_duk_free, low, low_duk_fatal);
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

    if(pthread_mutex_init(&low->loop_thread_mutex, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        goto err;
    }
    if(pthread_cond_init(&low->loop_thread_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        pthread_mutex_destroy(&low->loop_thread_mutex);
        goto err;
    }
    low->loop_callback_first = low->loop_callback_last = NULL;

    if(pthread_mutex_init(&low->data_thread_mutex, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);

        goto err;
    }
    if(pthread_cond_init(&low->data_thread_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
        pthread_mutex_destroy(&low->data_thread_mutex);

        goto err;
    }
    if(pthread_cond_init(&low->data_thread_done_cond, NULL) != 0)
    {
#if LOW_INCLUDE_CARES_RESOLVER
        pthread_mutex_destroy(&low->resolvers_mutex);
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        pthread_mutex_destroy(&low->ref_mutex);
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
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
                                      &low->data_thread[i],
                                      0);
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
            pthread_mutex_destroy(&low->loop_thread_mutex);
            pthread_cond_destroy(&low->loop_thread_cond);
            pthread_mutex_destroy(&low->data_thread_mutex);
            pthread_cond_destroy(&low->data_thread_cond);
            pthread_cond_destroy(&low->data_thread_done_cond);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            goto err;
        }
    }

    low->web_thread_done = low->web_thread_notinevents = false;
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
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
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
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
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
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
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
                                  &low->web_thread,
                                  0);
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
        pthread_mutex_destroy(&low->loop_thread_mutex);
        pthread_cond_destroy(&low->loop_thread_cond);
        pthread_mutex_destroy(&low->data_thread_mutex);
        pthread_cond_destroy(&low->data_thread_cond);
        pthread_cond_destroy(&low->data_thread_done_cond);
        pthread_mutex_destroy(&low->web_thread_mutex);
        pthread_cond_destroy(&low->web_thread_done_cond);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

        goto err;
    }

    low->fds[0] = new(low_new) LowSocket(low, 0);
    low->fds[1] = new(low_new) LowSocket(low, 1);
    low->fds[2] = new(low_new) LowSocket(low, 2);
    if(!low->fds[0] || !low->fds[1] || !low->fds[2])
    {
        low_destroy(low);
        return NULL;
    }

#if LOW_ESP32_LWIP_SPECIALITIES
    low->cwd = low_strdup("/");
    if(!low->cwd)
        return NULL;
#else
    g_low_system.signal_pipe_fd = low->web_thread_pipe[1];
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */

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

#if LOW_ESP32_LWIP_SPECIALITIES

// -----------------------------------------------------------------------------
//  low_reset
// -----------------------------------------------------------------------------

extern "C" void duk_copy_breakpoints(duk_context *from, duk_context *to);
void alloc_reset_heap();
bool isnntry2;
bool low_reset(low_main_t *low)
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
      low_duk_alloc, low_duk_realloc, low_duk_free, low, low_duk_fatal);
    if(!new_ctx && low->stash_ctx)
    {
        fprintf(stderr, "Cannot create Duktape heap, trying after free\n");
        alloc_reset_heap();

        duk_destroy_heap(low->stash_ctx);
        low->stash_ctx = NULL;

        alloc_reset_heap();

        duk_context *new_ctx = duk_create_heap(
          low_duk_alloc, low_duk_realloc, low_duk_free, low, low_duk_fatal);
    }
    if(!new_ctx)
    {
        fprintf(stderr, "Cannot create Duktape heap\n");
        return false;
    }

    if(low->stash_ctx)
    {
        duk_copy_breakpoints(low->stash_ctx, new_ctx);
        duk_destroy_heap(low->stash_ctx);
    }

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

    low->stash_ctx = low->duk_ctx = new_ctx;

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

    low_free(low->cwd);
    low->cwd = low_strdup("/");
    if(!low->cwd)
        return false;

    low->fds[0] = new(low_new) LowSocket(low, 0);
    low->fds[1] = new(low_new) LowSocket(low, 1);
    low->fds[2] = new(low_new) LowSocket(low, 2);
    if(!low->fds[0] || !low->fds[1] || !low->fds[2])
        return false;

    return true;
}

#endif /* LOW_ESP32_LWIP_SPECIALITIES */

// -----------------------------------------------------------------------------
//  low_lib_init
// -----------------------------------------------------------------------------

static duk_ret_t low_lib_init_safe(duk_context *ctx, void *udata)
{
    duk_push_global_stash(ctx);
    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, "low");
    duk_pop(ctx);

    low_module_init(ctx);
    low_module_run(ctx, "lib:init", LOW_MODULE_FLAG_GLOBAL);

    return 0;
}

bool low_lib_init(low_main_t *low)
{
    if(duk_safe_call(low->duk_ctx, low_lib_init_safe, NULL, 0, 1) !=
       DUK_EXEC_SUCCESS)
    {
        if(!low->duk_flag_stop)
            low_duk_print_error(low->duk_ctx);
        return false;
    }
    duk_pop(low->duk_ctx);
    return true;
}

// -----------------------------------------------------------------------------
//  low_destroy
// -----------------------------------------------------------------------------

void low_destroy(low_main_t *low)
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

    close(low->web_thread_pipe[0]);
    close(low->web_thread_pipe[1]);

    pthread_mutex_destroy(&low->data_thread_mutex);
    pthread_cond_destroy(&low->data_thread_cond);
    pthread_cond_destroy(&low->data_thread_done_cond);

    pthread_mutex_destroy(&low->web_thread_mutex);
    pthread_cond_destroy(&low->web_thread_done_cond);

    pthread_mutex_destroy(&low->loop_thread_mutex);
    pthread_cond_destroy(&low->loop_thread_cond);

    if(low->stash_ctx)
        duk_destroy_heap(low->stash_ctx);

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

int low_add_stash(low_main_t *low, int index)
{
    if(low->duk_flag_stop)
        return 0;

    duk_context *ctx = low->stash_ctx;
    if(duk_is_undefined(ctx, index))
        return 0;

    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");

    int stashIndex;
    while(true)
    {
        stashIndex = ++low->last_stash_index;
        if(!stashIndex)
            stashIndex = ++low->last_stash_index;

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

void low_remove_stash(low_main_t *low, int index)
{
    if(low->duk_flag_stop)
        return;

    duk_context *ctx = low->stash_ctx;
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");
    duk_del_prop_index(ctx, -1, index);
    duk_pop_2(ctx);
}

// -----------------------------------------------------------------------------
//  low_push_stash
// -----------------------------------------------------------------------------

void low_push_stash(low_main_t *low, int index, bool remove)
{
    if(low->duk_flag_stop)
        return;

    duk_context *ctx = low->stash_ctx;
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, "low");
    duk_get_prop_index(ctx, -1, index);
    if(remove)
        duk_del_prop_index(ctx, -2, index);
    duk_replace(ctx, -3);
    duk_pop(ctx);
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
            low_error("JavaScript error with stack");
    }
    else
        low_error("JavaScript error with without error object");
}

// -----------------------------------------------------------------------------
//  low_duk_get_low
// -----------------------------------------------------------------------------

low_main_t *low_duk_get_low(duk_context *ctx)
{
    duk_memory_functions funcs;
    duk_get_memory_functions(ctx, &funcs);
    return (low_main_t *)funcs.udata;
}
