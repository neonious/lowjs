// -----------------------------------------------------------------------------
//  low_process.h
// -----------------------------------------------------------------------------

#ifndef __LOW_PROCESS_H__
#define __LOW_PROCESS_H__

#include "duktape.h"

duk_ret_t low_gc(duk_context *ctx);
duk_ret_t low_hrtime(duk_context *ctx);

duk_ret_t low_process_exit(duk_context *ctx);
duk_ret_t low_process_info(duk_context *ctx);
duk_ret_t low_os_info(duk_context *ctx);

duk_ret_t low_tty_info(duk_context *ctx);

#endif /* __LOW_PROCESS_H__ */