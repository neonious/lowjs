// -----------------------------------------------------------------------------
//  low_fs.cpp
// -----------------------------------------------------------------------------

#include "low_fs.h"
#include "LowFile.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


// -----------------------------------------------------------------------------
//  low_fs_open
// -----------------------------------------------------------------------------

duk_ret_t low_fs_open(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    const char *path = duk_require_string(ctx, 0);

    int iflags = 0;
    if(duk_is_string(ctx, 1))
    {
        // TODO: handle x, s
        const char *flags = duk_require_string(ctx, 1);
        if(flags[0] == 'a')
            iflags = O_WRONLY | O_APPEND | O_CREAT;
        if(flags[0] == 'r')
            iflags = O_RDONLY;
        if(flags[0] == 'w')
            iflags = O_WRONLY | O_CREAT | O_TRUNC;

        for(int i = 1; flags[i]; i++)
            if(flags[i] == '+')
                iflags = (iflags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
    }
    else
        iflags = duk_require_int(ctx, 1);
#if LOW_ESP32_LWIP_SPECIALITIES
    if(iflags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC))
        duk_range_error(ctx, "flags not supported");
#else
    if(iflags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC |
                  O_CLOEXEC))
        duk_range_error(ctx, "flags not supported");
    iflags |= O_CLOEXEC; // on spawn, close file
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    new(ctx) LowFile(low, path, iflags, duk_is_undefined(ctx, 3) ? 2 : 3);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_open_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_open_sync(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    const char *path = duk_require_string(ctx, 0);

    int iflags = 0;
    if(duk_is_string(ctx, 1))
    {
        // TODO: handle x, s
        const char *flags = duk_require_string(ctx, 1);
        if(flags[0] == 'a')
            iflags = O_WRONLY | O_APPEND | O_CREAT;
        if(flags[0] == 'r')
            iflags = O_RDONLY;
        if(flags[0] == 'w')
            iflags = O_WRONLY | O_CREAT | O_TRUNC;

        for(int i = 1; flags[i]; i++)
            if(flags[i] == '+')
                iflags = (iflags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
    }
    else
        iflags = duk_require_int(ctx, 1);
#if LOW_ESP32_LWIP_SPECIALITIES
    if(iflags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC))
        duk_range_error(ctx, "flags not supported");
#else
    if(iflags & ~(O_RDONLY | O_WRONLY | O_RDWR | O_APPEND | O_CREAT | O_TRUNC |
                  O_CLOEXEC))
        duk_range_error(ctx, "flags not supported");
    iflags |= O_CLOEXEC; // on spawn, close file
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    LowFile *file = new(ctx) LowFile(low, path, iflags, 0);
    while(true)
    {
        low_loop_clear_callback(low, file);
        if(file->FinishPhase())
            break;

        pthread_mutex_lock(&low->loop_thread_mutex);
        while(!file->LowLoopCallback::mNext && low->loop_callback_last != file)
            low_loop_wait(low->duk_ctx, -1);
        pthread_mutex_unlock(&low->loop_thread_mutex);
    }

    duk_push_int(ctx, file->FD());
    return 1;
}

// -----------------------------------------------------------------------------
//  low_fs_close
// -----------------------------------------------------------------------------

