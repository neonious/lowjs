// -----------------------------------------------------------------------------
//  LowServerSocket.h
// -----------------------------------------------------------------------------

#ifndef __LOWSERVERSOCKET_H__
#define __LOWSERVERSOCKET_H__

#include "LowFD.h"

struct low_main_t;

class LowServerSocket : public LowFD
{
public:
  LowServerSocket(low_main_t *low, bool isHTTP, LowTLSContext *secureContext);

  virtual ~LowServerSocket();

  bool Listen(struct sockaddr *addr, int addrLen, int callIndex, int &err, const char *&syscall);

  void Read(int pos, unsigned char *data, int len, int callIndex);

  void Write(int pos, unsigned char *data, int len, int callIndex);

  bool Close(int callIndex);

protected:
  virtual bool OnEvents(short events);

private:
  low_main_t *mLow;
  bool mIsHTTP;

  int mFamily, mAcceptCallID;

  LowTLSContext *mSecureContext;
};

#endif /* __LOWSERVERSOCKET_H__ */