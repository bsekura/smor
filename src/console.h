#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

void console_init(void);
void console_putstr(const char* str);
void console_putchar(unsigned char ch);
void console_clear_char(unsigned int num);
void console_clear_line(void);

#endif // KERNEL_CONSOLE_H
