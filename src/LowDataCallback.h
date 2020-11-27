// -----------------------------------------------------------------------------
//  LowDataCallback.h
// -----------------------------------------------------------------------------

#ifndef __LOWDATACALLBACK_H__
#define __LOWDATACALLBACK_H__

struct low_t;

class LowDataCallback
{
    friend void *low_data_thread_main(void *arg);
    friend bool low_reset(low_t *low);
    friend void low_data_set_callback(low_t *low,
                                      LowDataCallback *callback, int priority);
    friend void low_data_clear_callback(low_t *low,
                                        LowDataCallback *callback);

public:
    LowDataCallback(low_t *low)
        : mLow(low), mNext(nullptr), mInDataThread(false), mDataClearOnReset(true)
    {
    }
    virtual ~LowDataCallback() { low_data_clear_callback(mLow, this); }

protected:
    virtual bool OnData() = 0;

private:
    low_t *mLow;
    LowDataCallback *mNext;

    bool mInDataThread;

protected:
    bool mDataClearOnReset;
};

#endif /* __LOWDATACALLBACK_H__ */