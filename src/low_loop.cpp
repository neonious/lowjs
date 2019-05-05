// -----------------------------------------------------------------------------
//  low_loop.cpp
// -----------------------------------------------------------------------------

#include "low_loop.h"
#include "LowLoopCallback.h"

#include "low_config.h"
#include "low_main.h"
#include "low_system.h"

#include <errno.h>

// -----------------------------------------------------------------------------
//  low_loop_run
// -----------------------------------------------------------------------------

#if LOW_ESP32_LWIP_SPECIALITIES
void user_cpu_load(bool active);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

bool low_loop_run(low_main_t *low)
{
    while(!low->duk_flag_stop)
    {
        if(!low->duk_flag_stop && !low->run_ref && !low->loop_callback_first && low->signal_call_id)
        {
            if(duk_safe_call(low->duk_ctx,
                            low_loop_exit_safe,
                            NULL,   // beforeExit
                            0,
                            1) != DUK_EXEC_SUCCESS)
            {
                if(!low->duk_flag_stop) // flag stop also produces error
                    low_duk_print_error(low->duk_ctx);
                duk_pop(low->duk_ctx);

                return low->duk_flag_stop;
            }
        }
        if(low->duk_flag_stop || (!low->run_ref && !low->loop_callback_first))
        {
            if(!low->duk_flag_stop && low->signal_call_id)
            {
                if(duk_safe_call(low->duk_ctx,
                                low_loop_exit_safe,
                                (void *)1,  // exit
                                0,
                                1) != DUK_EXEC_SUCCESS)
                {
                    if(!low->duk_flag_stop) // flag stop also produces error
                        low_duk_print_error(low->duk_ctx);
                    duk_pop(low->duk_ctx);

                    return low->duk_flag_stop;
                }
            }
            break;
        }

        int millisecs = -1;
        if(low->chore_times.size())
        {
            int tick_count = low_tick_count();

            auto iter = low->chore_times.lower_bound(low->last_chore_time);
            if(iter == low->chore_times.end())
                iter = low->chore_times.begin();

            millisecs = iter->first - tick_count;
            if(millisecs <= 0)
            {
                low->last_chore_time = iter->first;

                int index = iter->second;
                low->chore_times.erase(iter);

                auto iterData = low->chores.find(index);
                bool erase = iterData->second.oneshot;
                if(erase)
                {
                    if(iterData->second.ref)
                        low->run_ref--;
                    low->chores.erase(iterData);
                }
                else
                {
                    iterData->second.stamp += iterData->second.interval;
                    if(iterData->second.stamp - tick_count < 0)
                        iterData->second.stamp = tick_count;

                    low->chore_times.insert(
                      pair<int, int>(iterData->second.stamp, index));
                }

                int args[2] = {index, erase};
                if(duk_safe_call(
                     low->duk_ctx, low_loop_call_chore_safe, args, 0, 1) !=
                   DUK_EXEC_SUCCESS)
                {
                    if(!low->duk_flag_stop) // flag stop also produces error
                        low_duk_print_error(low->duk_ctx);
                    duk_pop(low->duk_ctx);

                    return low->duk_flag_stop;
                }
                duk_pop(low->duk_ctx);
                continue;
            }
        }

        pthread_mutex_lock(&low->loop_thread_mutex);
        if(!low->loop_callback_first)
        {
            duk_debugger_cooperate(low->duk_ctx);

#if LOW_ESP32_LWIP_SPECIALITIES
            user_cpu_load(false);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
            if(millisecs >= 0)
            {
                /*
                // TODO under os x
                pthread_condattr_t attr;
                pthread_cond_t cond;
                pthread_condattr_init(&attr);
                pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
                pthread_cond_init(&cond, &attr);
                */
                struct timespec ts;
                int secs = millisecs / 1000;

#if LOW_ESP32_LWIP_SPECIALITIES
                struct timeval tv;
                gettimeofday(&tv, NULL);
                ts.tv_sec = tv.tv_sec;
                ts.tv_nsec = tv.tv_usec * 1000;
#else
                clock_gettime(CLOCK_REALTIME, &ts);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                ts.tv_sec += secs;
                ts.tv_nsec += (millisecs - secs * 1000) * 1000000;
                if(ts.tv_nsec >= 1000000000)
                {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }

                pthread_cond_timedwait(
                  &low->loop_thread_cond, &low->loop_thread_mutex, &ts);
            }
            else
                pthread_cond_wait(&low->loop_thread_cond,
                                  &low->loop_thread_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES
            user_cpu_load(true);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        }
        if(low->loop_callback_first)
        {
            while(true)
            {
                LowLoopCallback *callback = low->loop_callback_first;

                low->loop_callback_first = callback->mNext;
                if(!low->loop_callback_first)
                    low->loop_callback_last = NULL;
                callback->mNext = NULL;

                pthread_mutex_unlock(&low->loop_thread_mutex);
                if(duk_safe_call(low->duk_ctx,
                                 low_loop_call_callback_safe,
                                 callback,
                                 0,
                                 1) != DUK_EXEC_SUCCESS)
                {
                    if(!low->duk_flag_stop) // flag stop also produces error
                        low_duk_print_error(low->duk_ctx);
                    duk_pop(low->duk_ctx);

                    return low->duk_flag_stop;
                }
                duk_pop(low->duk_ctx);
                if(!low->loop_callback_first)
                    break;
                pthread_mutex_lock(&low->loop_thread_mutex);
            }
        }
        else
            pthread_mutex_unlock(&low->loop_thread_mutex);
    }
    return true;
}

// -----------------------------------------------------------------------------
//  low_loop_call_chore_safe
// -----------------------------------------------------------------------------

duk_ret_t low_loop_call_chore_safe(duk_context *ctx, void *udata)
{
    int *args = (int *)udata;

    low_push_stash(duk_get_low_context(ctx), args[0], args[1]);
    duk_call(ctx, 0);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_loop_call_callback_safe
// -----------------------------------------------------------------------------

duk_ret_t low_loop_call_callback_safe(duk_context *ctx, void *udata)
{
    LowLoopCallback *callback = (LowLoopCallback *)udata;
    if(!callback->OnLoop())
        delete callback;
    return 0;
}


// -----------------------------------------------------------------------------
//  low_loop_exit_safe
// -----------------------------------------------------------------------------

duk_ret_t low_loop_exit_safe(duk_context *ctx, void *udata)
{
    low_main_t *low = duk_get_low_context(ctx);
    low_push_stash(low, low->signal_call_id, false);
    duk_push_string(ctx, udata == NULL ? "beforeExit" : "exit");
    duk_call(ctx, 1);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_loop_set_chore
// -----------------------------------------------------------------------------

duk_ret_t low_loop_set_chore(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    int index = low_add_stash(low, 0);

    int delay = duk_require_int(ctx, 1);
    if(delay < 0)
        delay = 0;

    low_chore_t &chore = low->chores[index];
    chore.interval = delay;
    chore.stamp = low_tick_count() + chore.interval;
    chore.oneshot = duk_require_boolean(ctx, 2);
    chore.ref = true;
    low->run_ref++;

    low->chore_times.insert(pair<int, int>(chore.stamp, index));
    duk_push_int(ctx, index);
    return 1;
}

// -----------------------------------------------------------------------------
//  low_loop_clear_chore
// -----------------------------------------------------------------------------

duk_ret_t low_loop_clear_chore(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    int index = duk_require_int(ctx, 0);

    auto iter = low->chores.find(index);
    if(iter == low->chores.end())
        return 0;

    auto iter2 = low->chore_times.find(iter->second.stamp);
    while(iter2->second != index)
        iter2++;
    low->chore_times.erase(iter2);

    if(iter->second.ref)
        low->run_ref--;
    low->chores.erase(iter);
    low_remove_stash(low, index);

    return 0;
}

// -----------------------------------------------------------------------------
//  low_loop_chore_ref
// -----------------------------------------------------------------------------

duk_ret_t low_loop_chore_ref(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    int index = duk_require_int(ctx, 0);
    bool ref = duk_require_boolean(ctx, 1);

    auto iter = low->chores.find(index);
    if(iter == low->chores.end())
        return 0;

    if(iter->second.ref != ref)
    {
        if(ref)
            low->run_ref++;
        else
            low->run_ref--;
        iter->second.ref = ref;
    }

    return 0;
}

// -----------------------------------------------------------------------------
//  low_loop_run_ref
// -----------------------------------------------------------------------------

duk_ret_t low_loop_run_ref(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    low->run_ref += duk_require_int(ctx, 0);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_loop_set_callback
// -----------------------------------------------------------------------------

void low_loop_set_callback(low_main_t *low, LowLoopCallback *callback)
{
    pthread_mutex_lock(&low->loop_thread_mutex);
    if(callback->mNext || low->loop_callback_last == callback)
    {
        pthread_mutex_unlock(&low->loop_thread_mutex);
        return;
    }

    if(low->loop_callback_last)
        low->loop_callback_last->mNext = callback;
    else
        low->loop_callback_first = callback;
    low->loop_callback_last = callback;

    pthread_cond_signal(&low->loop_thread_cond);
    pthread_mutex_unlock(&low->loop_thread_mutex);
}

// -----------------------------------------------------------------------------
//  low_loop_clear_callback
// -----------------------------------------------------------------------------

void low_loop_clear_callback(low_main_t *low, LowLoopCallback *callback)
{
    pthread_mutex_lock(&low->loop_thread_mutex);
    if(callback->mNext || low->loop_callback_last == callback)
    {
        LowLoopCallback *elem = low->loop_callback_first;
        if(elem == callback)
        {
            low->loop_callback_first = elem->mNext;
            if(!low->loop_callback_first)
                low->loop_callback_last = NULL;
        }
        else
        {
            while(elem->mNext != callback)
                elem = elem->mNext;

            if(low->loop_callback_last == callback)
            {
                low->loop_callback_last = elem;
                elem->mNext = NULL;
            }
            else
                elem->mNext = callback->mNext;
        }
        callback->mNext = NULL;
    }
    pthread_mutex_unlock(&low->loop_thread_mutex);
}
