#include "vm_boot.h"
#include "vm_page.h"
#include "kernel.h"
#include "spinlock.h"
#include "stdio.h"

#define BOOT_PML4       0x10000UL
#define BOOT_PD         0x11000UL
#define BOOT_PDP_HI     0x12000UL
#define BOOT_PDP_LO     0x13000UL
#define BOOT_PAGE_NEXT  0x14000UL

#define BOOT_PAGES_TOP  0x30000UL

static uint64_t boot_page;
static struct spinlock_t vm_boot_lock;

static uint64_t boot_page_alloc()
{
    uint64_t page = boot_page;
    check(page < BOOT_PAGES_TOP);
        
    boot_page += PAGE_4K_SIZE;

    uint64_t* p = (uint64_t*)KERNEL_VADDR(page);
    uint32_t n = PDIR_NUM_ENTRIES;
    while (n--)
        *p++ = 0UL;

    return page;
}

static void vm_boot_map_page(uintptr_t vaddr, uintptr_t paddr)
{
    uint32_t pml4_index = PML_INDEX(vaddr);
    uint32_t pdp_index = PDP_INDEX(vaddr);
    uint32_t pd_index = PAGE_2M_INDEX(vaddr);

    uint64_t* pml4 = (uint64_t*)KERNEL_VADDR(BOOT_PML4);
    uint64_t pdp_entry = pml4[pml4_index];
    if (!(pdp_entry & PAGE_PRESENT)) {
        pdp_entry = boot_page_alloc() | PAGE_PRESENT | PAGE_WRITE;
        pml4[pml4_index] = pdp_entry;
    }
    uint64_t* pdp = (uint64_t*)KERNEL_VADDR(PDIR_ADDR(pdp_entry));
    uint64_t pd_entry = pdp[pdp_index];
    if (!(pd_entry & PAGE_PRESENT)) {
        pd_entry = boot_page_alloc() | PAGE_PRESENT | PAGE_WRITE;
        pdp[pdp_index] = pd_entry;
    }
    uint64_t* pd = (uint64_t*)KERNEL_VADDR(PDIR_ADDR(pd_entry));
    if (!(pd[pd_index] & PAGE_PRESENT) || PDIR_ADDR(pd[pd_index]) != paddr) {
        pd[pd_index] = (uint64_t)paddr | PAGE_PRESENT | PAGE_WRITE | PAGE_2MB;
        invlpg(vaddr);
    }
}

static void vm_boot_unmap_page(uintptr_t vaddr)
{
    uint32_t pml4_index = PML_INDEX(vaddr);
    uint32_t pdp_index = PDP_INDEX(vaddr);
    uint32_t pd_index = PAGE_2M_INDEX(vaddr);

    uint64_t* pml4 = (uint64_t*)KERNEL_VADDR(BOOT_PML4);
    uint64_t pdp_entry = pml4[pml4_index];
    if (pdp_entry & PAGE_PRESENT) {
        uint64_t* pdp = (uint64_t*)KERNEL_VADDR(PDIR_ADDR(pdp_entry));
        uint64_t pd_entry = pdp[pdp_index];
        if (pd_entry & PAGE_PRESENT) {
            uint64_t* pd = (uint64_t*)KERNEL_VADDR(PDIR_ADDR(pd_entry));
            pd[pd_index] = 0;
            invlpg(vaddr);
        }
    }
}

void vm_boot_map_range(uintptr_t vaddr, uintptr_t paddr, uint64_t size)
{
    uint64_t num_pages = PAGE_2M_NUM(size + (vaddr & (PAGE_2M_SIZE-1)));

    vaddr &= PAGE_2M_MASK;
    paddr &= PAGE_2M_MASK;

    spinlock_lock(&vm_boot_lock);
    while (num_pages--) {
        vm_boot_map_page(vaddr, paddr);
        vaddr += PAGE_2M_SIZE;
        paddr += PAGE_2M_SIZE;
    }
    spinlock_unlock(&vm_boot_lock);
}

void vm_boot_unmap_range(uintptr_t vaddr, uint64_t size)
{
    uint64_t num_pages = PAGE_2M_NUM(size + (vaddr & (PAGE_2M_SIZE-1)));

    vaddr &= PAGE_2M_MASK;

    spinlock_lock(&vm_boot_lock);
    while (num_pages--) {
        vm_boot_unmap_page(vaddr);
        vaddr += PAGE_2M_SIZE;
    }
    spinlock_unlock(&vm_boot_lock);
}

