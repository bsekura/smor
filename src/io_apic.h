#ifndef KERNEL_IO_APIC_H
#define KERNEL_IO_APIC_H

#include "spinlock.h"

struct io_apic_iovr_t {
    uint16_t bus;
    uint16_t irq;
    uint32_t gsi;
};

#define MAX_IO_APIC_IOVR     16

struct io_apic_t {
    uint8_t* io_apic_addr;
    uint32_t id;
    uint32_t ver;
    uint32_t num_redtbl;
    uint32_t gsi_base;
    struct spinlock_t lock;
};

extern struct io_apic_t io_apic;
extern struct io_apic_iovr_t io_apic_iovr[MAX_IO_APIC_IOVR];
extern uint32_t io_apic_num_iovr;

void io_apic_register(uintptr_t addr, uint32_t gsi_base);
void io_apic_add_iovr(uint8_t bus, uint8_t irq, uint32_t gsi);
void io_apic_init(void);
void io_apic_enable_irq(uint8_t irq, uint8_t vector, uint8_t cpu_mask);
void io_apic_disable_irq(uint8_t irq);

#endif // KERNEL_IO_APIC_H
