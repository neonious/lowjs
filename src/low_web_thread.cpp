// -----------------------------------------------------------------------------
//  low_web_thread.cpp
// -----------------------------------------------------------------------------

#include "low_web_thread.h"
#include "LowFD.h"

#include "low_main.h"
#include "low_system.h"
#include "LowSignalHandler.h"

#if LOW_INCLUDE_CARES_RESOLVER
#include "LowDNSResolver.h"
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#include "low_alloc.h"

#include <unistd.h>
#include <signal.h>
#if LOW_HAS_POLL
#include <poll.h>
#endif /* LOW_HAS_POLL */

#if LOW_ESP32_LWIP_SPECIALITIES
extern "C" void lowjs_esp32_break_web(bool fromInterrupt);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
void lowjs_esp32_web_tick();
extern int gWebThreadNextTick;
extern bool gCodeRunning;
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#include <vector>

using namespace std;

// -----------------------------------------------------------------------------
//  low_web_thread_main
// -----------------------------------------------------------------------------

void *low_web_thread_main(void *arg)
{
    low_t *low = (low_t *)arg;

#if LOW_HAS_POLL && !defined(LOWJS_SERV)
    vector<pollfd> fds;
    vector<LowFD *> lowFDs;

    pollfd poll_entry;
    poll_entry.fd = low->web_thread_pipe[0];
    poll_entry.events = POLLIN;
    fds.push_back(poll_entry);
    lowFDs.push_back(NULL);

    while(true)
    {
        int timeout = -1, i;

#if LOW_INCLUDE_CARES_RESOLVER
        int first_cares_fd = fds.size();
        if(low->resolvers_active)
        {
            pthread_mutex_lock(&low->resolvers_mutex);
            for(i = 0; i < low->resolvers.size(); i++)
            {
                if(!low->resolvers[i]->IsActive())
                    continue;
                ares_channel &channel = low->resolvers[i]->Channel();

                int sockets[16];
                int mask = ares_getsock(channel, sockets, 16);
                for(int j = 0; j < 16; j++)
                {
                    short events =
                        (ARES_GETSOCK_READABLE(mask, j) ? POLLIN : 0) |
                        (ARES_GETSOCK_WRITABLE(mask, j) ? POLLOUT : 0);
                    if(!events)
                        break;

                    pollfd fd;
                    fd.fd = sockets[j];
                    fd.events = events;
                    fds.push_back(fd);

                    lowFDs.push_back((LowFD *)channel);

                    struct timeval tv;
                    struct timeval *val = ares_timeout(channel, NULL, &tv);
                    if(val)
                    {
                        int millisecs =
                            val->tv_sec * 1000 + val->tv_usec / 1000;
                        if(timeout > millisecs || timeout == -1)
                            timeout = millisecs;
                    }
                }
            }
            pthread_mutex_unlock(&low->resolvers_mutex);
        }
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        int count = poll(&fds[0], fds.size(), timeout);
        if(low->destroying)
            break;

#if LOW_INCLUDE_CARES_RESOLVER
        if(count == 0)
        {
            // Timeout
            for(i = first_cares_fd; i < fds.size(); i++)
                ares_process_fd((ares_channel)lowFDs[i], ARES_SOCKET_BAD,
                                ARES_SOCKET_BAD);
        }
#endif /* LOW_INCLUDE_CARES_RESOLVER */
        if(count > 0 && fds[0].revents)
        {
            unsigned char s;
            read(fds[0].fd, &s, 1);
            if(s != 0xFF)
            {
                LowSignalHandler *signal = new LowSignalHandler(low, s);
                if(!signal)
                {
                } // not much we can do here !
            }
            count--;
        }

        if(count > 0)
        {
            int firstNone = 1;
#if LOW_INCLUDE_CARES_RESOLVER
            for(i = 1; i < first_cares_fd; i++)
#else
            for(i = 1; i < fds.size(); i++)
#endif /* LOW_INCLUDE_CARES_RESOLVER */
            {
                if(fds[i].fd >= 0 && fds[i].revents)
                {
                    if(!lowFDs[i]->OnEvents(fds[i].revents))
                    {
                        fds[i].fd = -1;

                        // Remove from web thread list
                        auto fd = lowFDs[i];
                        pthread_mutex_lock(&low->web_thread_mutex);
                        if(fd->mNextChanged || low->web_changed_last == fd)
                        {
                            if(low->web_changed_first == fd)
                                low->web_changed_first = fd->mNextChanged;
                            else
                            {
                                auto elem = low->web_changed_first;
                                while(elem)
                                {
                                    if(elem->mNextChanged == fd)
                                    {
                                        elem->mNextChanged = fd->mNextChanged;
                                        if(low->web_changed_last == fd)
                                            low->web_changed_last = elem;
                                        break;
                                    }
                                    elem = elem->mNextChanged;
                                }
                            }
                            if(!low->web_changed_first)
                                low->web_changed_last = NULL;
                        }
                        fd->mPollIndex = -1;
                        fd->mNextChanged = NULL;
                        pthread_mutex_unlock(&low->web_thread_mutex);

                        delete fd;
                    }
                    count--;
                }
                if(fds[i].fd >= 0)
                    firstNone = i + 1;
            }
#if LOW_INCLUDE_CARES_RESOLVER
            for(; i < fds.size(); i++)
                if(fds[i].revents)
                {
                    ares_process_fd(
                        (ares_channel)lowFDs[i],
                        (fds[i].revents & POLLIN) ? fds[i].fd : ARES_SOCKET_BAD,
                        (fds[i].revents & POLLOUT) ? fds[i].fd
                                                   : ARES_SOCKET_BAD);
                    count--;
                }
#endif /* LOW_INCLUDE_CARES_RESOLVER */
            if(firstNone != fds.size())
            {
                // Effectivly also removes resolvers
                fds.resize(firstNone);
                lowFDs.resize(firstNone);
            }
        }
#if LOW_INCLUDE_CARES_RESOLVER
        else if(first_cares_fd != fds.size())
        {
            fds.resize(first_cares_fd);
            lowFDs.resize(first_cares_fd);
        }
#endif /* LOW_INCLUDE_CARES_RESOLVER */

        pthread_mutex_lock(&low->web_thread_mutex);
        while(low->web_changed_first)
        {
            LowFD *fd = low->web_changed_first;

            low->web_changed_first = fd->mNextChanged;
            if(!low->web_changed_first)
                low->web_changed_last = NULL;
            fd->mNextChanged = NULL;
            int mFD = fd->mFD;

            if(fd->mMarkDelete)
            {
                if(fd->mPollIndex != -1)
                {
                    fds[fd->mPollIndex].fd = -1;
                    fd->mPollIndex = -1;
                }

                pthread_mutex_unlock(&low->web_thread_mutex);
                delete fd;
                pthread_mutex_lock(&low->web_thread_mutex);
            }
            else if((mFD < 0 || !fd->mPollEvents) && fd->mPollIndex != -1)
            {
                fds[fd->mPollIndex].fd = -1;
                fd->mPollIndex = -1;
            }
            else if(mFD >= 0 && fd->mPollEvents)
            {
                if(fd->mPollIndex == -1)
                {
                    for(int i = 0; i < fds.size(); i++)
                    {
                        if(fds[i].fd == -1)
                        {
                            fd->mPollIndex = i;
                            lowFDs[i] = fd;
                            break;
                        }
                    }
                    if(fd->mPollIndex == -1)
                    {
                        fd->mPollIndex = fds.size();

                        fds.emplace_back(pollfd());
                        lowFDs.push_back(fd);
                    }
                }
                fds[fd->mPollIndex].fd = mFD;
                fds[fd->mPollIndex].events = fd->mPollEvents;
            }
        }
        pthread_cond_broadcast(&low->web_thread_done_cond);
        pthread_mutex_unlock(&low->web_thread_mutex);
    }
#else
    fd_set read_set, write_set;
    vector<pair<LowFD *, int>> fds;

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    while(true)
    {
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
#if !LOW_ESP32_LWIP_SPECIALITIES
        FD_SET(low->web_thread_pipe[0], &read_set);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

        while(true)
        {
            int timeout = -1, i;

            fd_set read_set1 = read_set;
            fd_set write_set1 = write_set;

#if LOW_INCLUDE_CARES_RESOLVER
            bool has_cares = false;
            if(low->resolvers_active)
            {
                pthread_mutex_lock(&low->resolvers_mutex);
                for(i = 0; i < low->resolvers.size(); i++)
                {
                    if(!low->resolvers[i]->IsActive())
                        continue;
                    ares_channel &channel = low->resolvers[i]->Channel();

                    int count = ares_fds(channel, &read_set1, &write_set1);
                    if(count)
                    {
                        has_cares = true;

                        struct timeval tv;
                        struct timeval *val = ares_timeout(channel, NULL, &tv);
                        if(val)
                        {
                            int millisecs =
                                val->tv_sec * 1000 + val->tv_usec / 1000;
                            if(timeout > millisecs || timeout == -1)
                                timeout = millisecs;
                        }
                    }
                }
                pthread_mutex_unlock(&low->resolvers_mutex);
            }
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
            int timeout2 = gWebThreadNextTick - low_tick_count();
            while(timeout2 <= 0)
            {
                lowjs_esp32_web_tick();
                timeout2 = gWebThreadNextTick - low_tick_count();
            }
            if(timeout == -1 || timeout > timeout2)
                timeout = timeout2;
#endif
#if LOW_ESP32_LWIP_SPECIALITIES
            int count;
            if(timeout == -1)
                count = lwip_select(FD_SETSIZE, &read_set1, &write_set1, NULL,
                                    NULL);
            else
            {
                struct timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout - tv.tv_sec * 1000) * 1000;

                count =
                    lwip_select(FD_SETSIZE, &read_set1, &write_set1, NULL, &tv);
            }
            if(count < 0)
                vTaskDelay(1000); // do not block!
#else
            int count;
            if(timeout == -1)
                count = select(FD_SETSIZE, &read_set1, &write_set1, NULL, NULL);
            else
            {
                struct timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout - tv.tv_sec * 1000) * 1000;

                count = select(FD_SETSIZE, &read_set1, &write_set1, NULL, &tv);
            }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            if(low->destroying)
                break;
#if LOW_ESP32_LWIP_SPECIALITIES
            if(!count)
                lowjs_esp32_web_tick();
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

#if LOW_INCLUDE_CARES_RESOLVER
            if(has_cares)
            {
                pthread_mutex_lock(&low->resolvers_mutex);
                for(i = 0; i < low->resolvers.size(); i++)
                {
                    if(!low->resolvers[i]->IsActive())
                        continue;
                    ares_channel &channel = low->resolvers[i]->Channel();

                    ares_process(channel, &read_set1, &write_set1);
                }
                pthread_mutex_unlock(&low->resolvers_mutex);
            }
#endif /* LOW_INCLUDE_CARES_RESOLVER */

#if !LOW_ESP32_LWIP_SPECIALITIES
            if(count > 0 && FD_ISSET(low->web_thread_pipe[0], &read_set1))
            {
                unsigned char s;
                read(low->web_thread_pipe[0], &s, 1);
#if defined(LOWJS_SERV)
                if(s == 0xFF)
                    lowjs_esp32_web_tick();
#endif /* LOWJS_SERV */
                if(s != 0xFF
#if defined(LOWJS_SERV)
                && s != SIGHUP
#endif /* LOWJS_SERV */
                )
                {
#if defined(LOWJS_SERV)
                    if(!gCodeRunning && (s == SIGTERM || s == SIGINT || s == SIGHUP))
                    {
                        // Go back to default handler
                        struct sigaction action;
                        memset(&action, 0, sizeof(action));
                        sigemptyset(&action.sa_mask);
                        action.sa_flags = SA_RESTART;
                        action.sa_handler = SIG_DFL;
                        sigaction(s, &action, NULL);

                        // Not needed, terminal not changed with lowserv anyways
                        //low_system_destroy();
                        // Exit
                        raise(s);
                    }
                    else
                    {
#endif /* LOWJS_SERV */
                        LowSignalHandler *signal =
                            new LowSignalHandler(low, s);
                        if(!signal)
                        {
                        } // not much we can do here !
#if defined(LOWJS_SERV)
                    }
#endif /* LOWJS_SERV */
                }
                count--;
            }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

            if(count > 0)
            {
                int firstNone = 0;
                for(i = 0; i < fds.size(); i++)
                {
                    if(fds[i].second >= 0)
                    {
                        int events =
                            (FD_ISSET(fds[i].second, &read_set1) ? POLLIN : 0) |
                            (FD_ISSET(fds[i].second, &write_set1) ? POLLOUT
                                                                  : 0);
                        if(events)
                        {
                            if(!fds[i].first->OnEvents(events))
                            {
                                auto fd = fds[i].first;

                                FD_CLR(fds[i].second, &read_set);
                                FD_CLR(fds[i].second, &write_set);
                                fds[i].second = -1;

                                // Remove from web thread list
                                pthread_mutex_lock(&low->web_thread_mutex);
                                if(fd->mNextChanged || low->web_changed_last == fd)
                                {
                                    if(low->web_changed_first == fd)
                                        low->web_changed_first =
                                            fd->mNextChanged;
                                    else
                                    {
                                        auto elem = low->web_changed_first;
                                        while(elem)
                                        {
                                            if(elem->mNextChanged == fd)
                                            {
                                                elem->mNextChanged =
                                                    fd->mNextChanged;
                                                if(low->web_changed_last == fd)
                                                    low->web_changed_last = elem;
                                                break;
                                            }
                                            elem = elem->mNextChanged;
                                        }
                                    }
                                    if(!low->web_changed_first)
                                        low->web_changed_last = NULL;
                                }
                                fd->mPollIndex = -1;
                                fd->mNextChanged = NULL;
                                pthread_mutex_unlock(&low->web_thread_mutex);

                                delete fd;
                            }
                            count--;
                        }
                    }
                    if(fds[i].second >= 0)
                        firstNone = i + 1;
                }
                if(firstNone != fds.size())
                    fds.resize(firstNone);
            }

            pthread_mutex_lock(&low->web_thread_mutex);
            while(low->web_changed_first)
            {
                LowFD *fd = low->web_changed_first;

                low->web_changed_first = fd->mNextChanged;
                if(!low->web_changed_first)
                    low->web_changed_last = NULL;
                fd->mNextChanged = NULL;
                int mFD = fd->mFD;

                if(fd->mMarkDelete)
                {
                    if(fd->mPollIndex != -1)
                    {
                        FD_CLR(fds[fd->mPollIndex].second, &read_set);
                        FD_CLR(fds[fd->mPollIndex].second, &write_set);

                        fds[fd->mPollIndex].second = -1;
                        fd->mPollIndex = -1;
                    }

                    pthread_mutex_unlock(&low->web_thread_mutex);
                    delete fd;
                    pthread_mutex_lock(&low->web_thread_mutex);
                }
                else if((mFD < 0 || !fd->mPollEvents) &&
                        fd->mPollIndex != -1)
                {
                    FD_CLR(fds[fd->mPollIndex].second, &read_set);
                    FD_CLR(fds[fd->mPollIndex].second, &write_set);

                    fds[fd->mPollIndex].second = -1;
                    fd->mPollIndex = -1;
                }
                else if(mFD >= 0 && fd->mPollEvents)
                {
                    if(fd->mPollIndex == -1)
                    {
                        for(int i = 0; i < fds.size(); i++)
                        {
                            if(fds[i].second == -1)
                            {
                                fd->mPollIndex = i;
                                fds[i].first = fd;
                                fds[i].second = mFD;
                                break;
                            }
                        }
                        if(fd->mPollIndex == -1)
                        {
                            fd->mPollIndex = fds.size();
                            fds.push_back(pair<LowFD *, int>(fd, mFD));
                        }
                    }
                    else
                    {
                        FD_CLR(fds[fd->mPollIndex].second, &read_set);
                        FD_CLR(fds[fd->mPollIndex].second, &write_set);
                        fds[fd->mPollIndex].second = mFD;
                    }

                    if(fd->mPollEvents & POLLIN)
                        FD_SET(fds[fd->mPollIndex].second, &read_set);
                    if(fd->mPollEvents & POLLOUT)
                        FD_SET(fds[fd->mPollIndex].second, &write_set);
                }
            }

            pthread_cond_broadcast(&low->web_thread_done_cond);
            pthread_mutex_unlock(&low->web_thread_mutex);
        }

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
        pthread_mutex_lock(&low->web_thread_mutex);
        low->web_thread_done = true;
        pthread_cond_broadcast(&low->web_thread_done_cond);

        // Remove the FDs here!
        for(int i = 0; i < fds.size(); i++)
        {
            if(fds[i].first->mFDClearOnReset && fds[i].second >= 0)
            {
                FD_CLR(fds[i].second, &read_set);
                FD_CLR(fds[i].second, &write_set);

                fds[i].second = -1;
                fds[i].first->mPollIndex = -1;
            }
        }

        while(low->destroying)
            pthread_cond_wait(&low->web_thread_done_cond,
                              &low->web_thread_mutex);
        pthread_mutex_unlock(&low->web_thread_mutex);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
