// -----------------------------------------------------------------------------
//  LowDataCallback.h
// -----------------------------------------------------------------------------

#ifndef __LOWDATACALLBACK_H__
#define __LOWDATACALLBACK_H__

struct low_main_t;

class LowDataCallback
{
  friend void *low_data_thread_main(void *arg);

  friend bool low_reset(low_main_t *low);

  friend void low_data_set_callback(low_main_t *low, LowDataCallback *callback, int priority);

  friend void low_data_clear_callback(low_main_t *low, LowDataCallback *callback);

public:
  LowDataCallback(low_main_t *low) : mLow(low), mNext(nullptr), mDataClearOnReset(true)
  {
  }

  virtual ~LowDataCallback()
  { low_data_clear_callback(mLow, this); }

protected:
  virtual bool OnData() = 0;

private:
  low_main_t *mLow;
  LowDataCallback *mNext;

protected:
  bool mDataClearOnReset;
};

#endif /* __LOWDATACALLBACK_H__ */