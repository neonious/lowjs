// -----------------------------------------------------------------------------
//  low_data_thread.cpp
// -----------------------------------------------------------------------------

#include "low_data_thread.h"
#include "LowDataCallback.h"

#include "low_main.h"


// -----------------------------------------------------------------------------
//  low_data_thread_main
// -----------------------------------------------------------------------------

void *low_data_thread_main(void *arg)
{
    low_t *low = (low_t *)arg;

    pthread_mutex_lock(&low->data_thread_mutex);
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    while(true)
    {
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
        while(true)
        {
        start:
            if(low->destroying)
                break;

            for(int priority = 0; priority < 2; priority++)
                if(low->data_callback_first[priority] && !low->destroying)
                {
                    LowDataCallback *callback =
                        low->data_callback_first[priority];

                    if(callback->mInDataThread)
                    {
                        LowDataCallback *last = callback;
                        callback = callback->mNext;

                        while(callback && callback->mInDataThread)
                        {
                            last = callback;
                            callback = callback->mNext;
                        }

                        if(!callback)
                            continue;

                        last->mNext = callback->mNext;
                        if(low->data_callback_last[priority] == callback)
                            low->data_callback_last[priority] = last;
                    }
                    else
                    {
                        low->data_callback_first[priority] = callback->mNext;
                        if(!low->data_callback_first[priority])
                            low->data_callback_last[priority] = NULL;
                    }

                    callback->mNext = NULL;
                    callback->mInDataThread = true;
                    pthread_mutex_unlock(&low->data_thread_mutex);

                    if(!callback->OnData())
                    {
                        pthread_mutex_lock(&low->data_thread_mutex);
                        callback->mInDataThread = false;
                        pthread_cond_broadcast(&low->data_thread_done_cond);
                        pthread_mutex_unlock(&low->data_thread_mutex);

                        delete callback;
                        callback = NULL;
                    }

                    pthread_mutex_lock(&low->data_thread_mutex);
                    if(callback)
                        callback->mInDataThread = false;
                    goto start;
                }
            if(low->destroying)
                break;

            pthread_cond_broadcast(&low->data_thread_done_cond);
            pthread_cond_wait(&low->data_thread_cond, &low->data_thread_mutex);
        }
        pthread_cond_broadcast(&low->data_thread_done_cond);

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        low->data_thread_done = true;
        while(low->destroying)
            pthread_cond_wait(&low->data_thread_cond, &low->data_thread_mutex);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    pthread_mutex_unlock(&low->data_thread_mutex);
    return NULL;
}

// -----------------------------------------------------------------------------
//  low_data_set_callback
// -----------------------------------------------------------------------------

void low_data_set_callback(low_t *low, LowDataCallback *callback,
                           int priority)
{
    pthread_mutex_lock(&low->data_thread_mutex);

    if(callback->mNext || low->data_callback_last[0] == callback ||
       low->data_callback_last[1] == callback)
    {
        pthread_mutex_unlock(&low->data_thread_mutex);
        return;
    }

    if(low->data_callback_last[priority])
        low->data_callback_last[priority]->mNext = callback;
    else
        low->data_callback_first[priority] = callback;
    low->data_callback_last[priority] = callback;

    if(!callback->mInDataThread)
        pthread_cond_broadcast(&low->data_thread_cond);
    pthread_mutex_unlock(&low->data_thread_mutex);
}

// -----------------------------------------------------------------------------
//  low_data_clear_callback
// -----------------------------------------------------------------------------

void low_data_clear_callback(low_t *low, LowDataCallback *callback)
{
    pthread_mutex_lock(&low->data_thread_mutex);

    if(low->data_callback_first[0] == callback)
    {
        LowDataCallback *elem = low->data_callback_first[0];
        low->data_callback_first[0] = elem->mNext;
        if(!low->data_callback_first[0])
            low->data_callback_last[0] = NULL;
    }
    else if(low->data_callback_first[1] == callback)
    {
        LowDataCallback *elem = low->data_callback_first[1];
        low->data_callback_first[1] = elem->mNext;
        if(!low->data_callback_first[1])
            low->data_callback_last[1] = NULL;
    }
    else if(callback->mNext || low->data_callback_last[0] == callback ||
            low->data_callback_last[1] == callback)
    {
        for(int priority = 0; priority < 2; priority++)
        {
            LowDataCallback *elem = low->data_callback_first[priority];
            while(elem && elem->mNext != callback)
                elem = elem->mNext;
            if(!elem)
                continue;

            if(low->data_callback_last[priority] == callback)
            {
                low->data_callback_last[priority] = elem;
                elem->mNext = NULL;
            }
            else
                elem->mNext = callback->mNext;
        }
    }
    callback->mNext = NULL;

    while(callback->mInDataThread)
        pthread_cond_wait(&low->data_thread_done_cond, &low->data_thread_mutex);
    pthread_mutex_unlock(&low->data_thread_mutex);
}