#endif /* LOW_HAS_POLL */

    pthread_mutex_lock(&low->web_thread_mutex);
    low->web_thread_done = true;
    pthread_cond_broadcast(&low->web_thread_done_cond);

    pthread_mutex_unlock(&low->web_thread_mutex);
    return NULL;
}

// -----------------------------------------------------------------------------
//  low_web_thread_break
// -----------------------------------------------------------------------------

void low_web_thread_break(low_t *low)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    lowjs_esp32_break_web(false);
#else
    char c = 0xFF;
    write(low->web_thread_pipe[1], &c, 1);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}

// -----------------------------------------------------------------------------
//  low_web_set_poll_events
// -----------------------------------------------------------------------------

void low_web_set_poll_events(low_t *low, LowFD *fd, short events)
{
    pthread_mutex_lock(&low->web_thread_mutex);

    fd->mPollEvents = events;
    if(fd->mNextChanged || low->web_changed_last == fd)
    {
        pthread_mutex_unlock(&low->web_thread_mutex);
        return;
    }

    if(low->web_changed_last)
        low->web_changed_last->mNextChanged = fd;
    else
        low->web_changed_first = fd;
    low->web_changed_last = fd;

    low_web_thread_break(low);
    pthread_mutex_unlock(&low->web_thread_mutex);
}

