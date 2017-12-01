#include "types.h"
#include "string.h"

size_t strlen(const char* p)
{
    if (p == NULL)
        return 0;

    size_t len = 0;
    while (*p++)
        ++len;

    return len;
}

int strcmp(const char* s1, const char* s2)
{
    while (1) {
        unsigned char c1 = *s1++;
        unsigned char c2 = *s2++;
        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            break;
    }

    return 0;
}

char* strcpy(char* dest, const char* src)
{
    char* tmp = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

// non-standard helpers
const char* c_strncpy(char* dst, const char* src, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        dst[i] = src[i];
    dst[len] = 0;
    return dst;
}
