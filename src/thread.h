#ifndef KERNEL_THREAD_H
#define KERNEL_THREAD_H

#include "types.h"

#define THREAD_DEFAULT_PRI  8

#define THREAD_STATE_RUNNING    0
#define THREAD_STATE_SLEEPING   1

#define THREAD_FLAG_SLEEP_TIMER (1<<0)

struct switch_context_t;

struct thread_t {
    struct thread_t* next;
    struct thread_t* prev;
    struct thread_t* next_wait;
    struct switch_context_t* ctx;
    uintptr_t stack;
    uint64_t ticks;
    uint32_t id;
    uint32_t cpu_id;
    uint32_t state;
    uint32_t sleep_time;
    uint32_t flags;
    int pri;
    int cnt;
};

struct thread_list_t {
    struct thread_t* head;
    struct thread_t* tail;
};

static inline void thread_list_init(struct thread_list_t* tl)
{
    tl->head = NULL;
    tl->tail = NULL;
}

static inline void thread_list_push(struct thread_list_t* tl, struct thread_t* t)
{
    if (!tl->head) {
        tl->head = t;
        tl->tail = t;
    } else {
        tl->tail->next_wait = t;
        tl->tail = t;
    }
    t->next_wait = NULL;
}

static inline struct thread_t* thread_list_pop(struct thread_list_t* tl)
{
    struct thread_t* t = tl->head;
    if (tl->head)
        tl->head = tl->head->next_wait;

    return t;
}

static inline bool thread_list_empty(struct thread_list_t* tl)
{
    return tl->head == NULL;
}

typedef void (*thread_entry_fn)(void);

void thread_init(void);
struct thread_t* thread_create(thread_entry_fn entry_fn, 
                               uint32_t stack_size,
                               uint32_t cpu_id);
void thread_wakeup(struct thread_t* thread);

#endif // KERNEL_THREAD_H
