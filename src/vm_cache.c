#include "vm_cache.h"
#include "vm_page.h"
#include "vm_alloc.h"
#include "kernel.h"
#include "kmalloc.h"
#include "stdio.h"

static struct page_hash_t page_hash;
static struct slab_list_t* sl_page_cache;

static inline uint32_t hash_func(struct page_cache_t* cache, uintptr_t offset)
{
    uintptr_t page2m_offset = offset >> PAGE_2M_SHIFT;
    uint32_t hash_index = (uint32_t)(((uintptr_t)cache + page2m_offset) & (uintptr_t)(page_hash.size-1));

    printf("hash_index: %d [%p:%016llx]\n", hash_index, cache, offset);
    return hash_index;
}

static inline uint32_t hash_func_local(struct page_cache_t* cache, uintptr_t offset)
{
    uintptr_t page4k_offset = offset >> PAGE_4K_SHIFT;
    uint32_t hash_index = (uint32_t)(page4k_offset & (uintptr_t)(cache->hash.size-1));

    printf("hash_index[local]: %d [%p:%016llx]\n", hash_index, cache, offset);
    return hash_index;
}

void page_hash_insert(struct page_cache_t* cache,
                      uint64_t offset,
                      struct page_desc_t* page)
{
    uint32_t hash_index;
    struct page_hash_t* hash;
    if (cache->flags & PAGE_CACHE_4K_PAGES) {
        hash_index = hash_func_local(cache, offset);
        hash = &cache->hash;
    } else {
        hash_index = hash_func(cache, offset);
        hash = &page_hash;
    }

    struct page_desc_t* hash_pages = hash->pages[hash_index];
    if (hash_pages)
        hash_pages->next_hash = page;
    page->next_hash = NULL;
    hash->pages[hash_index] = page;

    page->cache = cache;
    page->cache_offset = offset;
}

struct page_desc_t* page_hash_find(struct page_cache_t* cache,
                                   uint64_t offset)
{
    uint32_t hash_index;
    struct page_hash_t* hash;
    if (cache->flags & PAGE_CACHE_4K_PAGES) {
        hash_index = hash_func_local(cache, offset);
        hash = &cache->hash;
    } else {
        hash_index = hash_func(cache, offset);
        hash = &page_hash;
    }

    struct page_desc_t* hash_pages = hash->pages[hash_index];
    struct page_desc_t* page = hash_pages;
    while (page) {
        if (page->cache == cache && page->cache_offset == offset)
            return page;
        page = page->next_hash;
    }

    return NULL;
}

void page_hash_remove(struct page_cache_t* cache,
                      uint64_t offset,
                      struct page_desc_t* page_desc)
{
    uint32_t hash_index;
    struct page_hash_t* hash;
    if (cache->flags & PAGE_CACHE_4K_PAGES) {
        hash_index = hash_func_local(cache, offset);
        hash = &cache->hash;
    } else {
        hash_index = hash_func(cache, offset);
        hash = &page_hash;
    }

    struct page_desc_t* hash_pages = hash->pages[hash_index];
    struct page_desc_t* page = hash_pages;
    struct page_desc_t* prev = NULL;
    while (page) {
        if (page == page_desc) {
            if (prev)
                prev->next_hash = page->next_hash;
            else
                hash->pages[hash_index] = page->next_hash;

            break;
        }
        prev = page;
        page = page->next_hash;
    }
}

void page_cache_insert(struct page_cache_t* cache,
                       struct page_desc_t* page)
{
    if (cache->pages)
        cache->pages->prev_cache = &page->next_cache;

    page->next_cache = cache->pages;
    page->prev_cache = &cache->pages;

    cache->pages = page;
}

void page_cache_remove(struct page_desc_t* page)
{
    if (page->next_cache)
        page->next_cache->prev_cache = page->prev_cache;
    if (page->prev_cache)
        *page->prev_cache = page->next_cache;

    page->next_cache = NULL;
    page->prev_cache = NULL;
}

void vm_hash_init()
{
    uint32_t page_hash_size = 1;
    while (page_hash_size < page_db->num_pages)
        page_hash_size <<= 1;

    printf("vm_hash_init(): page_hash_size: %d (%d bytes)\n",
        page_hash_size, page_hash_size * (uint32_t)sizeof(uintptr_t));

    page_hash.size = page_hash_size;
    page_hash.pages = (struct page_desc_t**)kernel_slack_alloc(page_hash_size * sizeof(uintptr_t), 16);
    for (uint32_t i = 0; i < page_hash_size; ++i)
        page_hash.pages[i] = NULL;
}

void vm_cache_init()
{
    sl_page_cache = kmalloc_get_slab(sizeof(struct page_cache_t));
}

static struct page_cache_t* page_cache_alloc()
{
    return (struct page_cache_t*)slab_list_alloc(sl_page_cache);
}

static void page_cache_free(struct page_cache_t* cache)
{
    slab_list_free(sl_page_cache, cache);
}

struct page_cache_t* vm_cache_create(uint64_t size, uint32_t flags)
{
    struct page_cache_t* cache = page_cache_alloc();
    cache->pages = NULL;
    cache->hash.pages = NULL;
    cache->hash.size = 0;
    cache->read_page = NULL;
    cache->write_page = NULL;
    cache->size = size;
    cache->flags = flags;
    cache->map_refcnt = 0;

    if ((flags & PAGE_CACHE_4K_PAGES) && !(flags & PAGE_CACHE_NO_HASH)) {
        // init local hash
        cache->hash.size = KMALLOC_CHUNK_SIZE / sizeof(uintptr_t);
        cache->hash.pages = (struct page_desc_t**)kmalloc_alloc();
        for (uint32_t i = 0; i < cache->hash.size; ++i)
            cache->hash.pages[i] = NULL;
    }

    return cache;
}

void vm_cache_release(struct page_cache_t* cache)
{
    --cache->map_refcnt;
    if (cache->map_refcnt <= 0)
        vm_cache_delete(cache);
}

void vm_cache_delete(struct page_cache_t* cache)
{
#if 0
    if (cache->flags & PAGE_CACHE_4K_PAGES) {
        if (cache->hash.pages)
            kmalloc_free((void*)cache->hash.pages);
        struct page_desc_t* page = cache->pages;
        while (page) {
            struct page_desc_t* next = page->next_cache;
            vm_small_page_free(vm_small_page_desc2addr(page));
            page = next;
        }
    } else {
        struct page_desc_t* page = cache->pages;
        while (page) {
            struct page_desc_t* next = page->next_cache;
            page_hash_remove(cache, page->cache_offset, page);
            vm_big_page_free(vm_big_page_desc2addr(page));
            page = next;
        }
    }
#endif
    page_cache_free(cache);
}

void page_hash_dump(struct page_hash_t* hash)
{
    if (!hash)
        hash = &page_hash;

    printf("page_hash_dump(): size: %d\n", hash->size);
    for (uint32_t i = 0; i < hash->size; ++i) {
        struct page_desc_t* page = hash->pages[i];
        if (!page)
            continue;
        printf("[%d]\n", i);
        while (page) {
            printf("\tpage: cache[%p:%llu]\n", page->cache, page->cache_offset);
            page = page->next_hash;
        }
    }
}

void page_cache_dump(struct page_cache_t* cache)
{
    printf("page_cache_dump(): cache %p\n", cache);
    struct page_desc_t* page = cache->pages;
    while (page) {
        printf("\tpage: cache[%p:%llu]\n", page->cache, page->cache_offset);
        page = page->next_cache;
    }
}
