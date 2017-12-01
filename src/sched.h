#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include "types.h"

struct cpu_desc_t;

void sched_init(void);
void sched_dump(void);
void sched_init_cpu(struct cpu_desc_t* cpu);
void sched_tick(struct cpu_desc_t* cpu);
void sched_yield(void);
void sched_yield_locked(struct cpu_desc_t* cpu);
void sched_sleep(uint32_t ms);

#endif // KERNEL_SCHED_H
