#ifndef KERNEL_X86_H
#define KERNEL_X86_H

#include "types.h"

#define RFLAGS_TF   (1<<8)  // trap flag
#define RFLAGS_IF   (1<<9)  // interrupt enable flag

static inline uint64_t get_rflags()
{
    uint64_t rflags;
    asm volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        : "=r" (rflags)
        :
    );
    return rflags;
}

static inline uint64_t get_cr3()
{
    uint64_t cr3;
    asm volatile(
        "movq %%cr3, %0\n\t"
        : "=r" (cr3)
        :
    );
    return cr3;
}

static inline uint64_t get_cr2()
{
    uint64_t cr2;
    asm volatile(
        "movq %%cr2, %0\n\t"
        : "=r" (cr2)
        :
    );
    return cr2;
}

#define MSR_FS_BASE         0xc0000100      // 64-bit FS base
#define MSR_GS_BASE         0xc0000101      // 64-bit GS base
#define MSR_KERNEL_GS_BASE  0xc0000102      // swapgs

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    asm volatile(
        "rdmsr"
        : "=a"(lo), "=d"(hi)
        : "c"(msr)
    );
    return (uint64_t)lo | ((uint64_t)hi << 32);
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    asm volatile(
        "wrmsr"
        : // no outputs
        : "c"(msr), "a"(lo), "d"(hi)
    );
}

static inline void cpu_enable_interrupts()
{
    asm volatile("sti");
}

static inline void cpu_disable_interrupts()
{
    asm volatile("cli");
}

static inline void cpu_halt()
{
    asm volatile("hlt");
}

static inline int cpu_splhi()
{
    uint64_t rflags;
    asm volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli\n\t"
        : "=r" (rflags)
        :
    );
    return (rflags & RFLAGS_IF);
}

static inline void cpu_splx(int s)
{
    if (s) {
        asm volatile("sti");
    }
}

static inline uint32_t compare_and_swap_32(volatile uint32_t* ptr,
                                           uint32_t old_val,
                                           uint32_t new_val)
{
    uint32_t prev;
    asm volatile(
        "lock; cmpxchgl %2,%1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(new_val), "0"(old_val)
        : "memory"
    );
    return prev;
}

static inline uint32_t fetch_and_add_32(uint32_t* var, uint32_t value)
{
    asm volatile(
        "lock; xaddl %0,%1"
        : "+r"(value), "+m"(*var)
        : // no input only
        : "memory"
    );
    return value;
}

#endif // KERNEL_X86_H
