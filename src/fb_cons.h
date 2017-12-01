#ifndef KERNEL_FB_CONS_H
#define KERNEL_FB_CONS_H

void fb_cons_init(void);
void fb_cons_clear(void);
void fb_cons_putchar(unsigned char ch);
void fb_cons_clear_char(unsigned int num);
void fb_cons_clear_line(void);

#endif // KERNEL_FB_CONS_H
