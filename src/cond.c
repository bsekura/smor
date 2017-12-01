#include "cond.h"
#include "sched.h"
#include "cpu.h"

void cond_init(struct condition_t* c)
{
    thread_list_init(&c->threads);
}

// NOTE: assumes "lock" is held at splhi
void cond_wait(struct condition_t* c, struct spinlock_t* lock)
{
    struct cpu_desc_t* cpu = cpu_lock();
    thread_list_push(&c->threads, cpu->cur_thread);

    spinlock_unlock(lock);
    sched_yield_locked(cpu);
    cpu_unlock(cpu);
    spinlock_lock(lock);
}

void cond_signal(struct condition_t* c)
{
    if (thread_list_empty(&c->threads))
        return;

    struct thread_t* t = thread_list_pop(&c->threads);
    struct cpu_desc_t* cpu = cpu_lock_id(t->cpu_id);
    t->state = THREAD_STATE_RUNNING;
    cpu_unlock(cpu);
}

void cond_broadcast(struct condition_t* c)
{
    // TODO
}
