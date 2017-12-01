#ifndef KERNEL_COND_H
#define KERNEL_COND_H

#include "thread.h"
#include "spinlock.h"

struct condition_t {
    struct thread_list_t threads;
};

void cond_init(struct condition_t* c);
void cond_wait(struct condition_t* c, struct spinlock_t* lock);
void cond_signal(struct condition_t* c);
void cond_broadcast(struct condition_t* c);

#endif // KERNEL_COND_H
