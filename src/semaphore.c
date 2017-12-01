#include "semaphore.h"
#include "sched.h"
#include "cpu.h"

void sema_init(struct semaphore_t* s, int count)
{
    thread_list_init(&s->threads);
    spinlock_init(&s->lock);
    s->count = count;
}

void sema_wait(struct semaphore_t* s)
{
    int spl = cpu_splhi();
    spinlock_lock(&s->lock);
    if (s->count > 0) {
        s->count--;
        spinlock_unlock(&s->lock);
        cpu_splx(spl);
        return;
    }

    struct cpu_desc_t* cpu = cpu_lock();
    thread_list_push(&s->threads, cpu->cur_thread);
    spinlock_unlock(&s->lock);

    sched_yield_locked(cpu);

    cpu_unlock(cpu);
    cpu_splx(spl);
}

void sema_signal(struct semaphore_t* s)
{
    int spl = cpu_splhi();
    spinlock_lock(&s->lock);
    if (thread_list_empty(&s->threads)) {
        s->count++;
        spinlock_unlock(&s->lock);
        cpu_splx(spl);
        return;
    }

    struct thread_t* t = thread_list_pop(&s->threads);
    struct cpu_desc_t* cpu = cpu_lock_id(t->cpu_id);
    t->state = THREAD_STATE_RUNNING;
    cpu_unlock(cpu);

    spinlock_unlock(&s->lock);
    cpu_splx(spl);
}
