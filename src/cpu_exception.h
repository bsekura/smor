#ifndef KERNEL_CPU_EXCEPTION_H
#define KERNEL_CPU_EXCEPTION_H

#include "cpu.h"

void cpu_exception(struct isr_frame_t frame);

#endif // KERNEL_CPU_EXCEPTION_H
