#include "slab.h"
#include "types.h"
#include "kernel.h"
#include "stdio.h"
#include "spinlock.h"

//#define TRACE_ENABLED
#include "trace.h"

static uintptr_t sys_alloc_page(uint32_t size)
{
    // TODO: rip from page db once setup
    return 0;
}

static void sys_free_page(uintptr_t page)
{
}

#define SLAB_RESERVED   0x1

struct slab_t {
    struct slab_t* next;
    struct slab_t** prev;
    uint16_t chunk_shift;
    uint16_t num_chunks;
    uint16_t num_free;
    uint16_t num_reserved;
    uint16_t flags;
    uint16_t free_list;
    uint16_t chunk_list[];
};

static struct slab_t* slab_init(uintptr_t addr, uint32_t size, uint32_t chunk_size)
{
    uint32_t chunk_shift = ceil_pow2(chunk_size);
    uint32_t chunk_size_pow2 = 1 << chunk_shift;
    uint32_t num_chunks = size >> chunk_shift;
    uint32_t cache_size = sizeof(struct slab_t) + num_chunks * sizeof(uint16_t);
    uint32_t num_reserved = ((cache_size + (chunk_size_pow2-1)) & ~(chunk_size_pow2-1))
                          >> chunk_shift;

    trace("slab_init(): %016lx\n", addr);
    trace("slab_init(): chunk_size: %d -> [%d:%d], size %d\n",
            chunk_size, chunk_size_pow2, chunk_shift, size);
    trace("slab_init(): num,rsvd,free (%d,%d,%d), cache_size %d\n",
            num_chunks, num_reserved, num_chunks - num_reserved, cache_size);

    struct slab_t* slab = (struct slab_t*)addr;
    slab->next = NULL;
    slab->prev = NULL;
    slab->chunk_shift = (uint16_t)chunk_shift;
    slab->num_chunks = (uint16_t)num_chunks;
    slab->num_free = (uint16_t)(num_chunks - num_reserved);
    slab->num_reserved = (uint16_t)num_reserved;
    slab->flags = 0;

    uint32_t i;
    for (i = 0; i < num_reserved; ++i)
        slab->chunk_list[i] = 0;
    for (i = num_reserved; i < num_chunks-1; ++i)
        slab->chunk_list[i] = (uint16_t)(i+1);

    slab->chunk_list[i] = 0;
    slab->free_list = slab->num_reserved;

    return slab;
}

static void* slab_alloc(struct slab_t* slab)
{
    uint32_t index = slab->free_list;
    if (!index)
        return NULL;

    slab->free_list = slab->chunk_list[index];
    --slab->num_free;

    trace("slab_alloc(): %d\n", index);
    uintptr_t base = (uintptr_t)slab;
    return (void*)(base + (index << slab->chunk_shift));
}

// addr assumed checked
static void slab_free(struct slab_t* slab, void* addr)
{
    uintptr_t base = (uintptr_t)slab;
    uint32_t index = (uint32_t)((uintptr_t)addr - base) >> slab->chunk_shift;
    trace("slab_free(): %d\n", index);
    slab->chunk_list[index] = slab->free_list;
    slab->free_list = (uint16_t)index;
    ++slab->num_free;
}

static inline bool slab_is_empty(struct slab_t* slab)
{
    return slab->num_free == slab->num_chunks - slab->num_reserved;
}

static inline bool slab_is_full(struct slab_t* slab)
{
    return slab->num_free == 0;
}

static inline bool slab_is_reserved(struct slab_t* slab)
{
    return slab->flags & SLAB_RESERVED;
}

static inline void slab_list_lock(struct slab_list_t* sl)
{
    spinlock_lock(&sl->lock);
}

static inline void slab_list_unlock(struct slab_list_t* sl)
{
    spinlock_unlock(&sl->lock);
}

static inline void slab_list_insert(struct slab_t** list, struct slab_t* s)
{
    if (*list)
        (*list)->prev = &s->next;

    s->next = *list;
    s->prev = list;

    *list = s;
}

static inline void slab_list_remove(struct slab_t* s)
{
    if (s->next)
        s->next->prev = s->prev;
    if (s->prev)
        *s->prev = s->next;

    s->next = NULL;
    s->prev = NULL;
}

