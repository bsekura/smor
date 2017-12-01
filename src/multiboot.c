#include "multiboot.h"
#include "types.h"
#include "kernel.h"
#include "stdio.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

#define MULTIBOOT_TAG_TYPE_END                  0
#define MULTIBOOT_TAG_TYPE_CMDLINE              1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME     2
#define MULTIBOOT_TAG_TYPE_MODULE               3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO        4
#define MULTIBOOT_TAG_TYPE_BOOTDEV              5
#define MULTIBOOT_TAG_TYPE_MMAP                 6
#define MULTIBOOT_TAG_TYPE_VBE                  7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER          8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS         9
#define MULTIBOOT_TAG_TYPE_APM                  10

struct multiboot_tag_t {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_string_t {
    uint32_t type;
    uint32_t size;
    char string[0];
};

struct multiboot_tag_basic_meminfo_t {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

struct multiboot_tag_framebuffer_t {
    uint32_t type;
    uint32_t size;
    uint64_t fb_addr;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint8_t fb_bpp;
    uint8_t fb_type;
    uint8_t reserved;
};

#define MULTIBOOT_MEMORY_AVAILABLE          1
#define MULTIBOOT_MEMORY_RESERVED           2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE   3
#define MULTIBOOT_MEMORY_NVS                4

struct multiboot_mmap_entry_t {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap_t {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry_t entries[0];
};

static void multiboot_show_fb(struct multiboot_tag_framebuffer_t* fb_tag)
{
    printf("fb: %016lx [%d:%d:%d] pitch %d type %d\n",
        fb_tag->fb_addr, fb_tag->fb_width, fb_tag->fb_height, (uint32_t)fb_tag->fb_bpp,
        fb_tag->fb_pitch, (uint32_t)fb_tag->fb_type);
}

static void multiboot_show_mmap(struct multiboot_tag_mmap_t* mmap)
{
    struct multiboot_mmap_entry_t* e;
    for (e = mmap->entries;
            (uint8_t*)e < (uint8_t*)mmap + mmap->size;
            e = (struct multiboot_mmap_entry_t*)((uintptr_t)e + mmap->entry_size)) {
        printf("%16lx:%16lx %d\n", e->addr, e->len, e->type);
    }
}

// debug
void multiboot_show()
{
    struct boot_info_t* boot_info = kernel_boot_info();
    if (boot_info->mb_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        uint32_t mb_size = *(uint32_t*)KERNEL_VADDR(boot_info->mb_addr);
        printf("mb_size: %08x\n", mb_size);
        struct multiboot_tag_t* tag;
        for (tag = (struct multiboot_tag_t*)KERNEL_VADDR(boot_info->mb_addr + 8);
             tag->type != MULTIBOOT_TAG_TYPE_END;
             tag = (struct multiboot_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {

            printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);

            switch (tag->type) {
                case MULTIBOOT_TAG_TYPE_CMDLINE:
                    printf("cmd_line: %s\n", ((struct multiboot_tag_string_t*)tag)->string);
                    break;
                case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                    printf("bootloader: %s\n", ((struct multiboot_tag_string_t*)tag)->string);
                    break;
                case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                    printf("basic meminfo: %uKB %uKB\n",
                        ((struct multiboot_tag_basic_meminfo_t*)tag)->mem_lower,
                        ((struct multiboot_tag_basic_meminfo_t*)tag)->mem_upper);
                    break;
                case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                    multiboot_show_fb((struct multiboot_tag_framebuffer_t*)tag);
                    break;
                case MULTIBOOT_TAG_TYPE_MMAP:
                    multiboot_show_mmap((struct multiboot_tag_mmap_t*)tag);
                    break;
            }
        }
    }    
}

static void multiboot_cmdline(struct multiboot_tag_string_t* str)
{
    struct boot_info_t* boot_info = kernel_boot_info();
    char* p = boot_info->cmd_line;
    const char* s = str->string;
    while (*s) 
        *p++ = *s++;
    *p = 0;
}

static void multiboot_framebuffer(struct multiboot_tag_framebuffer_t* fb)
{
    struct boot_info_t* boot_info = kernel_boot_info();
    boot_info->fb_addr = fb->fb_addr;
    boot_info->fb_pitch = fb->fb_pitch;
    boot_info->fb_width = fb->fb_width;
    boot_info->fb_height = fb->fb_height;
    boot_info->fb_bpp = fb->fb_bpp;
    boot_info->fb_type = fb->fb_type;
}

static void multiboot_meminfo(struct multiboot_tag_basic_meminfo_t* meminfo)
{
    uint64_t mem_lower = (uint64_t)meminfo->mem_lower * 1024UL;
    uint64_t mem_upper = (uint64_t)meminfo->mem_upper * 1024UL;
    struct boot_info_t* boot_info = kernel_boot_info();
    boot_info->memory_size = mem_lower + mem_upper;
}

static void multiboot_mmap(struct multiboot_tag_mmap_t* mmap)
{
    struct boot_info_t* boot_info = kernel_boot_info();
    boot_info->num_mmap = 0;
    boot_info->mmap_top = 0;
    struct multiboot_mmap_entry_t* e;
    for (e = mmap->entries;
            (uint8_t*)e < (uint8_t*)mmap + mmap->size;
            e = (struct multiboot_mmap_entry_t*)((uintptr_t)e + mmap->entry_size)) {
        check(boot_info->num_mmap < BOOT_INFO_MMAP_MAX);
        struct boot_info_mmap_t* m = &boot_info->mmap[boot_info->num_mmap++];
        m->addr = e->addr;
        m->size = e->len;
        if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
            m->flags = 1;
            if (e->addr + e->len > boot_info->mmap_top)
                boot_info->mmap_top = e->addr + e->len;
        } else
            m->flags = 0;
    }
}

bool multiboot_init(void)
{
    struct boot_info_t* boot_info = kernel_boot_info();
    if (boot_info->mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC)
        return false;
    
    struct multiboot_tag_t* tag;
    for (tag = (struct multiboot_tag_t*)KERNEL_VADDR(boot_info->mb_addr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                multiboot_cmdline((struct multiboot_tag_string_t*)tag);
                break;
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                multiboot_meminfo((struct multiboot_tag_basic_meminfo_t*)tag);
                break;
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                multiboot_framebuffer((struct multiboot_tag_framebuffer_t*)tag);
                break;
            case MULTIBOOT_TAG_TYPE_MMAP:
                multiboot_mmap((struct multiboot_tag_mmap_t*)tag);
                break;
        }
    }

    return true;
}
