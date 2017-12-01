#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include "x86.h"

struct spinlock_t {
    volatile uint32_t counter;
};

static inline void spinlock_init(struct spinlock_t* lock)
{
    lock->counter = 0;
}

static inline void spinlock_lock(struct spinlock_t* lock)
{
    do {} while(compare_and_swap_32(&lock->counter, 0, 1) != 0);
}

static inline void spinlock_unlock(struct spinlock_t* lock)
{
    compare_and_swap_32(&lock->counter, 1, 0);
}

static inline int spinlock_lock_splhi(struct spinlock_t* lock)
{
    int s = cpu_splhi();
    spinlock_lock(lock);
    return s;
}

static inline void spinlock_unlock_splx(struct spinlock_t* lock, int s)
{
    spinlock_unlock(lock);
    cpu_splx(s);
}

#endif // KERNEL_SPINLOCK_H
