#include "thread.h"
#include "kernel.h"
#include "cpu.h"
#include "list.h"
#include "stdio.h"
#include "kmalloc.h"

struct switch_context_t {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rip;
};

static struct slab_list_t* thread_sl;

extern void _isr_ret(void);

static void thread_start()
{
    cpu_unlock(get_cpu());
}

void thread_init()
{
    thread_sl = kmalloc_get_slab(sizeof(struct thread_t));
}

static struct thread_t* thread_alloc()
{
    struct thread_t* t = (struct thread_t*)slab_list_alloc(thread_sl);
    return t;
}

struct thread_t* thread_create(thread_entry_fn entry_fn,
                               uint32_t stack_size,
                               uint32_t cpu_id)
{
    // FIXME:
    stack_size = KMALLOC_CHUNK_SIZE;

    struct thread_t* thread = thread_alloc();
    //uintptr_t stack = kernel_slack_alloc(stack_size, stack_size);
    uintptr_t stack = (uintptr_t)kmalloc_alloc();
    uintptr_t stack_top = stack + stack_size;

    //printf("thread_create(): %016lx\n", stack);

    uint8_t* sp = (uint8_t*)stack_top;
    sp -= sizeof(struct isr_frame_t);
    struct isr_frame_t* frame = (struct isr_frame_t*)sp;

    frame->r11 = 0;
    frame->r10 = 0;
    frame->r9 = 0;
    frame->r8 = 0;
    frame->rdx = 0;
    frame->rcx = 0;
    frame->rax = 0;
    frame->trap_num = 0;
    frame->trap_err = 0;

    frame->rip = (uintptr_t)entry_fn;
    frame->cs = 0x08;
    frame->rflags = RFLAGS_IF;
    frame->rsp = (uintptr_t)stack_top;
    frame->ss = 0x10;

    sp -= sizeof(uintptr_t);
    *(uintptr_t*)sp = (uintptr_t)_isr_ret;

    sp -= sizeof(struct switch_context_t);
    thread->ctx = (struct switch_context_t*)sp;

    thread->ctx->r15 = 0;
    thread->ctx->r14 = 0;
    thread->ctx->r13 = 0;
    thread->ctx->r12 = 0;
    thread->ctx->rsi = 0;
    thread->ctx->rdi = 0;
    thread->ctx->rbp = 0;
    thread->ctx->rbx = 0;
    thread->ctx->rip = (uintptr_t)thread_start;

    thread->next = NULL;
    thread->prev = NULL;
    thread->next_wait = NULL;
    thread->stack = stack_top;
    thread->ticks = 0;
    thread->state = THREAD_STATE_RUNNING;
    thread->sleep_time = 0;
    thread->flags = 0;
    thread->pri = THREAD_DEFAULT_PRI;
    thread->cnt = THREAD_DEFAULT_PRI;

    uint32_t this_cpu_id = get_cpu_id();
    struct cpu_desc_t* cpu = this_cpu_id == cpu_id 
                           ? cpu_lock_splhi()
                           : cpu_lock_id(cpu_id);

    uint32_t id = ++cpu->id_cnt;
    thread->id = id;
    thread->cpu_id = cpu->apic_id;
    queue_push_back(cpu->threads, thread, next, prev);

    if (this_cpu_id == cpu_id)
        cpu_unlock_splx(cpu);
    else
        cpu_unlock(cpu);

    return thread;
}

void thread_wakeup(struct thread_t* thread)
{
    const bool this_cpu = get_cpu_id() == thread->cpu_id;
    struct cpu_desc_t* cpu = this_cpu
                           ? cpu_lock_splhi()
                           : cpu_lock_id(thread->cpu_id);

    thread->state = THREAD_STATE_RUNNING;

    if (this_cpu)
        cpu_unlock_splx(cpu);
    else
        cpu_unlock(cpu);
}
