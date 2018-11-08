
// -----------------------------------------------------------------------------
//  native_adder.h
// -----------------------------------------------------------------------------

#ifndef __NATIVE_ADDER_H__
#define __NATIVE_ADDER_H__

#include "duktape.h"
#include "low.h"

bool register_native_adder(low_t *low);

int native_adder_method_add(duk_context *ctx);

#endif /* __NATIVE_ADDER_H__ */
