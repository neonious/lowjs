// -----------------------------------------------------------------------------
//  low_fs.h
// -----------------------------------------------------------------------------

#ifndef __LOW_FS_H__
#define __LOW_FS_H__

#include "duktape.h"

duk_ret_t low_fs_open(duk_context *ctx);
duk_ret_t low_fs_open_sync(duk_context *ctx);

duk_ret_t low_fs_close(duk_context *ctx);
duk_ret_t low_fs_close_sync(duk_context *ctx);

duk_ret_t low_fs_read(duk_context *ctx);
duk_ret_t low_fs_write(duk_context *ctx);
duk_ret_t low_fs_fstat(duk_context *ctx);

duk_ret_t low_fs_waitdone(duk_context *ctx);
duk_ret_t low_fs_file_pos(duk_context *ctx);

bool low_fs_resolve(char *res,
                    int res_len,
                    const char *base,
                    const char *add,
                    const char *add_node_modules_at = NULL
#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)
                    , bool add_esp_base = true
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
                    );

#endif /* __LOW_FS_H__ */