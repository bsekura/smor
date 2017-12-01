#ifndef KERNEL_PIC_H
#define KERNEL_PIC_H

#include "types.h"

void pic_init(void);
void pic_eoi(uint32_t irq_num);
void pic_enable_irq(uint16_t irq_num);
void pic_disable_irq(uint16_t irq_num);

#endif // KERNEL_PIC_H
