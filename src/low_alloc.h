// -----------------------------------------------------------------------------
//  low_alloc.h
// -----------------------------------------------------------------------------

#ifndef __LOW_ALLOC_H__
#define __LOW_ALLOC_H__

#include "low_config.h"

#include <duktape.h>
#include <stddef.h>

#if LOW_USE_SYSTEM_ALLOC
#include <cstdlib>

#define low_alloc malloc
#define low_calloc calloc
#define low_realloc realloc
#define low_free free

extern "C"
{
    char *low_strdup(const char *str);
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

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* LOW_USE_SYSTEM_ALLOC */

void *low_alloc_throw(duk_context *ctx, size_t size);

#ifdef __cplusplus
#include <memory>

enum class low_new_ident
{
    low_new_ident
};
#define low_new low_new_ident::low_new_ident

void *operator new(size_t size, low_new_ident ident) noexcept;

template<typename T> class low_allocator : public std::allocator<T>
{
  public:
    template<class U> struct rebind
    {
        typedef low_allocator<U> other;
    };

    T *allocate(size_t n, const void *hint = 0)
    {
        T *items = (T *)low_alloc(n * sizeof(T));
        if(!items)
            throw std::bad_alloc();

        return items;
    }

    void deallocate(T *p, size_t n) { low_free(p); }
};
#endif /* __cplusplus */

#endif /* __LOW_ALLOC_H__ */
