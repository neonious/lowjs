// -----------------------------------------------------------------------------
//  low_module.h
// -----------------------------------------------------------------------------

#ifndef __LOW_MODULE_H__
#define __LOW_MODULE_H__

#include "duktape.h"

struct low_main_t;

#define LOW_MODULE_FLAG_GLOBAL      1
#define LOW_MODULE_FLAG_JSON        2
#define LOW_MODULE_FLAG_DUK_FORMAT  4
#define LOW_MODULE_FLAG_NATIVE      8

void low_module_init(duk_context *ctx);

extern "C" bool low_module_main(low_main_t *low, const char *path);

bool low_module_make_native(low_main_t *low,
                            const char *name,
                            void (*setup_cb)(low_main_t *main, void *data),
                            void *setup_cb_data);

duk_ret_t low_module_require(duk_context *ctx);
duk_ret_t low_module_resolve(duk_context *ctx);
duk_ret_t low_module_make(duk_context *ctx);

void low_load_module(duk_context *ctx, const char *path, bool parent_on_stack);
bool low_module_resolve_c(duk_context *ctx,
                          const char *module_id,
                          const char *parent_id,
                          char *res_id);

#endif /* __LOW_MODULE_H__ */