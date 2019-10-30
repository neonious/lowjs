// -----------------------------------------------------------------------------
//  transpile.h
// -----------------------------------------------------------------------------

#ifndef __TRANSPILE_H__
#define __TRANSPILE_H__

#include <stdbool.h>

bool init_transpile();

bool transpile(const char *in_data, int in_len,
               char **out_data, int *out_len,
               const char **err, bool *err_malloc);

#endif /* __TRANSPILE_H__ */