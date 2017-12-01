#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#define BIT(x)      (1<<(x))

#define PACKED      __attribute__((__packed__));

#define ALIGN_UP(addr, align) \
    (((addr) + (typeof(addr))(align) - 1) & ~((typeof(addr))(align) - 1))

#endif // KERNEL_TYPES_H
