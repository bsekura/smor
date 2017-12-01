#ifndef KERNEL_SEMAPHORE_H
#define KERNEL_SEMAPHORE_H

#include "thread.h"
#include "spinlock.h"

struct semaphore_t {
    struct thread_list_t threads;
    struct spinlock_t lock;
    int count;
};

void sema_init(struct semaphore_t* s, int count);
void sema_wait(struct semaphore_t* s);
void sema_signal(struct semaphore_t* s);

#endif // KERNEL_SEMAPHORE_H
