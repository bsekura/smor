#include "types.h"
#include "stdio.h"
#include "cpu.h"
#include "sched.h"
#include "cond.h"
#include "thread.h"
#include "io.h"
#include "pic.h"
#include "pit.h"
#include "pci.h"
#include "vm_boot.h"
#include "vm_page.h"
#include "vm_cache.h"
#include "acpi.h"
#include "imps.h"
#include "kernel.h"
#include "console.h"
#include "fb_cons.h"
#include "multiboot.h"
#include "kbd_8042.h"
#include "ata.h"
#include "kterm.h"
#include "kmalloc.h"

extern uint8_t _end;

void kernel_main(void);

//uint8_t crap[3*1024*1024];

#if 0
static void boot_debug()
{
    struct boot_info_t* boot_info = kernel_boot_info();
    printf("mmap_top: %016lx\n", boot_info->mmap_top);
    printf("mmap: %ldG %ldM %ldK\n",
        boot_info->mmap_top/(1024UL*1024UL*1024UL),
        boot_info->mmap_top/(1024UL*1024UL),
        boot_info->mmap_top/1024UL);

    uint64_t num_pages = boot_info->mmap_top >> PAGE_2M_SHIFT;
    uint64_t top_addr = num_pages << PAGE_2M_SHIFT;

    printf("mmap: %ld pages, %016lx\n", num_pages, top_addr);
}
#endif

static struct spinlock_t cond_lock;
static struct condition_t cond_test;
static uint32_t cond_val;

static void test_thread()
{
    printf("test_thread()\n");
    struct cpu_desc_t* cpu = cpu_lock_splhi();
    uint32_t cpu_id = cpu->apic_id;
    uint32_t id = cpu->cur_thread->id;
    cpu_unlock_splx(cpu);

    printf("cpu: %d id: %d\n", cpu_id, id);

    //cpu_wait(300);
    sched_dump();

    printf("sleeping...\n");
    sched_sleep(5000);
    printf("back!\n");

    //cpu_wait(500);
    //sched_yield();

    while(1);
}

void kernel_main()
{
    kernel_init();
    vm_boot_init();

    if (!multiboot_init())
        return;
        
    console_init();
    printf("booting kif...%016lx\n", (uint64_t)&_end);

    vm_page_init();
    vm_hash_init();

    cpu_boot_init();
    pic_init();
    pit_init();
    acpi_init();

    kmalloc_init();
    vm_boot_sync();

    sched_init();
    cpu_init();

    kbd_8042_init();
    //ata_init();

#if 1
    //cpu_wait(300);
    spinlock_init(&cond_lock);
    cond_init(&cond_test);
    struct thread_t* t = thread_create(test_thread, 0x1000, 1);
    printf("created: %d\n", t->id);
    //for (uint32_t i = 0; i < num_cpus; ++i)
    //    printf("cpu[%d]: %d %d %d\n", i, cpus[i].flags, cpus[i].ticks, cpus[i].cur_thread->cnt);
#endif

    //cpu_wait(100);
    pci_init();
    //imps_init();

    kterm_start();
}
