// -----------------------------------------------------------------------------
//  low_web_thread.h
// -----------------------------------------------------------------------------

#ifndef __LOW_WEB_THREAD_H__
#define __LOW_WEB_THREAD_H__

#include <duktape.h>

#if LOW_HAS_POLL
#include <poll.h>
#else
#define POLLIN 0x01
#define POLLOUT 0x04
#define POLLERR 0x08
#define POLLHUP 0x10
#endif /* LOW_HAS_POLL */

struct low_t;
class LowFD;

void *low_web_thread_main(void *arg);
void low_web_thread_break(low_t *low);

void low_web_set_poll_events(low_t *low, LowFD *fd, short events);

void low_web_clear_poll(low_t *low,
                        LowFD *fd); // only call from not-web thread
void low_web_mark_delete(low_t *low, LowFD *fd);

void low_set_socket_events(duk_context *ctx, int events,
                           void (*func)(void *userdata), void *userdata);

#endif /* __LOW_WEB_THREAD_H__ */
