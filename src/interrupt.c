#include "interrupt.h"
#include "cpu.h"
#include "io_apic.h"
#include "local_apic.h"
#include "spinlock.h"
#include "stdio.h"

#define IRQ_BASE        0x40

void cpu_interrupt(struct isr_frame_t frame);
void cpu_local_interrupt(struct isr_frame_t frame);

#define IRQ_MAX         0x16

extern void _irq_0();
extern void _irq_1();
extern void _irq_2();
extern void _irq_3();
extern void _irq_4();
extern void _irq_5();
extern void _irq_6();
extern void _irq_7();
extern void _irq_8();
extern void _irq_9();
extern void _irq_10();
extern void _irq_11();
extern void _irq_12();
extern void _irq_13();
extern void _irq_14();
extern void _irq_15();

static irq_handler_fn irq_stubs[IRQ_MAX] = {
    _irq_0,
    _irq_1,
    _irq_2,
    _irq_3,
    _irq_4,
    _irq_5,
    _irq_6,
    _irq_7,
    _irq_8,
    _irq_9,
    _irq_10,
    _irq_11,
    _irq_12,
    _irq_13,
    _irq_14,
    _irq_15
};

#define LINT_MAX    8

extern void _lint_0();
extern void _lint_1();
extern void _lint_2();
extern void _lint_3();
extern void _lint_4();
extern void _lint_5();
extern void _lint_6();
extern void _lint_7();
extern void _lint_8();

static irq_handler_fn lint_stubs[LINT_MAX] = {
    _lint_0,
    _lint_1,
    _lint_2,
    _lint_3,
    _lint_4,
    _lint_5,
    _lint_6,
    _lint_7
};

struct interrupt_t {
    interrupt_handler_fn handler;
    uint8_t vector;
    uint8_t reserved[3];
};

static struct interrupt_t irq_handlers[IRQ_MAX];
static struct interrupt_t lint_handlers[LINT_MAX];
static struct spinlock_t intr_lock;

void intr_init()
{
}

void cpu_interrupt(struct isr_frame_t frame)
{
    local_apic_eoi();
    // TODO: consider enabling irqs here
    //          and letting each handler guard its critical paths

    uint32_t irq_num = (uint32_t)frame.trap_num;
    if (irq_handlers[irq_num].handler)
        irq_handlers[irq_num].handler(&frame);
}

void cpu_local_interrupt(struct isr_frame_t frame)
{
    local_apic_eoi();
    uint32_t irq_num = (uint32_t)frame.trap_num;
    if (lint_handlers[irq_num].handler)
        lint_handlers[irq_num].handler(&frame);
}

void intr_register_irq_handler(uint8_t irq, interrupt_handler_fn handler)
{
    int spl = spinlock_lock_splhi(&intr_lock);
    struct interrupt_t* i = &irq_handlers[irq];
    i->handler = handler;
    i->vector = IRQ_BASE + irq;
    cpu_interrupt_set(i->vector, irq_stubs[irq]);
    spinlock_unlock_splx(&intr_lock, spl);
}

void intr_register_local_irq_handler(uint8_t irq, uint8_t vector, interrupt_handler_fn handler)
{
    int spl = spinlock_lock_splhi(&intr_lock);
    struct interrupt_t* i = &lint_handlers[irq];
    i->handler = handler;
    i->vector = vector;
    cpu_interrupt_set(i->vector, lint_stubs[irq]);
    spinlock_unlock_splx(&intr_lock, spl);
}

void intr_irq_enable(uint8_t irq, uint8_t cpu_mask)
{
    const struct interrupt_t* i = &irq_handlers[irq];
    io_apic_enable_irq(irq, i->vector, cpu_mask);
}

void intr_irq_disable(uint8_t irq)
{
    io_apic_disable_irq(irq);
}
