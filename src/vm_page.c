#include "vm_page.h"
#include "vm_boot.h"
#include "kernel.h"
#include "spinlock.h"
#include "stdio.h"

static inline void page_desc_init(struct page_desc_t* page, uint32_t flags)
{
    page->next_cache = NULL;
    page->prev_cache = NULL;
    page->next_hash = NULL;
    page->cache = NULL;
    page->cache_offset = 0;
    page->next_free = 0;
    page->flags = flags;
    page->vaddr = 0;
    page->reserved = 0;
}

static void page_db_init_pages(struct page_db_t* pdb,
                               uint32_t num_pages,
                               uint32_t num_reserved)
{
    uint32_t i;
    for (i = 0; i < num_reserved; ++i)
        page_desc_init(&pdb->pages[i], PGF_RESERVED);

    for (i = num_reserved; i < num_pages-1; ++i) {
        page_desc_init(&pdb->pages[i], PGF_FREE);
        pdb->pages[i].next_free = i+1;
    }
    page_desc_init(&pdb->pages[i], PGF_FREE);

    pdb->free_list = num_reserved;
}

static uint32_t page_db_calc_size(uint64_t memory_size)
{
    uint32_t num_pages = (uint32_t)(memory_size >> PAGE_2M_SHIFT);
    uint32_t db_size =
        PAGE_4K_ROUND(sizeof(struct page_db_t)
                    + sizeof(struct page_desc_t) * num_pages);

    return db_size;
}

void page_db_init(struct page_db_t* pdb,
                  uint64_t memory_size, uint64_t reserved_size)
{
    uint32_t num_reserved = (uint32_t)PAGE_2M_NUM(reserved_size);
    uint32_t num_pages = (uint32_t)(memory_size >> PAGE_2M_SHIFT);
    uint32_t db_size =
        PAGE_4K_ROUND(sizeof(struct page_db_t)
                    + sizeof(struct page_desc_t) * num_pages);

    printf("page_db_init: %016lx num_pages: [%d,%d] db_size: %08x\n",
        (uintptr_t)pdb, num_reserved, num_pages, db_size);

    pdb->page_index = 0;
    pdb->num_pages = num_pages;
    pdb->num_free = num_pages - num_reserved;
    pdb->num_reserved = (uint16_t)num_reserved;
    pdb->flags = 0;

    spinlock_init(&pdb->lock);

    page_db_init_pages(pdb, num_pages, num_reserved);
}

void page_db_init_4k(struct page_db_t* pdb, uint32_t page_index)
{
    uint32_t num_pages = PAGE_2M_SIZE >> PAGE_4K_SHIFT;
    uint32_t db_size =
        PAGE_4K_ROUND(sizeof(struct page_db_t)
                    + sizeof(struct page_desc_t) * num_pages);

    uint32_t num_reserved = db_size >> PAGE_4K_SHIFT;

    printf("page_db_init_4k: %p:%d db_size: %08x num_pages %d num_reserved %d num_free %d\n",
        pdb, page_index, db_size, num_pages, num_reserved, num_pages - num_reserved);

    pdb->page_index = page_index;
    pdb->num_pages = num_pages;
    pdb->num_free = num_pages - num_reserved;
    pdb->num_reserved = (uint16_t)num_reserved;
    pdb->flags = 0;

    spinlock_init(&pdb->lock);

    page_db_init_pages(pdb, num_pages, num_reserved);
}

uint32_t page_db_alloc(struct page_db_t* pdb)
{
    uint32_t index = pdb->free_list;
    if (index > 0) {
        check(pdb->pages[index].flags == PGF_FREE);
        pdb->free_list = pdb->pages[index].next_free;
        pdb->pages[index].flags = PGF_USED;
        --pdb->num_free;
    }
    return index;
}

void page_db_free(struct page_db_t* pdb, uint32_t index)
{
    check(index >= pdb->num_reserved && index < pdb->num_pages);
    check(pdb->pages[index].flags != PGF_FREE);

    pdb->pages[index].flags = PGF_FREE;
    pdb->pages[index].next_free = pdb->free_list;
    pdb->free_list = index;
    ++pdb->num_free;
}

// given descriptor get page physical address
uintptr_t page_db_desc2addr(struct page_db_t* pdb, struct page_desc_t* page)
{
    uint32_t index = page_db_index(pdb, page);
    return page_db_index2addr(pdb, index);
}

// given page index, get page physical address
uintptr_t page_db_index2addr(struct page_db_t* pdb, uint32_t index)
{
    if (pdb->page_index) {
        uintptr_t base = (uintptr_t)pdb->page_index << PAGE_2M_SHIFT;
        uintptr_t offset = (uintptr_t)index << PAGE_4K_SHIFT;
        return base + offset;
    }
    return (uintptr_t)index << PAGE_2M_SHIFT;
}

uint32_t page_db_addr2index(struct page_db_t* pdb, uintptr_t addr)
{
    if (pdb->page_index) {
        uintptr_t base = (uintptr_t)pdb->page_index << PAGE_2M_SHIFT;
        uint32_t index = PAGE_4K_INDEX(addr);
        check(addr > base && addr < base + PAGE_2M_SIZE);
        check(index >= pdb->num_reserved && index < pdb->num_pages);
        return index;
    }
    return (uint32_t)(addr >> PAGE_2M_SHIFT);
}