bool slab_list_init(struct slab_list_t* sl,
                    struct slab_list_t* sl_owner,
                    uint32_t size,
                    uint32_t chunk_size)
{
    uint32_t size_shift = ceil_pow2(size);
    if (sl_owner && sl_owner->chunk_shift != size_shift) {
        printf("slab_list_init(): sl_owner has bad chunk size %d (expected %d)\n",
                (1 << sl_owner->chunk_shift), (1 << size_shift));
        return false;
    }

    uint32_t chunk_shift = ceil_pow2(chunk_size);
    trace("slab_list_init(): size [%d:%d], chunk_size [%d:%d]\n",
            size, size_shift, chunk_size, chunk_shift);

    sl->sl_owner = sl_owner;
    sl->free_slabs = NULL;
    sl->used_slabs = NULL;
    sl->size_shift = size_shift;
    sl->chunk_shift = chunk_shift;
    sl->flags = 0;
    sl->lock.counter = 0;

    return true;
}

bool slab_list_reserve_on_slack(struct slab_list_t* sl)
{
    uint32_t size = 1 << sl->size_shift;
    uintptr_t p;
    if (sl->sl_owner)
        p = (uintptr_t)slab_list_alloc(sl->sl_owner);
    else {
        p = kernel_slack_alloc(size, size);
    }

    if (p) {
        uint32_t chunk_size = 1 << sl->chunk_shift;
        struct slab_t* s = slab_init(p, size, chunk_size);
        if (s) {
            s->flags |= SLAB_RESERVED;
            slab_list_insert(&sl->free_slabs, s);
            return true;
        }
    }
    
    return false;
}

void* slab_list_alloc(struct slab_list_t* sl)
{
    slab_list_lock(sl);
    struct slab_t* s = sl->free_slabs;
    if (s) {
        void* addr = slab_alloc(s);
        if (slab_is_full(s)) {
            slab_list_remove(s);
            slab_list_insert(&sl->used_slabs, s);
        }
        slab_list_unlock(sl);
        return addr;
    }

    // use hard lock (semaphore, mutex?)

    uint32_t size = 1 << sl->size_shift;
    uintptr_t p;
    if (sl->sl_owner)
        p = (uintptr_t)slab_list_alloc(sl->sl_owner);
    else
        p = sys_alloc_page(size);

    if (p) {
        uint32_t chunk_size = 1 << sl->chunk_shift;
        s = slab_init(p, size, chunk_size);
        if (s) {
            slab_list_insert(&sl->free_slabs, s);
            void* addr = slab_alloc(s);
            slab_list_unlock(sl);
            return addr;
        }
    }

    slab_list_unlock(sl);
    return NULL;
}

void slab_list_free(struct slab_list_t* sl, void* addr)
{
    uintptr_t slab_addr = (uintptr_t)addr & ~((1UL << sl->size_shift) - 1);
    struct slab_t* s = (struct slab_t*)slab_addr;
    slab_list_lock(sl);
    bool was_full = slab_is_full(s);
    slab_free(s, addr);
    if (was_full) {
        slab_list_remove(s);
        slab_list_insert(&sl->free_slabs, s);
    } else if (slab_is_empty(s) && !slab_is_reserved(s)) {
        slab_list_remove(s);
        if (sl->sl_owner)
            slab_list_free(sl->sl_owner, s);
        else
            sys_free_page(slab_addr);
    }
    slab_list_unlock(sl);
}

void slab_list_dump(struct slab_list_t* sl)
{
    printf("slab_list: %016lx\n", (uintptr_t)sl);
    printf("\tsl_owner: %016lx\n", (uintptr_t)sl->sl_owner);
    printf("\tchunk_size: %08x\n", 1 << sl->chunk_shift);
    printf("\t      size: %08x\n", 1 << sl->size_shift);
    printf("\tfree_slabs:\n");
    struct slab_t* s = sl->free_slabs;
    while (s) {
        printf("\t\t%016lx num_free %d\n", (uintptr_t)s, s->num_free);
        s = s->next;
    }
    printf("\tused_slabs:\n");
    s = sl->used_slabs;
    while (s) {
        printf("\t\t%016lx num_free %d\n", (uintptr_t)s, s->num_free);
        s = s->next;
    }
}
