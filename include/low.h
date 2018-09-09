// -----------------------------------------------------------------------------
//  low.h
// -----------------------------------------------------------------------------

#ifndef __LOW_H__
#define __LOW_H__

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LOW_H__ */