uintptr_t page_db_4k_vaddr(struct page_db_t* pdb, uint32_t index)
{
    check(index >= pdb->num_reserved && index < pdb->num_pages);
    return (uintptr_t)pdb + ((uintptr_t)index << PAGE_4K_SHIFT);
}

uintptr_t page_db_alloc_addr(struct page_db_t* pdb)
{
    uint32_t index = page_db_alloc(pdb);
    return index > 0 ? page_db_index2addr(pdb, index) : 0;
}

void page_db_free_addr(struct page_db_t* pdb, uintptr_t addr)
{
    uint32_t index = page_db_index2addr(pdb, addr);
    if (index)
        page_db_free(pdb, index);
}

void page_db_reserve_page(struct page_db_t* pdb, uint32_t page_index)
{
    if (page_index >= pdb->num_pages)
        return;
    if (pdb->pages[page_index].flags == PGF_RESERVED)
        return;
    check(pdb->pages[page_index].flags == PGF_FREE);
    if (page_index == pdb->num_reserved) {
        pdb->pages[page_index].flags = PGF_RESERVED;
        pdb->pages[page_index].next_free = 0;
        pdb->num_reserved++;
        pdb->free_list = pdb->num_reserved;
        return;
    }
    for (uint32_t i = pdb->num_reserved; i < pdb->num_pages; ++i) {
        if (pdb->pages[i].next_free == page_index) {
            pdb->pages[i].next_free = pdb->pages[page_index].next_free;
            pdb->pages[page_index].flags = PGF_RESERVED;
            break;
        }
    }
}

void page_db_reserve_region(struct page_db_t* pdb,
                            uintptr_t addr, uint64_t size)
{
    uint32_t page_index = (uint32_t)(addr >> PAGE_2M_SHIFT);
    check(page_index >= pdb->num_reserved);
    uint32_t num_pages = (uint32_t)PAGE_2M_NUM(size + (addr & (PAGE_2M_SIZE-1)));
    for (uint32_t i = 0; i < num_pages; ++i)
        page_db_reserve_page(pdb, page_index++);
}

void page_db_dump_ranges(struct page_db_t* pdb)
{
    uint32_t n = 0;
    while (n < pdb->num_pages) {
        struct page_desc_t* p = &pdb->pages[n];
        if (p->flags == PGF_FREE) {
            uint32_t s = n;
            while (n < pdb->num_pages && pdb->pages[n].flags == PGF_FREE)
                ++n;
            //uint32_t e = n - s;
            printf("free:[%04d:%04d] %016lx:%016lx\n", s, n,
                    (s << PAGE_2M_SHIFT), (n << PAGE_2M_SHIFT));
        } else if (p->flags == PGF_RESERVED) {
            uint32_t s = n;
            while (n < pdb->num_pages && pdb->pages[n].flags == PGF_RESERVED)
                ++n;
            //uint32_t e = n - s;
            printf("rsvd:[%04d:%04d] %016lx:%016lx\n", s, n,
                    (s << PAGE_2M_SHIFT), (n << PAGE_2M_SHIFT));
        } else if (p->flags == PGF_USED) {
            uint32_t s = n;
            while (n < pdb->num_pages && pdb->pages[n].flags == PGF_USED)
                ++n;
            //uint32_t e = n - s;
            printf("used:[%04d:%04d] %016lx:%016lx\n", s, n,
                    (s << PAGE_2M_SHIFT), (n << PAGE_2M_SHIFT));
        }
    }
}

void page_db_dump_free_list(struct page_db_t* pdb)
{
    uint32_t index = pdb->free_list;
    while (index > 0) {
        uint32_t s = index;
        while (pdb->pages[index].next_free
                && pdb->pages[index].next_free == index+1)
            ++index;

        uint32_t n = (index+1) - s;
        if (n)
            printf("[%d:%d]\n", s, n);

        index = pdb->pages[index].next_free;
    }
}

void zero_page_4k(uintptr_t addr)
{
    uint64_t* p = (uint64_t*)addr;
    uint32_t n = PAGE_4K_SIZE/8;
    while (n--)
        *p++ = 0UL;
}

void zero_page_2m(uintptr_t addr)
{
    uint64_t* p = (uint64_t*)addr;
    uint32_t n = PAGE_2M_SIZE/8;
    while (n--)
        *p++ = 0UL;
}

void copy_page_4k(uintptr_t dst, uintptr_t src)
{
    const uint64_t* p_src = (uint64_t*)src;
    uint64_t* p_dst = (uint64_t*)dst;
    uint32_t n = PAGE_4K_SIZE/8;
    while (n--)
        *p_dst++ = *p_src++;
}

void copy_page_2m(uintptr_t dst, uintptr_t src)
{
    const uint64_t* p_src = (uint64_t*)src;
    uint64_t* p_dst = (uint64_t*)dst;
    uint32_t n = PAGE_2M_SIZE/8;
    while (n--)
        *p_dst++ = *p_src++;
}

struct page_db_t* page_db;

void vm_page_init()
{
    struct boot_info_t* boot_info = kernel_boot_info();
    uint64_t memory_size = boot_info->mmap_top;
    uint32_t db_size = page_db_calc_size(memory_size);

    printf("memory_size: %016lx, db_size: %08x\n", memory_size, db_size);

    page_db = (struct page_db_t*)kernel_slack_alloc(db_size, 16);
    page_db_init(page_db, memory_size, boot_info->kernel_top);
    //page_db_dump_ranges(page_db);
}
