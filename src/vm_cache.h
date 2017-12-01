#ifndef KERNEL_VM_CACHE_H
#define KERNEL_VM_CACHE_H

#include "types.h"

struct page_desc_t;

struct page_hash_t {
    struct page_desc_t** pages;
    uint32_t size;
};

#define PAGE_CACHE_4K_PAGES 0x01
#define PAGE_CACHE_NO_HASH  0x02

typedef void (*page_cache_fn)(uintptr_t vaddr, uint64_t offset);

struct page_cache_t {
    struct page_desc_t* pages;
    struct page_hash_t hash;
    page_cache_fn read_page;
    page_cache_fn write_page;
    uint64_t size; // size may be unaligned to page size
    uint32_t flags;
    int map_refcnt;
};

void vm_hash_init(void);
void vm_cache_init(void);

struct page_cache_t* vm_cache_create(uint64_t size, uint32_t flags);
void vm_cache_release(struct page_cache_t* cache);
void vm_cache_delete(struct page_cache_t* cache);

void page_cache_insert(struct page_cache_t* cache,
                       struct page_desc_t* page);
void page_cache_remove(struct page_desc_t* page);
void page_cache_dump(struct page_cache_t* cache);

void page_hash_insert(struct page_cache_t* cache,
                      uint64_t offset,
                      struct page_desc_t* page);
struct page_desc_t* page_hash_find(struct page_cache_t* cache,
                                   uint64_t offset);
void page_hash_remove(struct page_cache_t* cache,
                      uint64_t offset,
                      struct page_desc_t* page_desc);
void page_hash_dump(struct page_hash_t* hash);

#endif // KERNEL_VM_CACHE_H
