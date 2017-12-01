#include "pit.h"
#include "io.h"

#define TRES_PORT       (0x40)

/* 8254 programmable interrupt timer */
#define PITCTR0_PORT    (0x40)      /* counter 0 */
#define PITCTR1_PORT    (0x41)      /* counter 1 */
#define PITCTR2_PORT    (0x42)      /* counter 2 */

#define PITCTL_PORT     (0x43)      /* control port */
#define PITAUX_PORT     (0x61)      /* auxiliary port */

/* 8254 commands */

/* timer 0 */
#define PIT_C0          (0x00)      /* select counter 0 */
#define PIT_LOADMODE    (0x30)      /* load LSB followed by MSB */
#define PIT_NDIVMODE    (0x04)      /* divide by N counter */
#define PIT_SQUAREMODE  (0x06)      /* square-wave mode */

/* timer 1 */
#define PIT_C1          (0x40)      /* select counter 1 */
#define PIT_READMODE    (0x30)      /* read or load LSB, MSB */
#define PIT_RATEMODE    (0x06)      /* sqaure-wave mode for USART */

/* auxiliary control port for timer 2 */
#define PITAUX_GATE2    (0x01)      /* aux port, PIT gate 2 input */
#define PITAUX_OUT2     (0x02)      /* aux port, PIT clock out 2 enable */

#define CLKNUM      (1193167)
#define HZ          (1000)

void pit_init()
{
    unsigned int clock = (CLKNUM/HZ);

    outb(PITCTL_PORT, PIT_C0 | PIT_SQUAREMODE | PIT_LOADMODE);
    outb(PITCTR0_PORT, clock & 0x00FF);
    outb(PITCTR0_PORT, (clock & 0xFF00) >> 8);
}
