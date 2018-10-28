// -----------------------------------------------------------------------------
//  low_fs_misc.cpp
// -----------------------------------------------------------------------------

#include "low_fs_misc.h"
#include "LowFSMisc.h"

#include "low_alloc.h"
#include "low_config.h"
#include "low_main.h"
#include "low_system.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
//  low_fs_rename
// -----------------------------------------------------------------------------

duk_ret_t low_fs_rename(duk_context *ctx)
{
     low_main_t *low = low_duk_get_low(ctx);
     const char *old_name = duk_require_string(ctx, 0);
     const char *new_name = duk_require_string(ctx, 1);
   
    LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");
    //duk_push_context_dump(ctx);
    //printf("%s\n", duk_safe_to_string(ctx, -1));
    //duk_pop(ctx);
    fl->ReName(old_name, new_name, 2);
    return 0; 
}

// -----------------------------------------------------------------------------
//  low_fs_unlink
// -----------------------------------------------------------------------------

duk_ret_t low_fs_unlink(duk_context *ctx)
{
     low_main_t *low = low_duk_get_low(ctx);
     const char *file_name = duk_require_string(ctx, 0);
   
   LowFSMisc *fl = new(low_new) LowFSMisc(low);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    fl->UnLink(file_name,  1);
    return 0; 
}