// -----------------------------------------------------------------------------
//  low_fs_misc.cpp
// -----------------------------------------------------------------------------

#include "low_fs_misc.h"
#include "LowFSMisc.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_fs.h"
#include "low_main.h"
#include "low_system.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

// -----------------------------------------------------------------------------
//  low_fs_rename
// -----------------------------------------------------------------------------

duk_ret_t low_fs_rename(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *old_name = duk_require_string(ctx, 0);
    const char *new_name = duk_require_string(ctx, 1);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Rename(old_name, new_name, 2);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_unlink
// -----------------------------------------------------------------------------

duk_ret_t low_fs_unlink(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Unlink(file_name, 1);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_stat
// -----------------------------------------------------------------------------

duk_ret_t low_fs_stat(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Stat(file_name, 1);
    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_rename_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_rename_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *old_name = duk_require_string(ctx, 0);
    const char *new_name = duk_require_string(ctx, 1);


#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(old_name) + strlen(low->cwd);
    char *old_name2 = (char *)low_alloc(len);
    if(!old_name2)
    {
        low_push_error(low, ENOMEM, "rename");
        duk_throw(low->duk_ctx);
    }

    if(!low_fs_resolve(old_name2, len, low->cwd, old_name))
    {
        free(old_name2);
        duk_generic_error(low->duk_ctx, "fs resolve error");
    }

    len = 32 + strlen(old_name) + strlen(low->cwd);
    char *new_name2 = (char *)low_alloc(len);
    if(!new_name2)
    {
        free(old_name2);
        low_push_error(low, ENOMEM, "rename");
        duk_throw(low->duk_ctx);
    }

    if(!low_fs_resolve(new_name2, len, low->cwd, new_name))
    {
        free(old_name2);
        free(new_name2);
        duk_generic_error(low->duk_ctx, "fs resolve error");
    }

    if(rename(old_name2, new_name2) != 0)
    {
        free(old_name2);
        free(new_name2);

        low_push_error(low, errno, "rename");
        duk_throw(low->duk_ctx);
    }
    free(old_name2);
    free(new_name2);
#else
    if(rename(old_name, new_name) != 0)
    {
        low_push_error(low, errno, "rename");
        duk_throw(low->duk_ctx);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_unlink_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_unlink_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(file_name) + strlen(low->cwd);
    char *name = (char *)low_alloc(len);
    if(!name)
    {
        low_push_error(low, ENOMEM, "unlink");
        duk_throw(low->duk_ctx);
    }

    if(!low_fs_resolve(name, len, low->cwd, file_name))
    {
        free(name);
        duk_generic_error(low->duk_ctx, "fs resolve error");
    }

    if(unlink(name) != 0)
    {
        free(name);

        low_push_error(low, errno, "unlink");
        duk_throw(low->duk_ctx);
    }
    free(name);
#else
    if(unlink(file_name) != 0)
    {
        low_push_error(low, errno, "unlink");
        duk_throw(low->duk_ctx);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_stat_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_stat_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    struct stat st;
#if LOW_ESP32_LWIP_SPECIALITIES
    int len = 32 + strlen(file_name) + strlen(low->cwd);
    char *name = (char *)low_alloc(len);
    if(!name)
    {
        low_push_error(low, ENOMEM, "sync");
        duk_throw(low->duk_ctx);
    }

    if(!low_fs_resolve(name, len, low->cwd, file_name))
    {
        free(name);
        duk_generic_error(low->duk_ctx, "fs resolve error");
    }

    if(stat(name, &st) != 0)
    {
        free(name);

        low_push_error(low, errno, "stat");
        duk_throw(low->duk_ctx);
    }
    free(name);
#else
    if(stat(file_name, &st) != 0)
    {
        low_push_error(low, errno, "stat");
        duk_throw(low->duk_ctx);
    }
#endif /* LOW_ESP32_LWIP_SPECIALITIES */

    duk_push_object(low->duk_ctx);
#define applyStat(name) {#name, (double)st.st_##name}
    duk_number_list_entry numberList[] = {applyStat(dev),
                                          applyStat(ino),
                                          applyStat(mode),
                                          applyStat(nlink),
                                          applyStat(uid),
                                          applyStat(gid),
                                          applyStat(rdev),
                                          applyStat(blksize),
                                          applyStat(blocks),
                                          applyStat(size),
                                          {"atimeMs", st.st_atime * 1000.0},
                                          {"mtimeMs", st.st_mtime * 1000.0},
                                          {"ctimeMs", st.st_ctime * 1000.0},
                                          {NULL, 0.0}};
    duk_put_number_list(low->duk_ctx, -1, numberList);

    return 1;
}