// -----------------------------------------------------------------------------
//  low_web_clear_poll
// -----------------------------------------------------------------------------

void low_web_clear_poll(low_t *low, LowFD *fd)
{
    pthread_mutex_lock(&low->web_thread_mutex);
    while(true)
    {
        if(fd->mPollIndex == -1 && !fd->mNextChanged && fd != low->web_changed_last)
        {
            // Nothing to do
            pthread_mutex_unlock(&low->web_thread_mutex);
            return;
        }

        fd->mPollEvents = 0;
        if(!fd->mNextChanged && fd != low->web_changed_last)
        {
            if(low->web_changed_last)
                low->web_changed_last->mNextChanged = fd;
            else
                low->web_changed_first = fd;
            low->web_changed_last = fd;
        }

        // Make sure we are not handled
        if(low->web_thread_done)
            break;
        low_web_thread_break(low);
        pthread_cond_wait(&low->web_thread_done_cond, &low->web_thread_mutex);
    }
    pthread_mutex_unlock(&low->web_thread_mutex);
}

// -----------------------------------------------------------------------------
//  low_web_mark_delete
// -----------------------------------------------------------------------------

void low_web_mark_delete(low_t *low, LowFD *fd)
{
    pthread_mutex_lock(&low->web_thread_mutex);

    fd->mMarkDelete = true;
    if(fd->mNextChanged || low->web_changed_last == fd)
    {
        pthread_mutex_unlock(&low->web_thread_mutex);
        return;
    }

    if(low->web_changed_last)
        low->web_changed_last->mNextChanged = fd;
    else
        low->web_changed_first = fd;
    low->web_changed_last = fd;

    low_web_thread_break(low);
    pthread_mutex_unlock(&low->web_thread_mutex);
}
