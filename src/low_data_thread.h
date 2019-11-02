// -----------------------------------------------------------------------------
//  low_data_thread.h
// -----------------------------------------------------------------------------

#ifndef __LOW_DATA_THREAD_H__
#define __LOW_DATA_THREAD_H__

enum
{
    LOW_DATA_THREAD_PRIORITY_READ = 0,
    LOW_DATA_THREAD_PRIORITY_MODIFY
};

struct low_t;
class LowDataCallback;

void *low_data_thread_main(void *arg);

void low_data_set_callback(low_t *low, LowDataCallback *callback,
                           int priority);
void low_data_clear_callback(low_t *low, LowDataCallback *callback);

#endif /* __LOW_DATA_THREAD_H__ */