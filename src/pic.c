#include "pic.h"
#include "io.h"

/* programmable interrupt controler ports */

#define MASTER_PIC_CMD      (0x20)
#define MASTER_PIC_INTMASK  (0x21)

#define SECOND_PIC_CMD      (0xA0)
#define SECOND_PIC_INTMASK  (0xA1)

/* initialization command words */

#define ICW1_NEED_ICW4      BIT(0)
#define ICW1_NO_ICW4        (0)
#define ICW1_SINGLE         BIT(1)
#define ICW1_CASCADING      (0)
#define ICW1_4_BYTE_INTVEC  BIT(2)
#define ICW1_8_BYTE_INTVEC  (0)
#define ICW1_LEVEL_TRIG     BIT(3)
#define ICW1_EDGE_TRIG      (0)
#define ICW1_BASE           BIT(4)

#define ICW2_INTVEC_MASK    (BIT(7)|BIT(6)|BIT(5)|BIT(4)|BIT(3))

#define ICW3_INT_0_SLAVE    BIT(0)
#define ICW3_INT_1_SLAVE    BIT(1)
#define ICW3_INT_2_SLAVE    BIT(2)
#define ICW3_INT_3_SLAVE    BIT(3)
#define ICW3_INT_4_SLAVE    BIT(4)
#define ICW3_INT_5_SLAVE    BIT(5)
#define ICW3_INT_6_SLAVE    BIT(6)
#define ICW3_INT_7_SLAVE    BIT(7)

#define ICW3_SLAVE_ON_1     (0x01)
#define ICW3_SLAVE_ON_2     (0x02)

#define ICW4_80x86_MODE     BIT(0)
#define ICW4_8085_MODE      (0)
#define ICW4_AUTO_EOI       BIT(1)
#define ICW4_NORMAL_EOI     (0)
#define ICW4_SFN_MODE       BIT(4)
#define ICW4_SEQUENTIAL     (0)
#define ICW4_BUF_SLAVE      BIT(3)
#define ICW4_BUF_MASTER     BIT(2) | BIT(3)

#define OCW1_MASK_OFF_ALL   (0xFF)

#define OCW2_NON_SPEC_EOI   BIT(5)

#define SLAVE_IRQ           8
#define MASTER_SLAVE        2

static uint16_t irq_mask = 0xFFFF;

void pic_init()
{
    uint16_t master = 0x20;
    uint16_t slave = 0x28;

    outb(MASTER_PIC_CMD, ICW1_BASE | ICW1_NEED_ICW4);
    outb(MASTER_PIC_INTMASK, ICW2_INTVEC_MASK & master);
    outb(MASTER_PIC_INTMASK, ICW3_INT_2_SLAVE);
    outb(MASTER_PIC_INTMASK, ICW4_80x86_MODE);
    outb(MASTER_PIC_INTMASK, OCW1_MASK_OFF_ALL);
    outb(MASTER_PIC_CMD, OCW2_NON_SPEC_EOI);

    outb(SECOND_PIC_CMD, ICW1_BASE | ICW1_NEED_ICW4);
    outb(SECOND_PIC_INTMASK, ICW2_INTVEC_MASK & slave);
    outb(SECOND_PIC_INTMASK, ICW3_SLAVE_ON_2);
    outb(SECOND_PIC_INTMASK, ICW4_80x86_MODE);
    outb(SECOND_PIC_INTMASK, OCW1_MASK_OFF_ALL);
    outb(SECOND_PIC_CMD, OCW2_NON_SPEC_EOI);
}

static inline void irq_ack_master()
{
    asm volatile(
        "movb $0x20, %al\n\t"
        "outb %al, $0x20"
    );
}

static inline void irq_ack_slave()
{
    asm volatile(
        "movb $0x20, %al\n\t"
        "outb %al, $0x20\n\t"
        "outb %al, $0xA0"
    );
}

void pic_eoi(uint32_t irq_num)
{
    if (irq_num > 8)
        irq_ack_master();
    else
        irq_ack_slave();
}

void pic_enable_irq(uint16_t irq_num)
{
    irq_mask &= ~(1 << irq_num);
    if (irq_num >= SLAVE_IRQ)
        irq_mask &= ~(1 << MASTER_SLAVE);

    outb(MASTER_PIC_INTMASK, irq_mask & 0xff);
    outb(SECOND_PIC_INTMASK, (irq_mask >> 8) & 0xff);
}

void pic_disable_irq(uint16_t irq_num)
{
    irq_mask |= (1 << irq_num);
    if (irq_num >= SLAVE_IRQ)
        irq_mask |= (1 << MASTER_SLAVE);

    outb(MASTER_PIC_INTMASK, irq_mask & 0xff);
    outb(SECOND_PIC_INTMASK, (irq_mask >> 8) & 0xff);
}

