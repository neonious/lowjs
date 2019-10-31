// -----------------------------------------------------------------------------
//  transpile.h
// -----------------------------------------------------------------------------

#ifndef __TRANSPILE_H__
#define __TRANSPILE_H__

#include "low_main.h"

#include <duktape.h>

bool init_transpile(low_main_t *low);

int transpile(duk_context *ctx);

#endif /* __TRANSPILE_H__ */