// -----------------------------------------------------------------------------
//  low_web_thread.h
// -----------------------------------------------------------------------------

#ifndef __LOW_WEB_THREAD_H__
#define __LOW_WEB_THREAD_H__

#if LOW_HAS_POLL
#include <poll.h>
#else
#define POLLIN 1
#define POLLOUT 2
#define POLLERR 4
#define POLLHUP 8
#endif /* LOW_HAS_POLL */
#define POLLRESET 0xFF

struct low_main_t;
class LowFD;

void *low_web_thread_main(void *arg);
void low_web_thread_break(low_main_t *low);

void low_web_set_poll_events(low_main_t *low, LowFD *fd, short events);

void low_web_clear_poll(low_main_t *low,
                        LowFD *fd); // only call from not-web thread
void low_web_mark_delete(low_main_t *low, LowFD *fd);

#endif /* __LOW_WEB_THREAD_H__ */
