// -----------------------------------------------------------------------------
//  low_fs_misc.cpp
// -----------------------------------------------------------------------------

#include "low_fs_misc.h"
#include "LowNFD.h"
#include "LowNFD.cpp"

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

    
   LowNFD *fl =
      new(low_new) LowNFD(low, old_name, new_name, duk_is_undefined(ctx, 2) ? 1 : 2);
    if(!fl)
        duk_generic_error(ctx, "out of memory");

    return 0;
    
}