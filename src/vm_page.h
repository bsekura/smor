#ifndef KERNEL_VM_PAGE_H
#define KERNEL_VM_PAGE_H

#include "types.h"
#include "spinlock.h"

#define PAGE_4K_SHIFT       12UL    // 4K page
#define PAGE_4K_SIZE        (1UL<<PAGE_4K_SHIFT)
#define PAGE_4K_MASK        (~(PAGE_4K_SIZE-1))

#define PAGE_2M_SHIFT       21UL    // 2Mb page
#define PAGE_2M_SIZE        (1UL<<PAGE_2M_SHIFT)
#define PAGE_2M_MASK        (~(PAGE_2M_SIZE-1))

#define PML_SHIFT           39UL    // 512GB x 512 = 256TB space
#define PML_SIZE            (1UL<<PML_SHIFT)
#define PML_MASK            (~(PML_SIZE-1))

#define PDP_SHIFT           30UL    // 1GB x 512 = 512GB space
#define PDP_SIZE            (1UL<<PDP_SHIFT)
#define PDP_MASK            (~(PDP_SIZE-1))

#define PDIR_NUM_ENTRIES    0x200
#define PDIR_ADDR(x)        ((x) & ~(PDIR_NUM_ENTRIES-1))

// page directory indices
#define PML_INDEX(x)        (((x) >> PML_SHIFT) & 0x1FF)
#define PDP_INDEX(x)        (((x) >> PDP_SHIFT) & 0x1FF)
#define PAGE_2M_INDEX(x)    (((x) >> PAGE_2M_SHIFT) & 0x1FF)
#define PAGE_4K_INDEX(x)    (((x) >> PAGE_4K_SHIFT) & 0x1FF)

// size to rounded number of
#define PML_NUM(x)          (((x) + (PML_SIZE-1)) >> PML_SHIFT)
#define PDP_NUM(x)          (((x) + (PDP_SIZE-1)) >> PDP_SHIFT)
#define PAGE_2M_NUM(x)      (((x) + (PAGE_2M_SIZE-1)) >> PAGE_2M_SHIFT)
#define PAGE_4K_NUM(x)      (((x) + (PAGE_4K_SIZE-1)) >> PAGE_4K_SHIFT)

#define PAGE_2M_ROUND(x)    (((x) + (PAGE_2M_SIZE-1)) & PAGE_2M_MASK)
#define PAGE_4K_ROUND(x)    (((x) + (PAGE_4K_SIZE-1)) & PAGE_4K_MASK)

#define PAGE_PRESENT    0x01
#define PAGE_WRITE      0x02
#define PAGE_USER       0x04
#define PAGE_2MB        0x80

static inline void invlpg(uintptr_t addr)
{
    asm volatile(
        "invlpg (%0)"
        :
        :"r"(addr)
        : "memory"
    );
}

static inline void tlb_flush()
{
    unsigned long tmp;
    asm volatile(
        "mov %%cr3, %0\n\t"
        "mov %0,%%cr3" : "=r"(tmp)
    );
}

#define PGF_FREE        0
#define PGF_USED        1
#define PGF_RESERVED    2

struct page_cache_t;

struct page_desc_t {
    struct page_desc_t* next_cache;
    struct page_desc_t** prev_cache;
    struct page_desc_t* next_hash;
    struct page_cache_t* cache;
    uint64_t cache_offset;
    uint32_t next_free;
    uint32_t flags;
    uint64_t vaddr;     // mapped for kernel use, debug
    uint64_t reserved;
};

struct page_db_t {
    uint32_t page_index;    // big page index if we are small page database
    uint32_t num_pages;
    uint32_t num_free;
    uint32_t free_list;
    struct spinlock_t lock;
    uint16_t num_reserved;
    uint16_t flags;
    struct page_desc_t pages[];
};

static inline uint32_t page_db_index(struct page_db_t* pdb,
                                     struct page_desc_t* page)
{
    return (uint32_t)(page - pdb->pages);
}

static inline struct page_desc_t* page_db_desc(struct page_db_t* pdb,
                                               uint32_t page_index)
{
    return &pdb->pages[page_index];
}

extern struct page_db_t* page_db;

void page_db_init(struct page_db_t* pdb,
                  uint64_t memory_size, uint64_t reserved_size);
void page_db_init_4k(struct page_db_t* pdb, uint32_t page_index);
uint32_t page_db_alloc(struct page_db_t* pdb);
void page_db_free(struct page_db_t* pdb, uint32_t index);
uintptr_t page_db_alloc_addr(struct page_db_t* pdb);
void page_db_free_addr(struct page_db_t* pdb, uintptr_t addr);
uintptr_t page_db_desc2addr(struct page_db_t* pdb, struct page_desc_t* page);
uintptr_t page_db_index2addr(struct page_db_t* pdb, uint32_t index);
uint32_t page_db_addr2index(struct page_db_t* pdb, uintptr_t addr);
uintptr_t page_db_4k_vaddr(struct page_db_t* pdb, uint32_t index);
void page_db_reserve_page(struct page_db_t* pdb, uint32_t page_index);
void page_db_reserve_region(struct page_db_t* pdb,
                            uintptr_t addr, uint64_t size);
void page_db_dump_ranges(struct page_db_t* pdb);
void page_db_dump_free_list(struct page_db_t* pdb);

void zero_page_4k(uintptr_t addr);
void zero_page_2m(uintptr_t addr);
void copy_page_4k(uintptr_t dst, uintptr_t src);
void copy_page_2m(uintptr_t dst, uintptr_t src);

void vm_page_init(void);

#endif // KERNEL_VM_PAGE_H
