#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H

#include "types.h"

#define MEMSIZE_1K  (1ULL << 10ULL)
#define MEMSIZE_1M  (1ULL << 20ULL)
#define MEMSIZE_1G  (1ULL << 30ULL)
#define MEMSIZE_1T  (1ULL << 40ULL)

#define KERNEL_BASE         (0xFFFFFFFF80000000)
#define KERNEL_VADDR(x)     ((uintptr_t)(x)|KERNEL_BASE)

struct boot_info_mmap_t {
    uint64_t addr;
    uint64_t size;
    uint32_t flags;
};

#define BOOT_INFO_MMAP_MAX  32

struct boot_info_t {
    uint32_t mb_magic;
    uint32_t mb_addr;
    uint32_t mb_size;
    uint32_t boot_ap;
    // kernel slack
    uint64_t kernel_end;
    uint64_t kernel_top;
    uint64_t kernel_slack;
    // filled by multiboot_init
    struct boot_info_mmap_t mmap[BOOT_INFO_MMAP_MAX];
    uint32_t num_mmap;
    uint64_t mmap_top;
    uint64_t memory_size;
    uint64_t fb_addr;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_bpp;
    uint32_t fb_type;
    char cmd_line[];
};

static inline struct boot_info_t* kernel_boot_info()
{
    return (struct boot_info_t*)(KERNEL_BASE|0x0A000);
}

void kernel_panic(const char* msg);

#define check(x) \
    if (!(x)) \
        kernel_panic(#x);

void kernel_init(void);
uintptr_t kernel_slack_alloc(uint64_t size, uint64_t align);

#endif // KERNEL_KERNEL_H
