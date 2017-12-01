#ifndef KERNEL_VM_BOOT_H
#define KERNEL_VM_BOOT_H

#include "types.h"

void vm_boot_init(void);
void vm_boot_sync(void);
void vm_boot_map_range(uintptr_t vaddr, uintptr_t paddr, uint64_t size);
void vm_boot_unmap_range(uintptr_t vaddr, uint64_t size);
void vm_boot_dump(void);

#endif // KERNEL_VM_BOOT_H
