#ifndef KERNEL_KBD_8042_H
#define KERNEL_KBD_8042_H

#include "types.h"

void kbd_8042_init(void);
int kbd_read(char* out_buf);

#endif // KERNEL_KBD_8042_H
