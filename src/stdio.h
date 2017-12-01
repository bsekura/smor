#ifndef KERNEL_STDIO_H
#define KERNEL_STDIO_H

#include <stdarg.h>

int vsprintf(char* buf, const char* fmt, va_list args);
int printf(const char* fmt, ...);

#endif // KERNEL_STDIO_H
