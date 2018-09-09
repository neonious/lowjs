// -----------------------------------------------------------------------------
//  LowSocketDirect.h
// -----------------------------------------------------------------------------

#ifndef __LOWSOCKETDIRECT_H__
#define __LOWSOCKETDIRECT_H__

class LowSocket;
class LowSocketDirect
{
    friend class LowSocket;

protected:
    virtual ~LowSocketDirect() {}

    virtual void SetSocket(LowSocket *socket) = 0;

    virtual void OnSocketConnected() {}
    virtual bool OnSocketData(unsigned char *data, int len) = 0;
    virtual bool OnSocketWrite() = 0;
};

#endif /* __LOWSOCKETDIRECT_H__ */