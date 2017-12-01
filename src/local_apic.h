#ifndef KERNEL_LOCAL_APIC_H
#define KERNEL_LOCAL_APIC_H

#include "cpu.h"

struct local_apic_cpu_t {
    uint8_t apic_id;
    uint8_t flags;
};

struct local_apic_t {
    uint8_t* local_apic_addr;
    uint32_t num_cpus;
    struct local_apic_cpu_t cpus[MAX_CPUS];
};

extern struct local_apic_t local_apic;

void local_apic_register(uintptr_t addr);
void local_apic_add_cpu(uint8_t apic_id);
void local_apic_init(void);
void local_apic_timer_init(void);
uint32_t local_apic_id(void);
void local_apic_eoi(void);
void local_apic_ipi_init(uint32_t apic_id);
void local_apic_ipi_start(uint32_t apic_id);
void local_apic_ipi_self(uint32_t vector);
void local_apic_ipi_broadcast(uint32_t vector);
void local_apic_ipi_all(uint32_t vector);

#endif // KERNEL_LOCAL_APIC_H
