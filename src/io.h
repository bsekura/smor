#ifndef KERNEL_IO_H
#define KERNEL_IO_H

#include "types.h"

static inline uint8_t inb(unsigned int port)
{
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outb(unsigned int port, uint8_t data)
{
    asm volatile("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

static inline uint16_t inw(unsigned int port)
{
    uint16_t data;
    asm volatile("inw %w1, %w0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outw(unsigned int port, uint16_t data)
{
    asm volatile("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

static inline uint32_t inl(unsigned int port)
{
    uint32_t data;
    asm volatile("inl %w1, %0" : "=a" (data) : "Nd" (port));
    return data;
}

static inline void outl(unsigned int port, uint32_t data)
{
    asm volatile("outl %0, %w1" : : "a" (data), "Nd" (port));
}

// memory mapped
static inline uint32_t mmio_read_u32(void* p)
{
    return *(volatile uint32_t*)(p);
}

static inline void mmio_write_u32(void* p, uint32_t data)
{
    *(volatile uint32_t*)(p) = data;
}

#endif // KERNEL_IO_H
