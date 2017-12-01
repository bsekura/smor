#include "acpi.h"
#include "io.h"
#include "kernel.h"
#include "stdio.h"
#include "string.h"
#include "io_apic.h"
#include "local_apic.h"
#include "vm_boot.h"

//#define TRACE_ENABLED
#include "trace.h"

// root system description pointer
#define ACPI_RSDP_SIGNATURE 0x2052545020445352L

struct acpi_rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
} PACKED;

struct acpi_rsdp_v2_t {
    struct acpi_rsdp_t v1;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} PACKED;

// system description table
#define ACPI_APIC_SIGNATURE 0x43495041
#define ACPI_FACP_SIGNATURE 0x50434146

struct acpi_sdt_header_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} PACKED;

// root system description table
struct acpi_rsdt_t {
    struct acpi_sdt_header_t hdr;
    uint32_t sdt_table[];
} PACKED;

struct acpi_xsdt_t {
    struct acpi_sdt_header_t hdr;
    uint64_t sdt_table[];
} PACKED;

// multiple apic description table
struct acpi_madt_t {
    struct acpi_sdt_header_t hdr;
    uint32_t local_ctrl_addr;
    uint32_t flags;
} PACKED;

// fixed acpi description table
struct acpi_fadt_t {
    struct acpi_sdt_header_t hdr;
} PACKED;

// secondary system description table
struct acpi_ssdt_t {
    struct acpi_sdt_header_t hdr;
} PACKED;

#define APIC_TYPE_LOCAL_APIC                0x00
#define APIC_TYPE_IO_APIC                   0x01
#define APIC_TYPE_INTERRUPT_OVERRIDE        0x02
#define APIC_TYPE_NMI_SOURCE                0x03
#define APIC_TYPE_LOCAL_APIC_NMI            0x04
#define APIC_TYPE_LOCAL_APIC_ADDR_OVERRIDE  0x05
#define APIC_TYPE_IO_SAPIC                  0x06
#define APIC_TYPE_LOCAL_SAPIC               0x07
#define APIC_TYPE_PLATFORM_INT_SOURCE       0x08
#define APIC_TYPE_LOCAL_X2_APIC             0x09
#define APIC_TYPE_LOCAL_X2_APIC_NMI         0xA0

struct apic_header_t {
    uint8_t type;
    uint8_t length;
} PACKED;

struct apic_local_apic_t {
    struct apic_header_t hdr;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} PACKED;

struct apic_io_apic_t {
    struct apic_header_t hdr;
    uint8_t apic_id;
    uint8_t reserved;
    uint32_t apic_addr;
    uint32_t gsi_base;
} PACKED;

struct apic_interrupt_override_t {
    struct apic_header_t hdr;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} PACKED;

#ifdef TRACE_ENABLED
static const char* cstr(const char* s, size_t sz)
{
    static char tmp[16];
    return c_strncpy(tmp, s, sz);
}
#endif // TRACE_ENABLED

static struct acpi_rsdp_t* acpi_find_rsdp()
{
    const uint8_t* ptr = (uint8_t*)(KERNEL_BASE|0x000e0000);
    const uint8_t* end = (uint8_t*)(KERNEL_BASE|0x000fffff);

    while (ptr < end) {
        uint64_t signature = *(uint64_t*)ptr;
        if (signature == ACPI_RSDP_SIGNATURE) {
            uint8_t checksum = 0;
            unsigned int sz = sizeof(struct acpi_rsdp_t);
            for (unsigned int i = 0; i < sz; ++i) 
                checksum += ptr[i];

            if (checksum == 0)
                return (struct acpi_rsdp_t*)ptr;

            trace("acpi rsdt checksum failed\n");
            break;
        }   
        ptr += 16;
    }

    return NULL;
}

static bool acpi_sdt_check(const struct acpi_sdt_header_t* hdr)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < hdr->length; ++i)
        checksum += ((uint8_t*)hdr)[i];

    return (checksum == 0);
}

