// -----------------------------------------------------------------------------
//  low_alloc.cpp
// -----------------------------------------------------------------------------

#include "low_alloc.h"

#include <cstdlib>
#include <cstring>


// -----------------------------------------------------------------------------
//  operator new
// -----------------------------------------------------------------------------

void *operator new(size_t size, low_new_ident ident) noexcept
{
    return low_alloc(size);
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