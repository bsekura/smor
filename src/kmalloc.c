#include "kmalloc.h"
#include "vm_page.h"
#include "spinlock.h"
#include "stdio.h"

// 16, 32, 64, 128, 256, 512
//  4,  5,  6,   7,   8,   9
#define MAX_SL_TABLE    6
#define MIN_SL_SHIFT    4
#define MAX_SL_SHIFT    9

static struct spinlock_t sl_lock;
static struct slab_list_t sl_root;
static struct slab_list_t sl_table[MAX_SL_TABLE];

void kmalloc_init()
{
    slab_list_init(&sl_root, NULL, PAGE_2M_SIZE, KMALLOC_CHUNK_SIZE);
    slab_list_reserve_on_slack(&sl_root);
}

void* kmalloc_alloc()
{
    return slab_list_alloc(&sl_root);
}

void kmalloc_free(void* ptr)
{
    slab_list_free(&sl_root, ptr);
}

struct slab_list_t* kmalloc_get_slab(uint32_t elem_size)
{
    uint32_t elem_shift = ceil_pow2(elem_size);
    if (elem_shift < MIN_SL_SHIFT || elem_shift > MAX_SL_SHIFT) {
        printf("kmalloc_get_slab(): bad elem_size: [%d:%d]\n",
                elem_size, elem_shift);
        return NULL;
    }

    uint32_t index = elem_shift - MIN_SL_SHIFT;
    printf("kmalloc_get_slab(): [%d:%d] %d\n", elem_size, 1 << elem_shift, index);

    spinlock_lock(&sl_lock);
    struct slab_list_t* sl = &sl_table[index];
    if (!sl->sl_owner)
        slab_list_init(sl, &sl_root, KMALLOC_CHUNK_SIZE, 1 << elem_shift);
    
    spinlock_unlock(&sl_lock);
    return sl;
}

