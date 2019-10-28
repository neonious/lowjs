// -----------------------------------------------------------------------------
//  low.h
// -----------------------------------------------------------------------------

#ifndef __LOW_H__
#define __LOW_H__

#include "duktape.h"

#include <stdbool.h>

typedef void *low_t;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    bool low_system_init();
    void low_system_destroy();

    low_t *low_init();
    bool low_lib_init(low_t *low);
    void low_destroy(low_t *low);

    bool low_module_main(low_t *low, const char *path);
    bool low_loop_run(low_t *low);

    duk_context *low_get_duk_context(low_t *low);
    low_t *duk_get_low_context(duk_context *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LOW_H__ */