#ifndef KERNEL_VGA_CONS_H
#define KERNEL_VGA_CONS_H

void vga_cons_init(void);
void vga_cons_clear(void);
void vga_cons_putchar(unsigned char ch);

#endif // KERNEL_VGA_CONS_H