static void pml4_dump(uintptr_t pml4_addr)
{
    printf("pml4 %016lx\n", pml4_addr);
    const uint64_t* pml4 = (uint64_t*)KERNEL_VADDR(pml4_addr);
    for (uint32_t pml4_index = 0; pml4_index < 512; ++pml4_index) {
        uint64_t pdp_entry = pml4[pml4_index];
        if (pdp_entry) {
            uint64_t pdp_addr = PDIR_ADDR(pdp_entry);
            printf("pml[%03d]: %016lx -> %016lx [%02x]\n",
                    pml4_index, pdp_addr,
                    (uint64_t)pml4_index << PML_SHIFT,
                    (uint32_t)(pdp_entry & 0x1FF));
            const uint64_t* pdp = (uint64_t*)KERNEL_VADDR(pdp_addr);
            for (uint32_t pdp_index = 0; pdp_index < 512; ++pdp_index) {
                uint64_t pd_entry = pdp[pdp_index];
                if (pd_entry) {
                    uint64_t pd_addr = PDIR_ADDR(pd_entry);
                    uint64_t pdp_vaddr = ((uint64_t)pml4_index << PML_SHIFT)
                                       + ((uint64_t)pdp_index << PDP_SHIFT);
                    printf("\tpdp[%03d]: %016lx -> %016lx [%02x]\n",
                            pdp_index, pd_addr,
                            pdp_vaddr,
                            (uint32_t)(pd_entry & 0x1FF));

                    const uint64_t* pd = (uint64_t*)KERNEL_VADDR(pd_addr);
                    for (uint32_t pd_index = 0; pd_index < 512; ++pd_index) {
                        uint64_t pt_entry = pd[pd_index];
                        if (pt_entry) {
                            uint64_t pt_addr = PDIR_ADDR(pt_entry);
                            uint64_t pt_vaddr = ((uint64_t)pml4_index << PML_SHIFT)
                                              + ((uint64_t)pdp_index << PDP_SHIFT)
                                              + ((uint64_t)pd_index << PAGE_2M_SHIFT);
                            printf("\t\tpd[%03d]: %016lx -> %016lx [%02x]\n",
                                    pd_index, pt_addr,
                                    pt_vaddr,
                                    (uint32_t)(pt_entry & 0x1FF));
                        }
                    }
                }
            }
        }
    }
}

void vm_boot_dump()
{
    pml4_dump(BOOT_PML4);
}

void vm_boot_init()
{
    boot_page = BOOT_PAGE_NEXT;
}

void vm_boot_sync()
{
    // find all mapped pages so far and mark them as reserved
    const uint64_t* pml4 = (uint64_t*)KERNEL_VADDR(BOOT_PML4);
    for (uint32_t pml4_index = 0; pml4_index < 512; ++pml4_index) {
        uint64_t pdp_entry = pml4[pml4_index];
        if (pdp_entry) {
            uint64_t pdp_addr = PDIR_ADDR(pdp_entry);
            const uint64_t* pdp = (uint64_t*)KERNEL_VADDR(pdp_addr);
            for (uint32_t pdp_index = 0; pdp_index < 512; ++pdp_index) {
                uint64_t pd_entry = pdp[pdp_index];
                if (pd_entry) {
                    uint64_t pd_addr = PDIR_ADDR(pd_entry);
                    const uint64_t* pd = (uint64_t*)KERNEL_VADDR(pd_addr);
                    for (uint32_t pd_index = 0; pd_index < 512; ++pd_index) {
                        uint64_t pt_entry = pd[pd_index];
                        if (pt_entry) {
                            uint64_t pt_addr = PDIR_ADDR(pt_entry);
                            uint32_t page_index = (uint32_t)(pt_addr >> PAGE_2M_SHIFT);
                            //printf("pt_addr: %016lx %d\n", pt_addr, page_index);
                            page_db_reserve_page(page_db, page_index);
                        }
                    }
                }
            }
        }
    }

    page_db_dump_ranges(page_db);
}