duk_ret_t low_fs_close(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    LowFD *file = iter->second;

    if(!file->Close(1))
    {
        delete file;

        duk_dup(low->duk_ctx, 1);
        duk_push_null(low->duk_ctx);
        low_call_next_tick(low->duk_ctx, 1);
    }
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_close_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_close_sync(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    if(iter->second->FDType() != LOWFD_TYPE_FILE)
        duk_reference_error(ctx, "file descriptor is not a file");
    LowFile *file = (LowFile *)iter->second;

    if(!file->Close(-1))
    {
        delete file;
        return 0;
    }

    while(true)
    {
        low_loop_clear_callback(low, file);
        if(file->FinishPhase())
        {
            delete file;
            return 0;
        }

        pthread_mutex_lock(&low->loop_thread_mutex);
        while(!file->LowLoopCallback::mNext && low->loop_callback_last != file)
            low_loop_wait(low->duk_ctx, -1);
        pthread_mutex_unlock(&low->loop_thread_mutex);
    }

    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_read
// -----------------------------------------------------------------------------

duk_ret_t low_fs_read(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    duk_size_t buf_len;
    unsigned char *buf =
      (unsigned char *)duk_require_buffer_data(ctx, 1, &buf_len);

    int offset = duk_require_int(ctx, 2);
    int length = duk_require_int(ctx, 3);
    int position = duk_get_int_default(ctx, 4, -1);
    if(offset < 0 || length < 0 || offset + length > buf_len)
        duk_range_error(ctx, "offset/length outside of buffer");

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    LowFD *file = iter->second;

    file->Read(position, buf + offset, length, 5);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_write
// -----------------------------------------------------------------------------

duk_ret_t low_fs_write(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    duk_size_t buf_len;
    unsigned char *buf =
      (unsigned char *)duk_require_buffer_data(ctx, 1, &buf_len);

    int offset = duk_require_int(ctx, 2);
    int length = duk_require_int(ctx, 3);
    int position = duk_get_int_default(ctx, 4, -1);
    if(offset < 0 || length < 0 || offset + length > buf_len)
        duk_range_error(ctx, "offset/length outside of buffer");

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    LowFD *file = iter->second;

    file->Write(position, buf + offset, length, 5);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_fstat
// -----------------------------------------------------------------------------

duk_ret_t low_fs_fstat(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    if(iter->second->FDType() != LOWFD_TYPE_FILE)
        duk_reference_error(ctx, "file descriptor is not a file");
    LowFile *file = (LowFile *)iter->second;

    file->FStat(1);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_waitdone
// -----------------------------------------------------------------------------

duk_ret_t low_fs_waitdone(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        return 0;

    if(iter->second->FDType() != LOWFD_TYPE_FILE)
        duk_reference_error(ctx, "file descriptor is not a file");
    LowFile *file = (LowFile *)iter->second;

    while(true)
    {
        low_loop_clear_callback(low, file);
        if(file->FinishPhase())
            return 0;

        pthread_mutex_lock(&low->loop_thread_mutex);
        while(!file->LowLoopCallback::mNext && low->loop_callback_last != file)
            low_loop_wait(low->duk_ctx, -1);
        pthread_mutex_unlock(&low->loop_thread_mutex);
    }

    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_file_pos
// -----------------------------------------------------------------------------

duk_ret_t low_fs_file_pos(duk_context *ctx)
{
    low_t *low = duk_get_low_context(ctx);
    int fd = duk_require_int(ctx, 0);

    auto iter = low->fds.find(fd);
    if(iter == low->fds.end())
        duk_reference_error(ctx, "file descriptor not found");
    if(iter->second->FDType() != LOWFD_TYPE_FILE)
        duk_reference_error(ctx, "file descriptor is not a file");
    LowFile *file = (LowFile *)iter->second;

    duk_push_int(ctx, lseek(file->FD(), 0, SEEK_CUR));
    return 1;
}

// -----------------------------------------------------------------------------
//  low_fs_resolve
// -----------------------------------------------------------------------------

bool low_fs_resolve(char *res,
                    int res_len,
                    const char *base,
                    const char *add,
                    const char *add_node_modules_at
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
                    , bool add_esp_base
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                    )
{
    char *start, *path;

#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
    if(add_esp_base)
    {
        strcpy(res, "/fs/user/");
        start = res + 8;
        path = res + 9;
    }
    else
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
    {
        res[0] = '/';
        start = res;
        path = res + 1;
    }

    if(add[0] != '/')
    {
        for(const char *str = base; *str && str != add_node_modules_at; str++)
        {
            if(path != start)
            {
                if(path[-1] == '/' && str[0] == '/')
                    continue;
                else if(path[-1] == '/' && str[0] == '.' &&
                        (!str[1] || str[1] == '/'))
                {
                    str++;
                    if(!*str)
                        break;
                    continue;
                }
                else if(path[-1] == '/' && str[0] == '.' && str[1] == '.' &&
                        (!str[2] || str[2] == '/') &&
                        !(path - start > 2 && path[-2] == '.' &&
                          path[-3] == '.' &&
                          (path - start == 3 || path[-4] == '/')))
                {
                    path--;
                    while(path != start && path[-1] != '/')
                        path--;
                    if(path == start)
                        return false;

                    str += 2;
                    if(!*str)
                        break;
                }
                else
                    *path++ = *str;
            }
            else
                *path++ = *str;

            if(path - res == res_len)
                return false;
        }
        while(path != start && path[-1] != '/')
            path--;
        if(path == start)
            return false;
    }
    if(add_node_modules_at)
    {
        if(path + 13 - res >= res_len)
            return false;

        memcpy(path, "node_modules/", 13);
        path += 13;
    }

    for(const char *str = add; *str; str++)
    {
        if(path != start)
        {
            if(path[-1] == '/' && str[0] == '/')
                continue;
            else if(path[-1] == '/' && str[0] == '.' &&
                    (!str[1] || str[1] == '/'))
            {
                str++;
                if(!*str)
                    break;
                continue;
            }
            else if(path[-1] == '/' && str[0] == '.' && str[1] == '.' &&
                    (!str[2] || str[2] == '/') &&
                    !(path - start > 2 && path[-2] == '.' && path[-3] == '.' &&
                      (path - start == 3 || path[-4] == '/')))
            {
                path--;
                while(path != start && path[-1] != '/')
                    path--;
                if(path == start)
                    return false;
                str += 2;
                if(!*str)
                    break;
            }
            else
                *path++ = *str;
        }
        else
            *path++ = *str;

        if(path - res == res_len)
            return false;
    }

    *path = '\0';
    return true;
}
