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


#if LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV)

extern bool gAllocFailed;

#endif /* LOW_ESP32_LWIP_SPECIALITIES || defined(LOWJS_SERV) */
#if LOW_ESP32_LWIP_SPECIALITIES

extern "C" bool alloc_use_fund();
extern void *gRainyDayFund;

#endif /* LOW_ESP32_LWIP_SPECIALITIES */


// -----------------------------------------------------------------------------
//  low_duk_alloc
// -----------------------------------------------------------------------------

void *low_duk_alloc(void *udata, duk_size_t size)
{
    low_t *low = (low_t *)udata;

#if LOW_ESP32_LWIP_SPECIALITIES
    low->in_gc = true;
    void *ptr = low_alloc(size);
    low->in_gc = false;
    return ptr;
#else
    if(size > 0xFFFFFFF0 - low->heap_size)
        return NULL;

    if(!low->in_gc && low->heap_size + size > low->max_heap_size)
    {
        low->in_gc = true;
        duk_gc(low->duk_ctx, 0);
        low->in_gc = false;

#ifdef LOWJS_SERV
        if(low->heap_size + size + 4 > low->max_heap_size)
        {
            gAllocFailed = true;
            low->duk_flag_stop = 1;
        }
#else
        if(low->heap_size + size + 4 > low->max_heap_size)
        {
            fprintf(stderr, "Reached memory limit of %d MB, aborting. If the device has more memory, configure the low.js memory limit with --max-old-space-size.\n", (int)(low->max_heap_size / (1024 * 1024)));
            _exit(1);
        }
#endif /* LOWJS_SERV */
    }

    unsigned int *ptr = (unsigned int *)low_alloc(size + 4);
    if(!ptr)
        return NULL;

    low->heap_size += size + 4;
    *ptr = (unsigned int)size;
    return (void *)(ptr + 1);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}


// -----------------------------------------------------------------------------
//  low_duk_realloc
// -----------------------------------------------------------------------------

void *low_duk_realloc(void *udata, void *data, duk_size_t size)
{
    low_t *low = (low_t *)udata;

#if LOW_ESP32_LWIP_SPECIALITIES
    low->in_gc = true;
    void *ptr = low_realloc(data, size);
    low->in_gc = false;
    return ptr;
#else
    if(!data)
        return low_duk_alloc(udata, size);
    
    unsigned int *ptr = ((unsigned int *)data) - 1;
    size_t old_size = (size_t)*ptr;

    if(size > 0xFFFFFFF0 - (low->heap_size - old_size))
        return NULL;

    if(!low->in_gc && low->heap_size - old_size + size > low->max_heap_size)
    {
        low->in_gc = true;
        duk_gc(low->duk_ctx, 0);
        low->in_gc = false;

#ifdef LOWJS_SERV
        if(low->heap_size - old_size + size > low->max_heap_size)
        {
            gAllocFailed = true;
            low->duk_flag_stop = 1;
        }
#else
        if(low->heap_size - old_size + size > low->max_heap_size)
        {
            fprintf(stderr, "Reached memory limit of %d MB, aborting.\n", (int)(low->max_heap_size / (1024 * 1024)));
            _exit(1);
        }
#endif /* LOWJS_SERV */
    }

    ptr = (unsigned int *)low_realloc(ptr, size + 4);
    if(!ptr)
        return NULL;

    low->heap_size += size - old_size;
    *ptr = (unsigned int)size;
    return (void *)(ptr + 1);
#endif /* LOW_ESP32_LWIP_SPECIALITIES */
}


// -----------------------------------------------------------------------------
//  low_duk_free
// -----------------------------------------------------------------------------

void low_duk_free(void *udata, void *data)
{
#if LOW_ESP32_LWIP_SPECIALITIES
    low_free(data);
#else
    if(!data)
        return;

    low_t *low = (low_t *)udata;
    unsigned int *ptr = ((unsigned int *)data) - 1;

    low->heap_size -= ((size_t)*ptr) + 4;
    low_free(ptr);
#endif /* !LOW_ESP32_LWIP_SPECIALITIES */
}



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
