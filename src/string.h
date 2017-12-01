#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include "types.h"

#define is_digit(c)  ((c) >= '0' && (c) <= '9')

size_t strlen(const char* p);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);

static inline void* memcpy(void* dst, const void* src, size_t size)
{
    asm volatile(
        "cld; rep movsb" 
        :: "S"(src), "D"(dst), "c"(size) 
        : "flags", "memory"
    );

    return dst;
}

// non-standard helpers
const char* c_strncpy(char* dst, const char* src, size_t len);

#endif // KERNEL_STRING_H
