// -----------------------------------------------------------------------------
//  LowHTTPDirect.h
// -----------------------------------------------------------------------------

#ifndef __LOWHTTPDIRECT_H__
#define __LOWHTTPDIRECT_H__

#include "LowLoopCallback.h"
#include "LowSocketDirect.h"

#include <pthread.h>

#include "low_config.h"
#if LOW_ESP32_LWIP_SPECIALITIES
#include <lwip/sockets.h>
#else
#include <sys/uio.h>
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

using namespace std;

enum LowHTTPDirect_Phase
{
    LOWHTTPDIRECT_PHASE_FIRSTLINE1,
    LOWHTTPDIRECT_PHASE_FIRSTLINE2,
    LOWHTTPDIRECT_PHASE_FIRSTLINE3,
    LOWHTTPDIRECT_PHASE_HEADER_KEY,
    LOWHTTPDIRECT_PHASE_HEADER_VALUE,
    LOWHTTPDIRECT_PHASE_CHUNK_HEADER,
    LOWHTTPDIRECT_PHASE_BODY,
    LOWHTTPDIRECT_PHASE_SENDING_RESPONSE
};

enum LowHTTPDirect_ParamDataType
{
    LOWHTTPDIRECT_PARAMDATA_HEADER = 0,
    LOWHTTPDIRECT_PARAMDATA_TRAILER
};

struct LowHTTPDirect_ParamData
{
    LowHTTPDirect_ParamData *next;
    LowHTTPDirect_ParamDataType type;
    int size;
    char data[10240];
};

class LowSocket;
class LowHTTPDirect
    : public LowSocketDirect
    , public LowLoopCallback
{
  public:
    LowHTTPDirect(low_main_t *low, bool isServer);
    virtual ~LowHTTPDirect();

    virtual void SetSocket(LowSocket *socket);
    void Detach(bool pushRemainingRead = false);

    void SetRequestCallID(int callID);
    void Read(unsigned char *data, int len, int callIndex);

    void WriteHeaders(const char *txt, int index, int len, bool isChunked);
    void Write(unsigned char *data, int len, int bufferIndex, int callIndex);

  protected:
    void Init();

    virtual bool OnLoop();

    virtual bool OnSocketData(unsigned char *data, int len);
    bool SocketData(unsigned char *data, int len, bool inLoop);

    void DoWrite();
    virtual bool OnSocketWrite();

  private:
    low_main_t *mLow;
    bool mIsServer, mDetached;

    LowSocket *mSocket;
    int mRequestCallID, mReadCallID, mWriteCallID;

    pthread_mutex_t mMutex;
    LowHTTPDirect_Phase mPhase;
    bool mIsRequest;
    int mBytesRead;
    int mBytesWritten;

    bool mShutdown, mClosed, mEraseNextN, mAtTrailer;

    int mContentLength;
    bool mNoBodyDefault, mChunkedEncoding;

    int mContentLen, mDataLen;

    LowHTTPDirect_ParamData *mParamFirst, *mParamLast;
    unsigned char *mParamCurrent;
    int mParamStart, mParamPos, mParamPosNonSpace;

    char mChunkedHeaderLine[10];
    bool mIsContentLengthHeader, mIsTransferEncodingHeader;

    unsigned char *mRemainingRead;
    int mRemainingReadLen;

    unsigned char *mReadData;
    int mReadPos, mReadLen;

    char mWriteChunkedHeaderLine[10];
    struct iovec mWriteBuffers[3];
    int mWriteBufferStashID[3];
    int mWritePos, mWriteLen;
    uint8_t mWriteBufferCount, mWriteBufferStashInvalidCount;
    bool mWriting, mWriteDone, mWriteChunkedEncoding;

    bool mReadError, mWriteError, mHTTPError;
};

#endif /* __LOWHTTPDIRECT_H__ */