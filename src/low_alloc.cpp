// -----------------------------------------------------------------------------
//  low_alloc.cpp
// -----------------------------------------------------------------------------

#include "low_alloc.h"

#include "low_main.h"
#include "low_system.h"

#include <errno.h>

#include <cstdlib>
#include <cstring>
#include <unistd.h>


#if !LOW_ESP32_LWIP_SPECIALITIES

// -----------------------------------------------------------------------------
//  low_duk_alloc
// -----------------------------------------------------------------------------

void *low_duk_alloc(void *udata, duk_size_t size)
{
    low_t *low = (low_t *)udata;
    if(size > 0xFFFFFFF0 - low->heap_size)
        return NULL;

    if(!low->in_gc && low->heap_size + size > low->max_heap_size)
    {
        low->in_gc = true;
        duk_gc(low->duk_ctx, 0);
        low->in_gc = false;

        if(low->heap_size + size > low->max_heap_size)
        {
            fprintf(stderr, "Reached memory limit of %d bytes, aborting.\n", low->max_heap_size);
            _exit(1);
        }
    }

    unsigned int *ptr = (unsigned int *)low_alloc(size + 4);
    if(!ptr)
        return NULL;

    low->heap_size += size + 4;
    *ptr = (unsigned int)size;
    return (void *)(ptr + 1);
}


// -----------------------------------------------------------------------------
//  low_duk_realloc
// -----------------------------------------------------------------------------

void *low_duk_realloc(void *udata, void *data, duk_size_t size)
{
    if(!data)
        return low_duk_alloc(udata, size);
    
    unsigned int *ptr = ((unsigned int *)data) - 1;
    size_t old_size = (size_t)*ptr;
    low_t *low = (low_t *)udata;

    if(size > 0xFFFFFFF0 - (low->heap_size - old_size))
        return NULL;

    if(!low->in_gc && low->heap_size - old_size + size > low->max_heap_size)
    {
        low->in_gc = true;
        duk_gc(low->duk_ctx, 0);
        low->in_gc = false;

        if(low->heap_size - old_size + size > low->max_heap_size)
        {
            fprintf(stderr, "Reached memory limit of %d bytes, aborting.\n", low->max_heap_size);
            _exit(1);
        }
    }

    ptr = (unsigned int *)low_realloc(ptr, size + 4);
    if(!ptr)
        return NULL;

    low->heap_size += size - old_size;
    *ptr = (unsigned int)size;
    return (void *)(ptr + 1);
}


// -----------------------------------------------------------------------------
//  low_duk_free
// -----------------------------------------------------------------------------

void low_duk_free(void *udata, void *data)
{
    if(!data)
        return;

    low_t *low = (low_t *)udata;
    unsigned int *ptr = ((unsigned int *)data) - 1;

    low->heap_size -= ((size_t)*ptr) + 4;
    low_free(ptr);
}

#endif /* !LOW_ESP32_LWIP_SPECIALITIES */


// -----------------------------------------------------------------------------
//  low_alloc_throw
// -----------------------------------------------------------------------------

void *low_alloc_throw(duk_context *ctx, size_t size)
{
    void *data = low_alloc(size);
    if(!data)
    {
        low_push_error(ctx, ENOMEM, "malloc");
        duk_throw(ctx);
    }
    return data;
}


// -----------------------------------------------------------------------------
//  low_strdup
// -----------------------------------------------------------------------------

char *low_strdup(const char *str)
{
    int len = str ? strlen(str) : 0;
    char *dup = (char *)low_alloc(len + 1);

    if(dup)
    {
        if(len)
            memcpy(dup, str, len + 1);
        else
            *dup = '\0';
    }

    return dup;
}


// -----------------------------------------------------------------------------
//  low_strcat
// -----------------------------------------------------------------------------

char *low_strcat(const char *str1, const char *str2)
{
    int len1 = str1 ? strlen(str1) : 0;
    int len2 = str2 ? strlen(str2) : 0;
    char *dup = (char *)low_alloc(len1 + len2 + 1);

    if(dup)
    {
        if(len1)
            memcpy(dup, str1, len1);
        if(len2)
            memcpy(dup + len1, str2, len2);
        dup[len1 + len2] = '\0';
    }

    return dup;
}


// -----------------------------------------------------------------------------
//  operator new / operator delete
// -----------------------------------------------------------------------------

void *operator new(size_t size, duk_context *ctx)    { return low_alloc_throw(ctx, size); }
void *operator new[](size_t size, duk_context *ctx)  { return low_alloc_throw(ctx, size); }
