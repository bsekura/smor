#include "sched.h"
#include "cpu.h"
#include "thread.h"
#include "list.h"
#include "stdio.h"

void sched_init()
{
    thread_init();
}

void sched_dump()
{
    struct cpu_desc_t* cpu = cpu_lock_splhi();

    struct thread_t* threads = cpu->threads;
    struct thread_t* t = threads;
    do {
        printf("[%d] %ld\n", t->id, t->ticks);
        t = t->next;
    } while (t != threads);
    
    cpu_unlock_splx(cpu);
}

static struct thread_t* setup_idle_thread(struct cpu_desc_t* cpu)
{
    struct thread_t* thread = &cpu->idle_thread;

    thread->next = NULL;
    thread->prev = NULL;
    thread->next_wait = NULL;
    thread->ctx = NULL;
    thread->stack = 0;
    thread->ticks = 0;
    thread->id = 0;
    thread->cpu_id = cpu->apic_id;
    thread->state = THREAD_STATE_RUNNING;
    thread->sleep_time = 0;
    thread->flags = 0;    
    thread->pri = THREAD_DEFAULT_PRI;
    thread->cnt = THREAD_DEFAULT_PRI;

    return thread;
}

void sched_init_cpu(struct cpu_desc_t* cpu)
{
    struct thread_t* t = setup_idle_thread(cpu);
    queue_push_back(cpu->threads, t, next, prev);
    cpu->cur_thread = t;
}

extern void context_switch(struct switch_context_t** old_ctx,
                           struct switch_context_t* new_ctx);

// TODO: put sleeping threads on a separate list
static int sched_check_sleeping(struct thread_t* begin)
{
    int num_wakeups = 0;
    struct thread_t* t = begin;
    do {
        if (t->state == THREAD_STATE_SLEEPING && t->flags & THREAD_FLAG_SLEEP_TIMER) {
            t->sleep_time--;
            if (!t->sleep_time) {
                t->state = THREAD_STATE_RUNNING;
                t->flags &= ~THREAD_FLAG_SLEEP_TIMER;
                t->cnt = t->pri;
                ++num_wakeups;
            }
        }
        t = t->next;
    } while (t != begin);

    return num_wakeups;
}

// find next one to run starting at begin
static struct thread_t* sched_find(struct thread_t* begin)
{
    int cnt = -1;
    struct thread_t* t = begin;
    struct thread_t* next_thread = t;
    do {
        if (t->state == THREAD_STATE_RUNNING && t->cnt > cnt) {
            cnt = t->cnt;
            next_thread = t;
        }
        t = t->next;
    } while (t != begin);

    return next_thread;
}

static void sched_next(struct cpu_desc_t* cpu)
{
    struct thread_t* cur_thread = cpu->cur_thread;

    struct thread_t* threads = cpu->threads;
    struct thread_t* next_thread = sched_find(threads->next);
    if (next_thread->cnt == 0) {
        for (struct thread_t* t = threads->next; t != threads; t = t->next) {
            t->cnt = (t->cnt >> 1) + t->pri;
        }
        next_thread = sched_find(threads->next);
    }

    if (next_thread != cur_thread) {
        struct thread_t* this_thread = cur_thread;
        cpu->cur_thread = next_thread;
        this_thread->ticks++;
        context_switch(&this_thread->ctx, next_thread->ctx);
    }
}

// entered at splhi
void sched_tick(struct cpu_desc_t* cpu)
{
    struct thread_t* threads = cpu->threads;
    sched_check_sleeping(threads->next);

    struct thread_t* cur_thread = cpu->cur_thread;
    cur_thread->ticks++;

    --cur_thread->cnt;
    if (cur_thread->cnt > 0)
        return;

    cur_thread->cnt = 0;
    sched_next(cpu);

    // NOTES:
    // once we use one shot lapic timer, we can enable interrupts
    // other interrupts will most likely wake up threads
    // as long as we hold the queue locks, we're fine
    // we have to disable ints before returning (iretq)
    // we know we won't be re-entered here
    // we can run cleanup tasks (dead threads, dead processes)
}

void sched_yield_locked(struct cpu_desc_t* cpu)
{
    struct thread_t* cur_thread = cpu->cur_thread;
    cur_thread->state = THREAD_STATE_SLEEPING;
    sched_next(cpu);
}

void sched_yield()
{
    struct cpu_desc_t* cpu = cpu_lock_splhi();
    sched_yield_locked(cpu);
    cpu_unlock_splx(cpu);
}

void sched_sleep(uint32_t ms)
{
    struct cpu_desc_t* cpu = cpu_lock_splhi();
    struct thread_t* cur_thread = cpu->cur_thread;
    if (cur_thread != cpu->threads) {
        cur_thread->state = THREAD_STATE_SLEEPING;
        cur_thread->sleep_time = ms;
        cur_thread->flags |= THREAD_FLAG_SLEEP_TIMER;
        sched_next(cpu);
    }
    cpu_unlock_splx(cpu);
}