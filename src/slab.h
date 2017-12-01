#ifndef KERNEL_SLAB_H
#define KERNEL_SLAB_H

#include "types.h"
#include "spinlock.h"

struct slab_t;

struct slab_list_t {
    struct slab_list_t* sl_owner;
    struct slab_t* free_slabs;
    struct slab_t* used_slabs;
    uint32_t size_shift;
    uint32_t chunk_shift;
    uint32_t flags;
    struct spinlock_t lock;
};

bool slab_list_init(struct slab_list_t* sl,
                    struct slab_list_t* sl_owner,
                    uint32_t size,
                    uint32_t chunk_size);
bool slab_list_reserve_on_slack(struct slab_list_t* sl);
void* slab_list_alloc(struct slab_list_t* sl);
void slab_list_free(struct slab_list_t* sl, void* addr);
void slab_list_dump(struct slab_list_t* sl);

static inline uint32_t ceil_pow2(uint32_t x)
{
    return 32 - __builtin_clz(x - 1);
}

#endif // KERNEL_SLAB_H
