#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H

#include "types.h"

#define IRQ_TIMER       0x00
#define IRQ_KEYBOARD    0x01

struct isr_frame_t;
typedef void (*interrupt_handler_fn)(struct isr_frame_t* frame);

void intr_init(void);
void intr_register_irq_handler(uint8_t irq, interrupt_handler_fn handler);
void intr_register_local_irq_handler(uint8_t irq, uint8_t vector, interrupt_handler_fn handler);
void intr_irq_enable(uint8_t irq, uint8_t cpu_mask);
void intr_irq_disable(uint8_t irq);

#endif // KERNEL_INTERRUPT_H
