// -----------------------------------------------------------------------------
//  LowFD.h
// -----------------------------------------------------------------------------

#ifndef __LOWFD_H__
#define __LOWFD_H__

#include "low_main.h"
#include "low_web_thread.h"

enum LowFDType
{
  LOWFD_TYPE_FILE, LOWFD_TYPE_SERVER, LOWFD_TYPE_SOCKET, LOWFD_TYPE_CUSTOM
};

class LowFD
{
  friend void *low_web_thread_main(void *arg);

  friend bool low_reset(low_main_t *low);

  friend void low_web_set_poll_events(low_main_t *low, LowFD *fd, short events);

  friend void low_web_clear_poll(low_main_t *low, LowFD *fd);

  friend void low_web_mark_delete(low_main_t *low, LowFD *fd);

public:
  LowFD(low_main_t *low, LowFDType type, int fd = -1) : mLow(low), mFD(fd), mAdvertisedFD(-1), mFDType(type),
                                                        mMarkDelete(false), mPollIndex(-1), mPollEvents(0),
                                                        mNextChanged(nullptr), mFDClearOnReset(true)
  {
  }

  virtual ~LowFD()
  {
      if (mAdvertisedFD >= 0)
      {
          mLow->fds.erase(mAdvertisedFD);
      }
      low_web_clear_poll(mLow, this);
  }

  virtual void Read(int pos, unsigned char *data, int len, int callIndex) = 0;

  virtual void Write(int pos, unsigned char *data, int len, int callIndex) = 0;

  virtual bool Close(int callIndex) = 0;

  LowFDType FDType()
  { return mFDType; }

  int &FD()
  { return mFD; }

  int PollEvents()
  { return mPollEvents; }

  void SetFD(int fd)
  { mFD = fd; }

  void AdvertiseFD()
  {
      if (mAdvertisedFD >= 0)
      {
          mLow->fds.erase(mAdvertisedFD);
      }
      mAdvertisedFD = mFD;
      if (mAdvertisedFD >= 0)
      {
          mLow->fds[mAdvertisedFD] = this;
      }
  }

protected:
  virtual bool OnEvents(short events)
  { return true; }

private:
  low_main_t *mLow;

  int mFD, mAdvertisedFD;
  LowFDType mFDType;
  bool mMarkDelete;

  int mPollIndex;
  short mPollEvents;

  LowFD *mNextChanged;

protected:
  bool mFDClearOnReset;
};

#endif /* __LOWFD_H__ */