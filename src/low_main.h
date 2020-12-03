// -----------------------------------------------------------------------------
//  low_main.h
// -----------------------------------------------------------------------------

#ifndef __LOW_MAIN_H__
#define __LOW_MAIN_H__

#include "low_alloc.h"
#include "low_config.h"
#include "low_loop.h"

#if LOW_ESP32_LWIP_SPECIALITIES
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#include <map>
#include <pthread.h>
#include <vector>

using namespace std;

class LowLoopCallback;
class LowDataCallback;
class LowFD;
class LowDNSResolver;
class LowTLSContext;
class LowCryptoHash;

struct low_t
{
    uint8_t duk_flag_stop;
    bool destroying;
    duk_context *duk_ctx, *next_tick_ctx;

#if !LOW_ESP32_LWIP_SPECIALITIES
    unsigned int heap_size, max_heap_size;
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
    bool in_gc, disallow_native;

    int run_ref, last_stash_index;

    int signal_call_id;
    bool in_uncaught_exception;

    map<int, low_chore_t> chores;
    multimap<int, int> chore_times;
    int last_chore_time;

#if LOW_ESP32_LWIP_SPECIALITIES
    TaskHandle_t data_thread[LOW_NUM_DATA_THREADS];
    TaskHandle_t web_thread;
    SemaphoreHandle_t loop_thread_sema;
#else
    pthread_t data_thread[LOW_NUM_DATA_THREADS];
    pthread_t web_thread;
    pthread_cond_t loop_thread_cond;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    pthread_mutex_t loop_thread_mutex;

    LowLoopCallback *loop_callback_first, *loop_callback_last;

    pthread_mutex_t data_thread_mutex;
    pthread_cond_t data_thread_cond, data_thread_done_cond;
    LowDataCallback *data_callback_first[2], *data_callback_last[2];
    bool data_thread_done;

    pthread_mutex_t web_thread_mutex;
    pthread_cond_t web_thread_done_cond;
#if !LOW_ESP32_LWIP_SPECIALITIES
    int web_thread_pipe[2];
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    LowFD *web_changed_first, *web_changed_last;
    bool web_thread_done;
    bool reset_accepts;

    map<int, LowFD *, less<int>> fds;

#if LOW_INCLUDE_CARES_RESOLVER
    vector<LowDNSResolver *> resolvers;
    int resolvers_active;
    pthread_mutex_t resolvers_mutex;
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    vector<LowTLSContext *> tlsContexts;
    vector<LowCryptoHash *> cryptoHashes;

    pthread_mutex_t ref_mutex;

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    char *cwd;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    int (*module_transpile_hook)(duk_context *ctx);
};

typedef enum
{
    LOW_THREAD_CODE,
    LOW_THREAD_WORKER,
    LOW_THREAD_IMMEDIATE
} low_thread;

low_t *low_init();
bool low_lib_init(low_t *low);
void low_destroy(low_t *low);

duk_context *low_get_duk_context(low_t *low);
low_t *duk_get_low_context(duk_context *ctx);

bool low_reset(low_t *low);

extern "C" void low_call_thread(duk_context *ctx, low_thread thread, int priority, void (*func)(duk_context *ctx, void *userdata), void *userdata);
extern "C" low_thread low_get_current_thread(duk_context *ctx);

extern "C" int low_add_stash(duk_context *ctx, int index);
extern "C" void low_remove_stash(duk_context *ctx, int index);
extern "C" void low_push_stash(duk_context *ctx, int index, bool remove);

extern "C" void *low_push_buffer(duk_context *ctx, int len);

void low_duk_print_error(duk_context *ctx);

#endif /* __LOW_MAIN_H__ */
