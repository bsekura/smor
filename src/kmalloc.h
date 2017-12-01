#ifndef KERNEL_KMALLOC_H
#define KERNEL_KMALLOC_H

#include "slab.h"

#define KMALLOC_CHUNK_SIZE  0x4000

void kmalloc_init(void);
void* kmalloc_alloc(void);
void kmalloc_free(void* ptr);
struct slab_list_t* kmalloc_get_slab(uint32_t elem_size);

#endif // KERNEL_KMALLOC_H
