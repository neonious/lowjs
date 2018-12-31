// -----------------------------------------------------------------------------
//  LowHTTPDirect.cpp
// -----------------------------------------------------------------------------

#include "LowHTTPDirect.h"
#include "LowSocket.h"

#include "low_alloc.h"
#include "low_system.h"

#include <errno.h>

// -----------------------------------------------------------------------------
//  LowHTTPDirect::LowHTTPDirect
// -----------------------------------------------------------------------------

LowHTTPDirect::LowHTTPDirect(low_main_t *low, bool isServer) : LowLoopCallback(low), mLow(low), mIsServer(isServer),
                                                               mRequestCallID(0), mReadCallID(0), mWriteCallID(0),
                                                               mBytesRead(0), mBytesWritten(0), mParamFirst(NULL),
                                                               mParamLast(NULL), mWriteBufferCount(0), mShutdown(false),
                                                               mClosed(false), mEraseNextN(false), mReadData(NULL),
                                                               mRemainingRead(NULL), mReadError(false),
                                                               mWriteError(false), mHTTPError(false)
{
    pthread_mutex_init(&mMutex, NULL);
    Init();
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::~LowHTTPDirect
// -----------------------------------------------------------------------------

LowHTTPDirect::~LowHTTPDirect()
{
    if (mSocket)
    {
        mSocket->SetDirect(NULL, 0);
    }

    if (mRequestCallID)
    {
        low_remove_stash(mLow, mRequestCallID);
    }
    if (mReadCallID)
    {
        low_remove_stash(mLow, mReadCallID);
    }
    if (mWriteCallID)
    {
        low_remove_stash(mLow, mWriteCallID);
    }

    while (mParamFirst)
    {
        LowHTTPDirect_ParamData *param = mParamFirst;
        mParamFirst = mParamFirst->next;
        low_free(param);
    }

    for (int i = 0; i < mWriteBufferCount; i++)
    {
        if (mWriteBufferStashID[i])
        {
            low_remove_stash(mLow, mWriteBufferStashID[i]);
        }
    }

    pthread_mutex_destroy(&mMutex);
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::SetSocket
// -----------------------------------------------------------------------------

void LowHTTPDirect::SetSocket(LowSocket *socket)
{
    mSocket = socket;
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::Init
// -----------------------------------------------------------------------------

void LowHTTPDirect::Init()
{
    mIsRequest = false;
    mPhase = LOWHTTPDIRECT_PHASE_FIRSTLINE1;

    mAtTrailer = false;
    mContentLen = -1;
    mDataLen = 0;
    mChunkedEncoding = false;
    mNoBodyDefault = false;

    while (mParamFirst)
    {
        LowHTTPDirect_ParamData *param = mParamFirst;
        mParamFirst = mParamFirst->next;
        low_free(param);
    }
    mParamLast = NULL;

    mWriting = false;
    mWriteDone = false;

    if (mReadCallID)
    {
        low_remove_stash(mLow, mReadCallID);
        mReadCallID = 0;
    }
    if (mWriteCallID)
    {
        low_remove_stash(mLow, mWriteCallID);
        mWriteCallID = 0;
    }
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::Detach
// -----------------------------------------------------------------------------

void LowHTTPDirect::Detach(bool pushRemainingRead)
{
    if (pushRemainingRead)
    {
        low_web_clear_poll(mLow, mSocket);
        if (mRemainingRead)
        {
            memcpy(duk_push_fixed_buffer(mLow->duk_ctx, mRemainingReadLen), mRemainingRead, mRemainingReadLen);
        }
        else
        {
            duk_push_fixed_buffer(mLow->duk_ctx, 0);
        }
    }

    if (mSocket)
    {
        mSocket->SetDirect(NULL, 0);
        mSocket = NULL;
    }

    low_loop_set_callback(mLow, this);
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::SetRequestCallID
// -----------------------------------------------------------------------------

void LowHTTPDirect::SetRequestCallID(int callID)
{
    mRequestCallID = callID;
    if (mParamFirst || mClosed)
    {
        low_loop_set_callback(mLow, this);
    }
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::Read
// -----------------------------------------------------------------------------

void LowHTTPDirect::Read(unsigned char *data, int len, int callIndex)
{
    if (!mIsRequest || mReadData || !mSocket)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow, EAGAIN, "read");
        duk_call(mLow->duk_ctx, 1);
        return;
    }

    pthread_mutex_lock(&mMutex);
    mReadPos = 0;
    mReadLen = len;
    mReadData = data;

    if (mRemainingRead && !mClosed)
    {
        pthread_mutex_unlock(&mMutex);

        unsigned char *data = mRemainingRead;
        mRemainingRead = NULL;
        if (SocketData(data, mRemainingReadLen, true))
        {
            mSocket->TriggerDirect(LOWSOCKET_TRIGGER_READ);
        }
    }
    else
    {
        pthread_mutex_unlock(&mMutex);
    }

    if (mReadPos || mPhase == LOWHTTPDIRECT_PHASE_SENDING_RESPONSE || mClosed || mReadError || mHTTPError)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        if (mReadError || mHTTPError)
        {
            low_push_stash(mLow, mRequestCallID, false);
            if (mReadError)
            {
                mSocket->PushError(0);
            }
            else
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, "HTTP data not valid");
                duk_push_string(mLow->duk_ctx, "ERR_HTTP_PARSER");
                duk_put_prop_string(mLow->duk_ctx, -2, "code");
            }
            mReadError = mHTTPError = false;

            Detach();
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            duk_push_null(mLow->duk_ctx);
            mReadData = NULL;
            duk_push_int(mLow->duk_ctx, mReadPos);

            pthread_mutex_lock(&mLow->ref_mutex);
            int read = mBytesRead;
            mBytesRead = 0;
            pthread_mutex_unlock(&mLow->ref_mutex);

            duk_push_int(mLow->duk_ctx, read);

            if (!mReadPos)
            {
                duk_push_array(mLow->duk_ctx);
                int arr_ind = 0;

                while (mParamFirst)
                {
                    pthread_mutex_lock(&mMutex);
                    LowHTTPDirect_ParamData *param = mParamFirst;
                    mParamFirst = mParamFirst->next;
                    if (!mParamFirst)
                    {
                        mParamLast = NULL;
                    }
                    pthread_mutex_unlock(&mMutex);

                    int pos = 0;
                    while (param->data[pos])
                    {
                        int len = param->data[pos];
                        char next = param->data[pos + 1 + len];
                        param->data[pos + 1 + len] = 0;

                        duk_push_string(mLow->duk_ctx, param->data + pos + 1);
                        duk_put_prop_index(mLow->duk_ctx, -2, arr_ind++);

                        param->data[pos + 1 + len] = next;
                        pos += 1 + len;
                    }

                    low_free(param);
                }

                if (mIsServer)
                {
                    duk_call(mLow->duk_ctx, 4);
                }
                else
                {
                    Detach();

                    duk_push_boolean(mLow->duk_ctx, !mClosed && mWriteDone && !mWriteBufferCount);
                    duk_call(mLow->duk_ctx, 5);
                }
            }
            else
            {
                duk_call(mLow->duk_ctx, 3);
            }
        }
    }
    else
    {
        mReadCallID = low_add_stash(mLow, callIndex);
    }
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::WriteHeaders
// -----------------------------------------------------------------------------

void LowHTTPDirect::WriteHeaders(const char *txt, int index, int len, bool isChunked)
{
    if ((mIsServer && !mIsRequest) || mWriting)
    {
        return;
    }

    pthread_mutex_lock(&mMutex);
    mWriting = true;
    mWritePos = 0;
    mWriteLen = len;
    mWriteChunkedEncoding = isChunked;

    mWriteBuffers[0].iov_base = (unsigned char *) txt;
    mWriteBuffers[0].iov_len = strlen(txt);
    if (isChunked)
    {
        mWriteBuffers[0].iov_len -= 2;
    } // get rid of last \r\n, will be added with chunks
    mWriteBufferStashID[0] = low_add_stash(mLow, index);
    mWriteBufferCount = 1;

    DoWrite();
    pthread_mutex_unlock(&mMutex);
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::Write
// -----------------------------------------------------------------------------

void LowHTTPDirect::Write(unsigned char *data, int len, int bufferIndex, int callIndex)
{
    if ((mIsServer && !mIsRequest) || !mWriting || mWriteBufferCount > 1 || mWriteCallID || mWriteDone || !mSocket)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        low_push_error(mLow, EAGAIN, "write");
        duk_call(mLow->duk_ctx, 1);
        return;
    }

    pthread_mutex_lock(&mMutex);
    if (len == 0)
    {
        mWriteDone = true;
        if (mWriteChunkedEncoding)
        {
            sprintf(mWriteChunkedHeaderLine, "\r\n0\r\n\r\n");
            mWriteBuffers[mWriteBufferCount].iov_base = mWriteChunkedHeaderLine;
            mWriteBuffers[mWriteBufferCount].iov_len = strlen(mWriteChunkedHeaderLine);
            mWriteBufferStashID[mWriteBufferCount] = 0;
            mWriteBufferCount++;
        }
    }
    else
    {
        mWritePos += len;

        if (mWriteChunkedEncoding)
        {
            sprintf(mWriteChunkedHeaderLine, "\r\n%x\r\n", len);
            mWriteBuffers[mWriteBufferCount].iov_base = mWriteChunkedHeaderLine;
            mWriteBuffers[mWriteBufferCount].iov_len = strlen(mWriteChunkedHeaderLine);
            mWriteBufferStashID[mWriteBufferCount] = 0;
            mWriteBufferCount++;
        }

        mWriteBuffers[mWriteBufferCount].iov_base = data;
        mWriteBuffers[mWriteBufferCount].iov_len = len;
        mWriteBufferStashID[mWriteBufferCount] = low_add_stash(mLow, bufferIndex);
        mWriteBufferCount++;
    }

    DoWrite();
    pthread_mutex_unlock(&mMutex);

    if (!mWriteBufferCount || mWriteError)
    {
        duk_dup(mLow->duk_ctx, callIndex);
        if (mWriteError)
        {
            mSocket->PushError(1);
            mWriteError = false;

            Detach();
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            duk_push_null(mLow->duk_ctx);
            duk_push_int(mLow->duk_ctx, mBytesWritten);
            mBytesWritten = 0;
            duk_call(mLow->duk_ctx, 2);
        }

        if (!mWriteBufferCount && mWriteDone) // we need to recheck b/c of duk_call
        {
            if (!mIsServer)
            {
                if (!mShutdown && !mWriteChunkedEncoding && (mWriteLen < 0 || mWritePos != mWriteLen))
                {
                    mSocket->Shutdown();
                    mShutdown = true;
                }
            }
            else if ((!mWriteChunkedEncoding && (mWriteLen < 0 || mWritePos != mWriteLen)) ||
                     mPhase != LOWHTTPDIRECT_PHASE_SENDING_RESPONSE)
            {
                mSocket->Close();
            }
            else
            {
                Init();
            }
        }
    }
    else
    {
        mWriteCallID = low_add_stash(mLow, callIndex);
        mSocket->TriggerDirect(LOWSOCKET_TRIGGER_WRITE);
    }
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::DoWrite
// -----------------------------------------------------------------------------

void LowHTTPDirect::DoWrite()
{
    while (mWriteBufferCount)
    {
        int size = mSocket->writev(mWriteBuffers, mWriteBufferCount);
        if (size < 0)
        {
            if (errno != EAGAIN)
            {
                mWriteError = true;
            }
            return;
        }
        mBytesWritten += size;

        while (mWriteBufferCount && size >= mWriteBuffers[0].iov_len)
        {
            size -= mWriteBuffers[0].iov_len;
            low_remove_stash(mLow, mWriteBufferStashID[0]);

            mWriteBuffers[0].iov_base = mWriteBuffers[1].iov_base;
            mWriteBuffers[0].iov_len = mWriteBuffers[1].iov_len;
            mWriteBufferStashID[0] = mWriteBufferStashID[1];

            mWriteBuffers[1].iov_base = mWriteBuffers[2].iov_base;
            mWriteBuffers[1].iov_len = mWriteBuffers[2].iov_len;
            mWriteBufferStashID[1] = mWriteBufferStashID[2];

            mWriteBufferStashID[2] = 0;
            mWriteBufferCount--;
        }
        if (size)
        {
            mWriteBuffers[0].iov_base = ((unsigned char *) mWriteBuffers[0].iov_base) + size;
            mWriteBuffers[0].iov_len -= size;
        }
    }
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::OnLoop
// -----------------------------------------------------------------------------

bool LowHTTPDirect::OnLoop()
{
    if (!mSocket)
    {
        return false;
    }
    if (!mRequestCallID)
    {
        return true;
    }

    if (!mIsRequest)
    {
        if (mClosed || mReadError || mHTTPError)
        {
            low_push_stash(mLow, mRequestCallID, false);
            if (mReadError)
            {
                mSocket->PushError(0);
            }
            else if (mHTTPError)
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, "HTTP data not valid");
                duk_push_string(mLow->duk_ctx, "ERR_HTTP_PARSER");
                duk_put_prop_string(mLow->duk_ctx, -2, "code");
            }
            else
            {
                low_push_error(mLow, ECONNRESET, "read");
            }
            mReadError = mHTTPError = false;

            Detach();
            duk_call(mLow->duk_ctx, 1);
        }
        if (mParamFirst && mParamFirst->type == LOWHTTPDIRECT_PARAMDATA_HEADER && mAtTrailer)
        {
            low_push_stash(mLow, mRequestCallID, false);
            duk_push_null(mLow->duk_ctx);

            duk_push_array(mLow->duk_ctx);
            int arr_ind = 0;

            while (mParamFirst && mParamFirst->type == LOWHTTPDIRECT_PARAMDATA_HEADER)
            {
                pthread_mutex_lock(&mMutex);
                LowHTTPDirect_ParamData *param = mParamFirst;
                mParamFirst = mParamFirst->next;
                if (!mParamFirst)
                {
                    mParamLast = NULL;
                }
                pthread_mutex_unlock(&mMutex);

                int pos = 0;
                while (param->data[pos])
                {
                    int len = (unsigned int) (unsigned char) param->data[pos];
                    char next = param->data[pos + 1 + len];
                    param->data[pos + 1 + len] = 0;

                    duk_push_string(mLow->duk_ctx, param->data + pos + 1);
                    duk_put_prop_index(mLow->duk_ctx, -2, arr_ind++);

                    param->data[pos + 1 + len] = next;
                    pos += 1 + len;
                }

                low_free(param);
            }

            pthread_mutex_lock(&mLow->ref_mutex);
            int read = mBytesRead;
            mBytesRead = 0;
            pthread_mutex_unlock(&mLow->ref_mutex);
            duk_push_int(mLow->duk_ctx, read);

            mIsRequest = true;
            duk_call(mLow->duk_ctx, 3);
        }
    }

    if (mReadCallID && (mReadPos || mPhase == LOWHTTPDIRECT_PHASE_SENDING_RESPONSE || mClosed))
    {
        pthread_mutex_lock(&mMutex);

        int callID = mReadCallID;
        mReadCallID = 0;
        low_push_stash(mLow, callID, true);

        if (mReadError || mHTTPError)
        {
            low_push_stash(mLow, mRequestCallID, false);
            if (mReadError)
            {
                mSocket->PushError(0);
            }
            else
            {
                duk_push_error_object(mLow->duk_ctx, DUK_ERR_ERROR, "HTTP data not valid");
                duk_push_string(mLow->duk_ctx, "ERR_HTTP_PARSER");
                duk_put_prop_string(mLow->duk_ctx, -2, "code");
            }
            mReadError = mHTTPError = false;
            pthread_mutex_unlock(&mMutex);

            Detach();
            duk_call(mLow->duk_ctx, 1);
        }
        else
        {
            mReadData = NULL;

            duk_push_null(mLow->duk_ctx);
            duk_push_int(mLow->duk_ctx, mReadPos);
            pthread_mutex_unlock(&mMutex);

            pthread_mutex_lock(&mLow->ref_mutex);
            int read = mBytesRead;
            mBytesRead = 0;
            pthread_mutex_unlock(&mLow->ref_mutex);
            duk_push_int(mLow->duk_ctx, read);

            if (!mReadPos)
            {
                duk_push_array(mLow->duk_ctx);
                int arr_ind = 0;

                while (mParamFirst)
                {
                    pthread_mutex_lock(&mMutex);
                    LowHTTPDirect_ParamData *param = mParamFirst;
                    mParamFirst = mParamFirst->next;
                    if (!mParamFirst)
                    {
                        mParamLast = NULL;
                    }
                    pthread_mutex_unlock(&mMutex);

                    int pos = 0;
                    while (param->data[pos])
                    {
                        int len = param->data[pos];
                        char next = param->data[pos + 1 + len];
                        param->data[pos + 1 + len] = 0;

                        duk_push_string(mLow->duk_ctx, param->data + pos + 1);
                        duk_put_prop_index(mLow->duk_ctx, -2, arr_ind++);

                        param->data[pos + 1 + len] = next;
                        pos += 1 + len;
                    }

                    low_free(param);
                }

                if (mIsServer)
                {
                    duk_call(mLow->duk_ctx, 4);
                }
                else
                {
                    Detach();

                    duk_push_boolean(mLow->duk_ctx, !mClosed && mWriteDone && !mWriteBufferCount);
                    duk_call(mLow->duk_ctx, 5);
                }
            }
            else
            {
                duk_call(mLow->duk_ctx, 3);
            }
        }
    }
    if (!mWriteBufferCount || mWriteError)
    {
        if (mWriteCallID)
        {
            int callID = mWriteCallID;
            mWriteCallID = 0;
            low_push_stash(mLow, callID, true);

            if (mWriteError)
            {
                mSocket->PushError(1);
                mWriteError = false;

                Detach();
                duk_call(mLow->duk_ctx, 1);
            }
            else
            {
                duk_push_null(mLow->duk_ctx);
                duk_push_int(mLow->duk_ctx, mBytesWritten);
                mBytesWritten = 0;
                duk_call(mLow->duk_ctx, 2);
            }
        }
        if (!mWriteBufferCount && mWriteDone) // we need to recheck b/c of duk_call
        {
            if (!mIsServer)
            {
                if (!mShutdown && !mWriteChunkedEncoding && (mWriteLen < 0 || mWritePos != mWriteLen))
                {
                    mSocket->Shutdown();
                    mShutdown = true;
                }
            }
            else if ((!mWriteChunkedEncoding && (mWriteLen < 0 || mWritePos != mWriteLen)) ||
                     mPhase != LOWHTTPDIRECT_PHASE_SENDING_RESPONSE)
            {
                mSocket->Close();
            }
            else
            {
                Init();
            }
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::OnSocketData
// -----------------------------------------------------------------------------

bool LowHTTPDirect::OnSocketData(unsigned char *data, int len)
{
    bool res = SocketData(data, len, false);
    return res;
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::SocketData
// -----------------------------------------------------------------------------

#if !LOW_HAS_STRCASESTR

char *strcasestr(const char *s, const char *find);

#endif /* !LOW_HAS_STRCASESTR */

bool LowHTTPDirect::SocketData(unsigned char *data, int len, bool inLoop)
{
    bool setCallback = false;

    if (len == 0 || mClosed)
    {
        goto done;
    }
    if (len < 0)
    {
        mReadError = true;
        goto done;
    }

    while (len--)
    {
        unsigned char c = *data++;
        if (mEraseNextN && c == '\n')
        {
            mEraseNextN = false;
            continue;
        }
        mEraseNextN = mPhase != LOWHTTPDIRECT_PHASE_BODY && c == '\r';

        if (mPhase == LOWHTTPDIRECT_PHASE_SENDING_RESPONSE)
        {
            goto err;
        }

        LowHTTPDirect_ParamData *param = mParamLast;
        if (mPhase != LOWHTTPDIRECT_PHASE_BODY && mPhase != LOWHTTPDIRECT_PHASE_CHUNK_HEADER &&
            (!param || mParamPos + 2 >= sizeof(param->data)))
        {
            LowHTTPDirect_ParamData *newParam = (LowHTTPDirect_ParamData *) low_alloc(sizeof(LowHTTPDirect_ParamData));
            if (!newParam)
            {
                mSocket->SetError(false, ENOMEM, false);
                goto done;
            }
            newParam->next = NULL;
            newParam->type = mAtTrailer ? LOWHTTPDIRECT_PARAMDATA_TRAILER : LOWHTTPDIRECT_PARAMDATA_HEADER;
            pthread_mutex_lock(&mMutex);
            if (param)
            {
                if (mParamStart == 1)
                {
                    goto err;
                }

                param->next = newParam;
                if (mParamStart != mParamPos)
                {
                    memcpy(newParam->data + 1, param->data + mParamStart, mParamPos - mParamStart);
                }
                param->data[mParamStart - 1] = '\0';
                mParamPos -= mParamStart - 1;
                mParamPosNonSpace -= mParamStart - 1;
                mParamStart = 1;
            }
            else
            {
                mParamFirst = mParamLast = newParam;
                mParamPos = mParamPosNonSpace = mParamStart = 1;
            }
            pthread_mutex_unlock(&mMutex);

            param = newParam;
        }

        bool isSpace = c == ' ' || c == '\t';
        if (mPhase == LOWHTTPDIRECT_PHASE_CHUNK_HEADER)
        {
            if (mParamStart == sizeof(mChunkedHeaderLine))
            {
                goto err;
            }

            if (c == '\r' || c == '\n')
            {
                if (mParamStart)
                {
                    mChunkedHeaderLine[mParamStart++] = '\0';

                    int i;
                    if (sscanf(mChunkedHeaderLine, "%x", &i) == 1)
                    {
                        if (i == 0)
                        {
                            mPhase = LOWHTTPDIRECT_PHASE_HEADER_KEY;
                        }
                        else
                        {
                            mContentLen += i;
                            mPhase = LOWHTTPDIRECT_PHASE_BODY;
                        }
                    }
                    else
                    {
                        goto err;
                    }
                }
            }
            else
            {
                mChunkedHeaderLine[mParamStart++] = c;
            }
        }
        else if (mPhase == LOWHTTPDIRECT_PHASE_BODY)
        {
            len++;
            data--; // we need more than 1 char

            int size = len;
            if (mContentLen >= 0 && size > mContentLen - mDataLen)
            {
                size = mContentLen - mDataLen;
            }

            pthread_mutex_lock(&mMutex);
            if (!mReadData || mReadPos == mReadLen)
            {
                mRemainingReadLen = len;
                mRemainingRead = data;
                pthread_mutex_unlock(&mMutex);
                break;
            }
            if (size > mReadLen - mReadPos)
            {
                size = mReadLen - mReadPos;
            }

            memcpy(mReadData + mReadPos, data, size);
            mReadPos += size;
            pthread_mutex_unlock(&mMutex);

            mDataLen += size;
            data += size;
            len -= size;

            if (mContentLen >= 0 && mDataLen == mContentLen)
            {
                if (mChunkedEncoding)
                {
                    mPhase = LOWHTTPDIRECT_PHASE_CHUNK_HEADER;
                    mParamStart = 0;
                }
                else
                {
                    mPhase = LOWHTTPDIRECT_PHASE_SENDING_RESPONSE;
                }
            }
            setCallback = true;
        }
        else if (mParamStart == mParamPos &&
                 (isSpace || ((c == '\r' || c == '\n') && mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE1)))
        {
            continue;
        }
        else
        {
            if (mEraseNextN)
            {
                c = '\n';
            }

            bool done = false;
            if (mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE1 && isSpace)
            {
                mPhase = LOWHTTPDIRECT_PHASE_FIRSTLINE2;
                if (!mIsServer)
                {
                    if (mParamPosNonSpace - mParamStart < 5 // x/1.0
                        || param->data[mParamPosNonSpace - 4] != '/' || param->data[mParamPosNonSpace - 2] != '.')
                    {
                        goto err;
                    }

                    param->data[mParamStart - 1] = 3;
                    param->data[mParamStart] = param->data[mParamPosNonSpace - 3];
                    param->data[mParamStart + 1] = '.';
                    param->data[mParamStart + 2] = param->data[mParamPosNonSpace - 1];
                    mParamPos = mParamStart = mParamPosNonSpace = mParamStart + 4;

                    continue;
                }
                else
                {
                    param->data[mParamPosNonSpace] = '\0';
                    mNoBodyDefault = strcmp(param->data + mParamStart, "OPTIONS") == 0 ||
                                     strcmp(param->data + mParamStart, "GET") == 0 ||
                                     strcmp(param->data + mParamStart, "HEAD") == 0 ||
                                     strcmp(param->data + mParamStart, "UNLOCK") == 0 ||
                                     strcmp(param->data + mParamStart, "MKCOL") == 0 ||
                                     strcmp(param->data + mParamStart, "COPY") == 0 ||
                                     strcmp(param->data + mParamStart, "MOVE") == 0 ||
                                     strcmp(param->data + mParamStart, "DELETE") == 0 ||
                                     strcmp(param->data + mParamStart, "CONNECT") == 0;
                    done = true;
                }
            }
            else if (mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE2 && isSpace)
            {
                mPhase = LOWHTTPDIRECT_PHASE_FIRSTLINE3;
                done = true;
            }
            else if (mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE3 && c == '\n')
            {
                mPhase = LOWHTTPDIRECT_PHASE_HEADER_KEY;
                if (mIsServer)
                {
                    if (mParamPosNonSpace - mParamStart < 5 // x/1.0
                        || param->data[mParamPosNonSpace - 4] != '/' || param->data[mParamPosNonSpace - 2] != '.')
                    {
                        goto err;
                    }

                    param->data[mParamStart - 1] = 3;
                    param->data[mParamStart] = param->data[mParamPosNonSpace - 3];
                    param->data[mParamStart + 1] = '.';
                    param->data[mParamStart + 2] = param->data[mParamPosNonSpace - 1];
                    mParamPos = mParamStart = mParamPosNonSpace = mParamStart + 4;

                    continue;
                }
                else
                {
                    done = true;
                }
            }
            else if (mPhase == LOWHTTPDIRECT_PHASE_HEADER_VALUE && c == '\n')
            {
                if (mIsContentLengthHeader)
                {
                    param->data[mParamPosNonSpace] = '\0';
                    mContentLen = atoi(param->data + mParamStart);
                }
                else if (mIsTransferEncodingHeader)
                {
                    param->data[mParamPosNonSpace] = '\0';
                    mChunkedEncoding = strcasestr(param->data + mParamStart, "chunked") != NULL;
                }

                mPhase = LOWHTTPDIRECT_PHASE_HEADER_KEY;
                done = true;
            }
            else if (mPhase == LOWHTTPDIRECT_PHASE_HEADER_KEY && c == ':')
            {
                param->data[mParamPosNonSpace] = '\0';
                mIsContentLengthHeader = strcmp(param->data + mParamStart, "content-length") == 0;
                mIsTransferEncodingHeader =
                    !mIsContentLengthHeader && strcmp(param->data + mParamStart, "transfer-encoding") == 0;

                mPhase = LOWHTTPDIRECT_PHASE_HEADER_VALUE;
                done = true;
            }
            else if (mPhase == LOWHTTPDIRECT_PHASE_HEADER_KEY && c == '\n')
            {
                if (mParamStart == mParamPos)
                {
                    param->data[mParamStart - 1] = '\0';

                    if (mAtTrailer)
                    {
                        mPhase = LOWHTTPDIRECT_PHASE_SENDING_RESPONSE;
                    }
                    else
                    {
                        mAtTrailer = true;
                        if (mContentLen == -1 && !mChunkedEncoding && mNoBodyDefault)
                        {
                            mContentLen = 0;
                        }
                        if (mChunkedEncoding)
                        {
                            mPhase = LOWHTTPDIRECT_PHASE_CHUNK_HEADER;
                            mParamStart = 0;
                            mContentLen = 0;
                        }
                        else if (mContentLen != 0)
                        {
                            mPhase = LOWHTTPDIRECT_PHASE_BODY;
                        }
                        else
                        {
                            mPhase = LOWHTTPDIRECT_PHASE_SENDING_RESPONSE;
                        }
                    }
                    setCallback = true;
                }
                else
                {
                    // Header without value
                    param->data[mParamStart - 1] = mParamPosNonSpace - mParamStart;
                    param->data[mParamPosNonSpace] = 0;
                    mParamPos = mParamStart = mParamPosNonSpace + 2;
                }
                continue;
            }
            if (done)
            {
                if (mParamPosNonSpace - mParamStart > 255)
                {
                    mParamPosNonSpace = mParamStart + 255;
                }
                param->data[mParamStart - 1] = mParamPosNonSpace - mParamStart;
                mParamPos = mParamStart = mParamPosNonSpace = mParamPosNonSpace + 1;
            }
            else
            {
                if (mPhase == LOWHTTPDIRECT_PHASE_HEADER_KEY)
                {
                    param->data[mParamPos++] = tolower(c);
                }
                else if (mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE1 ||
                         (mIsServer && mPhase == LOWHTTPDIRECT_PHASE_FIRSTLINE3))
                {
                    param->data[mParamPos++] = toupper(c);
                }
                else
                {
                    param->data[mParamPos++] = c;
                }

                if (!isSpace)
                {
                    mParamPosNonSpace = mParamPos;
                }
            }
        }
    }

    pthread_mutex_lock(&mLow->ref_mutex);
    mBytesRead += len;
    pthread_mutex_unlock(&mLow->ref_mutex);
    if (mRequestCallID && setCallback && !inLoop)
    {
        low_loop_set_callback(mLow, this);
    }
    return !mRemainingRead;

err:
    mHTTPError = true;
done:
    mClosed = true;
    if (mRequestCallID)
    {
        low_loop_set_callback(mLow, this);
    }

    return false;
}

// -----------------------------------------------------------------------------
//  LowHTTPDirect::OnSocketWrite
// -----------------------------------------------------------------------------

bool LowHTTPDirect::OnSocketWrite()
{
    pthread_mutex_lock(&mMutex);
    DoWrite();
    if (mWriteCallID && (!mWriteBufferCount || mWriteError))
    {
        low_loop_set_callback(mLow, this);
    }
    bool res = mWriteBufferCount != 0;
    pthread_mutex_unlock(&mMutex);

    return res;
}
