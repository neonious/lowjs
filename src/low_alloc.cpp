// -----------------------------------------------------------------------------
//  low_alloc.cpp
// -----------------------------------------------------------------------------

#include "low_alloc.h"

#include "low_main.h"
#include "low_system.h"

#include <errno.h>

#include <cstdlib>
#include <cstring>


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
