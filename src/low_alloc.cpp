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
        low_push_error(duk_get_low_context(ctx), ENOMEM, "malloc");
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