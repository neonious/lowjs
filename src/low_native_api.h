
// -----------------------------------------------------------------------------
//  low_native_api.h
// -----------------------------------------------------------------------------

#ifndef __LOW_NATIVE_API_H__
#define __LOW_NATIVE_API_H__

#include <duktape.h>

void *native_api_load(const char *data, unsigned int size, const char **err, bool *err_malloc);
int native_api_call(duk_context *ctx);

void native_api_unload_all();

#endif /* __LOW_NATIVE_API_H__ */
