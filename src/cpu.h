#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

#include "io.h"
#include "spinlock.h"
#include "thread.h"

struct isr_frame_t {
    uint64_t r11, r10, r9, r8;
    //uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t rdx, rcx, rax;
    uint64_t trap_num, trap_err;
    uint64_t rip, cs, rflags, rsp, ss;
};

static inline struct cpu_desc_t* get_cpu()
{
    uintptr_t p;
    asm volatile(
        "movq %%gs:0, %0"
        : "=r"(p)
        : // no input
        : "memory"
    );
    return (struct cpu_desc_t*)p;
}

#define CPU_FLAGS_ACTIVE    0x01
#define CPU_FLAGS_BSP       0x80

struct cpu_desc_t {
    struct cpu_desc_t* self;
    struct thread_t* threads;
    struct thread_t* cur_thread;
    struct thread_t idle_thread;
    volatile uint64_t ticks;
    struct spinlock_t lock;
    uint32_t apic_id;
    uint32_t flags;
    uint32_t id_cnt;
    int spl;
};

#define MAX_CPUS 16
extern struct cpu_desc_t cpus[MAX_CPUS];
extern uint32_t num_cpus;

static inline uint32_t get_cpu_id()
{
    struct cpu_desc_t* cpu = get_cpu();
    return cpu->apic_id;
}

static inline struct cpu_desc_t* cpu_lock_splhi()
{
    int spl = cpu_splhi();
    struct cpu_desc_t* cpu = get_cpu();
    spinlock_lock(&cpu->lock);
    cpu->spl = spl;
    return cpu;
}

static void inline cpu_unlock_splx(struct cpu_desc_t* cpu)
{
    spinlock_unlock(&cpu->lock);
    cpu_splx(cpu->spl);
}

static inline struct cpu_desc_t* cpu_lock()
{
    struct cpu_desc_t* cpu = get_cpu();
    spinlock_lock(&cpu->lock);
    return cpu;
}

static inline struct cpu_desc_t* cpu_lock_id(int id)
{
    struct cpu_desc_t* cpu = &cpus[id];
    spinlock_lock(&cpu->lock);
    return cpu;
}

static void inline cpu_unlock(struct cpu_desc_t* cpu)
{
    spinlock_unlock(&cpu->lock);
}

static struct cpu_desc_t* cpu_lock_smp(uint32_t id)
{
    return get_cpu_id() == id
        ? cpu_lock_splhi()
        : cpu_lock_id(id);
}

static void cpu_unlock_smp(struct cpu_desc_t* cpu)
{
    if (cpu == get_cpu())
        cpu_unlock_splx(cpu);
    else
        cpu_unlock(cpu);
}

typedef void (*irq_handler_fn)(void);

void cpu_boot_init(void);
void cpu_init(void);
void cpu_interrupt_set(uint8_t vector, irq_handler_fn handler);
void cpu_interrupt_unset(uint8_t vector);
void cpu_wait(uint32_t ms);
void cpu_show_cmd(int argc, const char* argv[]);

#endif // KERNEL_CPU_H
