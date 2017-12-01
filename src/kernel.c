#include "kernel.h"
#include "stdio.h"
#include "cpu.h"
#include "vm_boot.h"
#include "vm_page.h"

void kernel_panic(const char* msg)
{
    cpu_disable_interrupts();
    printf("kernel_panic(): %s\n", msg);
    cpu_halt();
}

extern uint8_t _end;

void kernel_init()
{
    struct boot_info_t* boot_info = kernel_boot_info();
    boot_info->kernel_end = (uint64_t)&_end - KERNEL_BASE;
    boot_info->kernel_top = PAGE_2M_ROUND(boot_info->kernel_end);
    boot_info->kernel_slack = boot_info->kernel_end;
}

static struct spinlock_t slack_lock;

uintptr_t kernel_slack_alloc(uint64_t size, uint64_t align)
{
    struct boot_info_t* boot_info = kernel_boot_info();
    spinlock_lock(&slack_lock);
    uint64_t slack = ALIGN_UP(boot_info->kernel_slack, align);
    uint64_t top = slack + size;
    if (top > boot_info->kernel_top) {
        uint64_t paddr = boot_info->kernel_top;
        uint64_t alloc_size = PAGE_2M_ROUND(top - boot_info->kernel_top);
        boot_info->kernel_top += alloc_size;
        vm_boot_map_range(KERNEL_VADDR(paddr), paddr, alloc_size);
    }

    boot_info->kernel_slack = top;
    spinlock_unlock(&slack_lock);

    return KERNEL_VADDR(slack);
}
