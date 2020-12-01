// -----------------------------------------------------------------------------
//  low_alloc.h
// -----------------------------------------------------------------------------

#ifndef __LOW_ALLOC_H__
#define __LOW_ALLOC_H__

#include "low_config.h"

#include <duktape.h>
#include <stddef.h>

void *low_duk_alloc(void *udata, duk_size_t size);
void *low_duk_realloc(void *udata, void *ptr, duk_size_t size);
void low_duk_free(void *udata, void *ptr);

#if LOW_USE_SYSTEM_ALLOC
#include <cstdlib>

#define low_alloc malloc
#define low_calloc calloc
#define low_realloc realloc
#define low_free free

extern "C"
{
    char *low_strdup(const char *str);
    char *low_strcat(const char *str1, const char *str2);

    void *low_alloc_throw(duk_context *ctx, size_t size);
}
#else
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
    void *low_alloc(size_t size);
    void *low_calloc(size_t num, size_t size);
    void *low_realloc(void *ptr, size_t size);
    void low_free(void *ptr);

    char *low_strdup(const char *str);
    char *low_strcat(const char *str1, const char *str2);

    void *low_alloc_throw(duk_context *ctx, size_t size);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* LOW_USE_SYSTEM_ALLOC */

#ifdef __cplusplus
#include <memory>

void *operator new(size_t size, duk_context *ctx);
void *operator new[](size_t size, duk_context *ctx);

#endif /* __cplusplus */

#endif /* __LOW_ALLOC_H__ */
