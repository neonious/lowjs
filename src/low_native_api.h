
// -----------------------------------------------------------------------------
//  low_native_api.h
// -----------------------------------------------------------------------------

#ifndef __LOW_NATIVE_API_H__
#define __LOW_NATIVE_API_H__

#include "low_main.h"

bool low_register_native_api(low_main_t *low);

int native_api_load(duk_context *ctx);
int native_api_load_sync(duk_context *ctx);

#endif /* __LOW_NATIVE_API_H__ */
