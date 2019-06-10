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

    fl->Rename(old_name, new_name);
    fl->Run(2);
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

    fl->Unlink(file_name);
    fl->Run(1);
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

    fl->Stat(file_name);
    fl->Run(1);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_access
// -----------------------------------------------------------------------------

duk_ret_t low_fs_access(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    int mode, callIndex;
    if(duk_is_undefined(ctx, 2))
    {
        mode = F_OK;
        callIndex = 1;
    }
    else
    {
        mode = duk_require_int(ctx, 1);
        callIndex = 2;
    }

    fl->Access(file_name, mode);
    fl->Run(callIndex);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_readdir
// -----------------------------------------------------------------------------

duk_ret_t low_fs_readdir(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    bool withFileTypes = false;
    int callIndex;

    if(duk_is_undefined(ctx, 2))
        callIndex = 1;
    else if(duk_is_object(ctx, 1))
    {
        withFileTypes = duk_get_prop_string(ctx, 1, "withFileTypes") && duk_require_boolean(ctx, -1);
        callIndex = 2;
    }
    else
        callIndex = 2;

    fl->ReadDir(file_name, withFileTypes);
    fl->Run(callIndex);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_mkdir
// -----------------------------------------------------------------------------

duk_ret_t low_fs_mkdir(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    bool recursive = false;
    int mode = 0777;
    int callIndex;

    if(duk_is_undefined(ctx, 2))
        callIndex = 1;
    else if(duk_is_object(ctx, 1))
    {
        recursive = duk_get_prop_string(ctx, 1, "recursive") && duk_require_boolean(ctx, -1);
        if(duk_get_prop_string(ctx, 1, "mode"))
            mode = duk_require_int(ctx, -1);
        callIndex = 2;
    }
    else
        callIndex = 2;

    fl->MkDir(file_name, recursive, mode);
    fl->Run(callIndex);
    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_rmdir
// -----------------------------------------------------------------------------

duk_ret_t low_fs_rmdir(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    int callIndex;
    if(duk_is_undefined(ctx, 2))
        callIndex = 1;
    else
        callIndex = 2;

    fl->RmDir(file_name);
    fl->Run(callIndex);
    delete fl;

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

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Rename(old_name, new_name);
    fl->Run();
    delete fl;

    return 0;
}

// -----------------------------------------------------------------------------
//  low_fs_unlink_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_unlink_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Unlink(file_name);
    fl->Run();
    delete fl;

    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_stat_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_stat_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->Stat(file_name);
    fl->Run();
    delete fl;

    return 1;
}


// -----------------------------------------------------------------------------
//  low_fs_access_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_access_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    int mode;
    if(duk_is_undefined(ctx, 2))
        mode = F_OK;
    else
        mode = duk_require_int(ctx, 1);

    fl->Access(file_name, mode);
    fl->Run();
    delete fl;

    return 1;
}


// -----------------------------------------------------------------------------
//  low_fs_readdir_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_readdir_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    bool withFileTypes;
    int callIndex;

    if(!duk_is_undefined(ctx, 2))
        withFileTypes = false;
    else if(duk_is_object(ctx, 1))
        withFileTypes = duk_get_prop_string(ctx, 1, "withFileTypes") && duk_require_boolean(ctx, -1);

    fl->ReadDir(file_name, withFileTypes);
    fl->Run();
    delete fl;

    return 1;
}


// -----------------------------------------------------------------------------
//  low_fs_mkdir_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_mkdir_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    bool recursive = false;
    int mode = 0777;

    if(duk_is_undefined(ctx, 2))
        ;
    else if(duk_is_object(ctx, 1))
    {
        recursive = duk_get_prop_string(ctx, 1, "recursive") && duk_require_boolean(ctx, -1);
        if(duk_get_prop_string(ctx, 1, "mode"))
            mode = duk_require_int(ctx, -1);
    }

    fl->MkDir(file_name, recursive, mode);
    fl->Run();
    delete fl;

    return 0;
}


// -----------------------------------------------------------------------------
//  low_fs_rmdir_sync
// -----------------------------------------------------------------------------

duk_ret_t low_fs_rmdir_sync(duk_context *ctx)
{
    low_main_t *low = duk_get_low_context(ctx);
    const char *file_name = duk_require_string(ctx, 0);

    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->RmDir(file_name);
    fl->Run();
    delete fl;

    return 0;
}