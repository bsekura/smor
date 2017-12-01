#include "imps.h"
#include "kernel.h"
#include "string.h"
#include "stdio.h"

#define IMPS_FPS_SIGNATURE  ('_' | ('M'<<8) | ('P'<<16) | ('_'<<24))
#define IMPS_CTH_SIGNATURE  ('P' | ('C'<<8) | ('M'<<16) | ('P'<<24))

// floating pointer structure
struct imps_fps_t {
    uint32_t signature;
    uint32_t config_table_addr;
    uint8_t length;
    uint8_t spec_rev;
    uint8_t checksum;
    uint8_t features[2];
    uint8_t reserved[3];
} PACKED;

#define IMPS_CTH_ENTRY_PROCESSOR        0
#define IMPS_CTH_ENTRY_BUS              1
#define IMPS_CTH_ENTRY_IO_APIC          2
#define IMPS_CTH_ENTRY_IO_INTERRUPT     3
#define IMPS_CTH_ENTRY_LOCAL_INTERRUPT  4

// config table header
struct imps_cth_t {
    uint32_t signature;
    uint16_t base_table_len;
    uint8_t spec_rev;
    uint8_t checksum;
    char oem_id[8];
    char product_id[12];
    uint32_t oem_table_ptr;
    uint16_t oem_table_size;
    uint16_t entry_count;
    uint32_t lapic_addr;
    uint16_t ext_table_len;
    uint8_t ext_table_checksum;
    uint8_t reserved;
} PACKED;

struct imps_processor_t {
    uint8_t type;
    uint8_t local_apic_id;
    uint8_t local_apic_ver;
    uint8_t cpu_flags;
    uint32_t cpu_signature;
    uint32_t feature_flags;
    uint64_t reserved;
} PACKED;

struct imps_bus_t {
    uint8_t type;
    uint8_t bus_id;
    char bus_type[6];
} PACKED;

struct imps_io_apic_t {
    uint8_t type;
    uint8_t io_apic_id;
    uint8_t io_apic_ver;
    uint8_t io_apic_flags;
    uint32_t io_apic_addr;
} PACKED;

struct imps_io_interrupt_t {
    uint8_t type;
    uint8_t int_type;
    uint16_t int_flags;
    uint8_t src_bus_id;
    uint8_t src_bus_irq;
    uint8_t dst_io_apic_id;
    uint8_t dst_io_apic_intin;
} PACKED;

struct imps_local_interrupt_t {
    uint8_t type;
    uint8_t int_type;
    uint16_t int_flags;
    uint8_t src_bus_id;
    uint8_t src_bus_irq;
    uint8_t dst_lapic_id;
    uint8_t dst_lapic_lintin;
} PACKED;

static uint32_t imps_checksum(const void* base, uint32_t len)
{
    uint32_t sum = 0;
    const uint8_t* p = (uint8_t*)base;
    for (uint32_t i = 0; i < len; ++i)
        sum += p[i];

    return sum & 0xFF;
}

static bool imps_check_fps(struct imps_fps_t* fps)
{
    if (fps->signature == IMPS_FPS_SIGNATURE
        && fps->length == 0x1
        && (fps->spec_rev == 0x1 || fps->spec_rev == 0x4)
        && !imps_checksum(fps, sizeof(*fps)))
        return true;
    
    return false;
}

static struct imps_fps_t* imps_scan(uintptr_t addr, uint32_t len)
{
    while (len > 0) {
        struct imps_fps_t* fps = (struct imps_fps_t*)addr;
        if (imps_check_fps(fps))
            return fps;

        len -= 16;
        addr += 16;
    }

    return NULL;
}

static struct imps_fps_t* imps_find()
{
    struct imps_fps_t* fps;

    uintptr_t ebda_addr = (*(uint16_t*)KERNEL_VADDR(0x40E)) << 4;
    fps = imps_scan(KERNEL_VADDR(ebda_addr), 1024);
    if (fps)
        return fps;

    fps = imps_scan(KERNEL_VADDR(639UL * 1024UL), 1024);
    if (fps)
        return fps;

    fps = imps_scan(KERNEL_VADDR(0xF0000UL), 0x10000);
    if (fps)
        return fps;

    return NULL;
}

static void imps_read_processor(struct imps_processor_t* cpu)
{
    printf("cpu: local_apic_id %d, signature %08x\n",
            cpu->local_apic_id, cpu->cpu_signature);
}

static void imps_read_bus(struct imps_bus_t* bus)
{
    char bus_type[sizeof(bus->bus_type)+1];
    printf("bus: id %d, type: %s\n",
            bus->bus_id, c_strncpy(bus_type, bus->bus_type, sizeof(bus->bus_type)));
}

static void imps_read_io_apic(struct imps_io_apic_t* io_apic)
{
    printf("io_apic: id %d addr: %08x\n",
            io_apic->io_apic_id, io_apic->io_apic_addr);
}

static void imps_read_io_interrupt(struct imps_io_interrupt_t* io_int)
{
    printf("io_interrupt: type %d, src %d:%d dst: %d:%d\n",
        io_int->int_type,
        io_int->src_bus_id, io_int->src_bus_irq,
        io_int->dst_io_apic_id, io_int->dst_io_apic_intin);
}

static void imps_read_local_interrupt(struct imps_local_interrupt_t* local_int)
{
    printf("local_interrupt: type %d, src %d:%d dst: %d:%d\n",
        local_int->int_type,
        local_int->src_bus_id, local_int->src_bus_irq,
        local_int->dst_lapic_id, local_int->dst_lapic_lintin);
}

static void imps_read_config_table(uintptr_t addr)
{
    struct imps_cth_t* cth = (struct imps_cth_t*)addr;

    char oem_id[sizeof(cth->oem_id)+1];
    char product_id[sizeof(cth->product_id)+1];
    printf("oem id: %s product id: %s\n",
        c_strncpy(oem_id, cth->oem_id, sizeof(cth->oem_id)),
        c_strncpy(product_id, cth->product_id, sizeof(cth->product_id)));

    addr += sizeof(struct imps_cth_t);
    uint32_t entry_count = cth->entry_count;
    while (entry_count--) {
        uint8_t entry_type = *((uint8_t*)addr);
        switch (entry_type) {
        case IMPS_CTH_ENTRY_PROCESSOR:
            imps_read_processor((struct imps_processor_t*)addr);
            addr += 12;
            break;
        case IMPS_CTH_ENTRY_BUS:
            imps_read_bus((struct imps_bus_t*)addr);
            break;
        case IMPS_CTH_ENTRY_IO_APIC:
            imps_read_io_apic((struct imps_io_apic_t*)addr);
            break;
        case IMPS_CTH_ENTRY_IO_INTERRUPT:
            imps_read_io_interrupt((struct imps_io_interrupt_t*)addr);
            break;
        case IMPS_CTH_ENTRY_LOCAL_INTERRUPT:
            imps_read_local_interrupt((struct imps_local_interrupt_t*)addr);
            break;
        }

        addr += 8;
    }
}

void imps_init()
{
    struct imps_fps_t* fps = imps_find();
    if (!fps) {
        printf("no MP floating pointer structure found\n");
        return;
    }

    printf("Intel MultiProcessor Spec 1.%d\n", fps->spec_rev);
    if (fps->config_table_addr)
        imps_read_config_table(KERNEL_VADDR(fps->config_table_addr));
}

void imps_show_cmd(int argc, const char* argv[])
{
    imps_init();
}