static void acpi_parse_madt(struct acpi_madt_t* madt)
{
    local_apic_register((uintptr_t)madt->local_ctrl_addr);
    trace("apic: %016lx %08x\n", (uintptr_t)madt->local_ctrl_addr, madt->flags);

    uint8_t* ptr = (uint8_t*)(madt+1);
    uint8_t* end = (uint8_t*)madt + madt->hdr.length;

    while (ptr < end) {
        struct apic_header_t* apic = (struct apic_header_t*)ptr;
        if (apic->type == APIC_TYPE_LOCAL_APIC) {
            struct apic_local_apic_t* local_apic = (struct apic_local_apic_t*)ptr;
            trace("local_apic: processor_id %d apic_id %d flags %08x\n",
                    (int)local_apic->processor_id, (int)local_apic->apic_id, local_apic->flags);

            local_apic_add_cpu(local_apic->apic_id);

        } else if (apic->type == APIC_TYPE_IO_APIC) {
            struct apic_io_apic_t* io_apic = (struct apic_io_apic_t*)ptr;
            trace("io_apic: apic_id %d apic_addr %08lx\n",
                    (int)io_apic->apic_id, (uintptr_t)io_apic->apic_addr);

            io_apic_register((uintptr_t)io_apic->apic_addr, io_apic->gsi_base);

        } else if (apic->type == APIC_TYPE_INTERRUPT_OVERRIDE) {
            struct apic_interrupt_override_t* ior
                = (struct apic_interrupt_override_t*)ptr;
            trace("interrupt_override: bus_source %d irq_source %d gsi %d\n",
                    ior->bus_source, ior->irq_source, ior->gsi);

            io_apic_add_iovr(ior->bus_source, ior->irq_source, ior->gsi);
        }

        ptr += apic->length;
    }
}

static void acpi_parse_sdt(struct acpi_sdt_header_t* sdt)
{
    trace("%016lx %s\n", (uintptr_t)sdt, cstr(sdt->signature, sizeof(sdt->signature)));

    uint32_t signature = *(uint32_t*)sdt->signature;
    if (signature == ACPI_APIC_SIGNATURE)
        acpi_parse_madt((struct acpi_madt_t*)sdt);
}

static void acpi_parse_rsdt(struct acpi_rsdt_t* rsdt)
{
    unsigned int num_sdt = (rsdt->hdr.length - sizeof(rsdt->hdr)) >> 2;
    for (unsigned int i = 0; i < num_sdt; ++i) {
        uintptr_t addr = (uintptr_t)rsdt->sdt_table[i];
        acpi_parse_sdt((struct acpi_sdt_header_t*)addr);
    }
}

static void acpi_parse_xsdt(struct acpi_xsdt_t* xsdt)
{
    unsigned int num_sdt = (xsdt->hdr.length - sizeof(xsdt->hdr)) >> 3;
    for (unsigned int i = 0; i < num_sdt; ++i) {
        uintptr_t addr = (uintptr_t)xsdt->sdt_table[i];
        acpi_parse_sdt((struct acpi_sdt_header_t*)addr);
    }
}

bool acpi_init()
{
    struct acpi_rsdp_t* rsdp = acpi_find_rsdp();
    if (!rsdp)
        return false;

    trace("acpi rsdp: \"%s\" revision: %d rsdt_addr: %016lx\n",
        cstr(rsdp->oem_id, sizeof(rsdp->oem_id)), (int)rsdp->revision, (uintptr_t)rsdp->rsdt_addr);

    if (rsdp->revision == 0) {
        uintptr_t rsdt_addr = (uintptr_t)rsdp->rsdt_addr;
        trace("rsdt_addr: %016lx\n", rsdt_addr);
        vm_boot_map_range(rsdt_addr, rsdt_addr, sizeof(struct acpi_rsdt_t));

        struct acpi_rsdt_t* rsdt = (struct acpi_rsdt_t*)rsdt_addr;
        vm_boot_map_range(rsdt_addr, rsdt_addr, rsdt->hdr.length);
        if (acpi_sdt_check((struct acpi_sdt_header_t*)rsdt))
            acpi_parse_rsdt(rsdt);

        vm_boot_unmap_range(rsdt_addr, rsdt->hdr.length);
    } else if (rsdp->revision == 2) {
        struct acpi_rsdp_v2_t* rsdp_v2 = (struct acpi_rsdp_v2_t*) rsdp;
        uint8_t checksum = 0;
        for (uint32_t i = 0; i < rsdp_v2->length; ++i)
            checksum += ((uint8_t*)rsdp_v2)[i];

        if (checksum == 0) {
            uintptr_t xsdt_addr = (uintptr_t)rsdp_v2->xsdt_addr;
            trace("xsdt_addr: %016lx\n", xsdt_addr);
            vm_boot_map_range(xsdt_addr, xsdt_addr, sizeof(struct acpi_xsdt_t));

            struct acpi_xsdt_t* xsdt = (struct acpi_xsdt_t*)xsdt_addr;
            vm_boot_map_range(xsdt_addr, xsdt_addr, xsdt->hdr.length);
            if (acpi_sdt_check((struct acpi_sdt_header_t*)xsdt))
                acpi_parse_xsdt(xsdt);

            vm_boot_unmap_range(xsdt_addr, xsdt->hdr.length);
        } else
            printf("rsdp_v2 checksum failed\n");
    }

    return true;
}