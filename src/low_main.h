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

struct low_main_t
{
    uint8_t duk_flag_stop;
    bool destroying;
    duk_context *duk_ctx;

    int run_ref, last_stash_index;
    int signal_call_id;

    map<int,
        low_chore_t,
        less<int>,
        low_allocator<pair<const int, low_chore_t>>>
      chores;
    multimap<int, int, less<int>, low_allocator<pair<const int, int>>>
      chore_times;
    int last_chore_time;

    pthread_mutex_t loop_thread_mutex;
    pthread_cond_t loop_thread_cond;
    LowLoopCallback *loop_callback_first, *loop_callback_last;

#if LOW_ESP32_LWIP_SPECIALITIES
    TaskHandle_t data_thread[LOW_NUM_DATA_THREADS];
    TaskHandle_t web_thread;
#else
    pthread_t data_thread[LOW_NUM_DATA_THREADS];
    pthread_t web_thread;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    bool data_thread_done;
    pthread_mutex_t data_thread_mutex;
    pthread_cond_t data_thread_cond, data_thread_done_cond;
    LowDataCallback *data_callback_first[2], *data_callback_last[2];

    pthread_mutex_t web_thread_mutex;
    pthread_cond_t web_thread_done_cond;
#if !LOW_ESP32_LWIP_SPECIALITIES
    int web_thread_pipe[2];
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    LowFD *web_changed_first, *web_changed_last;
    bool web_thread_done, web_thread_notinevents;
    bool reset_accepts;

    map<int, LowFD *, less<int>, low_allocator<pair<const int, LowFD *>>> fds;

#if LOW_INCLUDE_CARES_RESOLVER
    vector<LowDNSResolver *, low_allocator<LowDNSResolver *>> resolvers;
    int resolvers_active;
    pthread_mutex_t resolvers_mutex;
#endif /* LOW_INCLUDE_CARES_RESOLVER */
    vector<LowTLSContext *, low_allocator<LowTLSContext *>> tlsContexts;
    vector<LowCryptoHash *, low_allocator<LowCryptoHash *>> cryptoHashes;

    pthread_mutex_t ref_mutex;

#if LOW_ESP32_LWIP_SPECIALITIES
    char *cwd;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
};

extern "C"
{
    low_main_t *low_init();
    bool low_lib_init(low_main_t *low);
    void low_destroy(low_main_t *low);

    duk_context *low_get_duk_context(low_main_t *low);
    low_main_t *duk_get_low_context(duk_context *ctx);
}

#if LOW_ESP32_LWIP_SPECIALITIES
bool low_reset(low_main_t *low);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

int low_add_stash(duk_context *ctx, int index);
void low_remove_stash(duk_context *ctx, int index);
void low_push_stash(duk_context *ctx, int index, bool remove);

void *low_push_buffer(duk_context *ctx, int len);

void low_duk_print_error(duk_context *ctx);

#endif /* __LOW_MAIN_H__ */
