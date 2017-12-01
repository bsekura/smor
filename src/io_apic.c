#include "io_apic.h"
#include "kernel.h"
#include "cpu.h"
#include "vm_boot.h"
#include "stdio.h"

// io apic registers
#define IOAPICID        0x00    // RW   IOAPIC ID
#define IOAPICVER       0x01    // R    IOAPIC Version
#define IOAPICARB       0x02    // R    IOAPIC Arbitration ID
#define IOREDTBL        0x10    // RW   Redirection Table (Entries 0-23) (64 bits each)

// (rw) io apic id register
#define IOAPICID_SHIFT              24  // 27:24 IOAPIC identification
#define IOAPICID_MASK               0xf
// (ro) io apic version register
#define IOAPICVER_MAX_REDTBL_SHIFT  16  // 23:16 number of interrupt input pins minus one
#define IOAPICVER_MAX_REDTBL_MASK   0xff
#define IOAPICVER_VERSION_MASK      0xff

// redirection table entry

// (rw) destination field
#define IOREDTBL_DEST_SHIFT         56UL
#define IOREDTBL_DEST_MASK_PHYSICAL 0x0f00000000000000  // 56:59 apic_id
#define IOREDTBL_DEST_MASK_LOGICAL  0xff00000000000000  // 56:63 set of processors
// (rw) interrupt mask 1 - interrupt is masked, 0 - unmasked
#define IOREDTBL_INTERRUPT_ON       0x0
#define IOREDTBL_INTERRUPT_OFF      (1UL<<16)
// (rw) trigger mode 1 = level sensitive, 0 = edge sensitive
#define IOREDTBL_TRIGGER_EDGE       0x0
#define IOREDTBL_TRIGGER_LEVEL      (1UL<<15)
// (ro)
#define IOREDTBL_REMOTE_IRR         (1UL<<14)
// (rw) interrupt polarity 0 = high active, 1 = low active
#define IOREDTBL_INTPOL_HIGH_ACTIVE 0x0
#define IOREDTBL_INTPOL_LOW_ACTIVE  (1UL<<13)
// (ro) delivery status 0 = idle, 1 = send pending
#define IOREDTBL_DELIVS_IDLE        0x0
#define IOREDTBL_DELIVS_SEND_PEND   (1UL<<12)
// (rw) destination mode 0 = physical mode, 1 = logical mode
#define IOREDTBL_DESTMOD_PHYSICAL   0x0
#define IOREDTBL_DESTMOD_LOGICAL    (1UL<<11)
// (rw) delivery mode
#define IOREDTBL_DELMOD_FIXED       0x000UL
#define IOREDTBL_DELMOD_LOWEST_PRI  0x100UL
#define IOREDTBL_DELMOD_SMI         0x200UL
#define IOREDTBL_DELMOD_NMI         0x400UL
#define IOREDTBL_DELMOD_INIT        0x500UL
#define IOREDTBL_DELMOD_EXINT       0x700UL
// (rw) interrupt vector 0:7 range from 10h to FEh
#define IOREDTBL_INTVEC_MASK        0xffUL

struct io_apic_t io_apic;
struct io_apic_iovr_t io_apic_iovr[MAX_IO_APIC_IOVR];
uint32_t io_apic_num_iovr;

static uint32_t io_apic_read(uint32_t reg)
{
    volatile uint32_t* addr = (volatile uint32_t*)io_apic.io_apic_addr;
    addr[0] = reg & 0xff;   // IOREGSEL 0x00
    return addr[4];         // IOWIN    0x10
}
 
static void io_apic_write(uint32_t reg, uint32_t data)
{
    volatile uint32_t* addr = (volatile uint32_t*)io_apic.io_apic_addr;
    addr[0] = reg & 0xff;   // IOREGSEL 0x00
    addr[4] = data;         // IOWIN    0x10
}

void io_apic_register(uintptr_t addr, uint32_t gsi_base)
{
    io_apic.io_apic_addr = (uint8_t*)addr;
    io_apic.gsi_base = gsi_base;
}

void io_apic_add_iovr(uint8_t bus, uint8_t irq, uint32_t gsi)
{
    check(io_apic_num_iovr < MAX_IO_APIC_IOVR);
    struct io_apic_iovr_t* iovr = &io_apic_iovr[io_apic_num_iovr++];
    iovr->bus = bus;
    iovr->irq = irq;
    iovr->gsi = gsi;
}

static uint64_t io_apic_redtbl_make(uint64_t flags, uint8_t intvec, uint8_t cpu_mask)
{
    return (((uint64_t)cpu_mask << IOREDTBL_DEST_SHIFT) & IOREDTBL_DEST_MASK_LOGICAL)
            | ((uint64_t)intvec & IOREDTBL_INTVEC_MASK)
            | flags;
}

static uint64_t io_apic_redtbl_get(uint32_t index)
{
    uint32_t lo = io_apic_read(IOREDTBL + index * 2 + 0);
    uint32_t hi = io_apic_read(IOREDTBL + index * 2 + 1);
    return (uint64_t)lo + ((uint64_t)hi << 32);
}

static void io_apic_redtbl_set(uint32_t index, uint64_t data)
{
    io_apic_write(IOREDTBL + index * 2 + 0, (uint32_t)data);
    io_apic_write(IOREDTBL + index * 2 + 1, (uint32_t)(data >> 32));
}

static uint32_t io_apic_interrupt_override(uint8_t irq)
{
    for (uint32_t i = 0; i < io_apic_num_iovr; ++i) {
        struct io_apic_iovr_t* iovr = &io_apic_iovr[i];
        if (iovr->irq == irq)
            return iovr->gsi;
    }

    return irq;
}

void io_apic_enable_irq(uint8_t irq, uint8_t vector, uint8_t cpu_mask)
{
    uint32_t index = io_apic_interrupt_override(irq);
    uint64_t entry = ((uint64_t)vector & IOREDTBL_INTVEC_MASK)
                   | IOREDTBL_INTERRUPT_ON
                   | IOREDTBL_TRIGGER_EDGE
                   | IOREDTBL_INTPOL_HIGH_ACTIVE
                   | IOREDTBL_DELMOD_FIXED
                   | IOREDTBL_DESTMOD_LOGICAL
                   | (((uint64_t)cpu_mask << IOREDTBL_DEST_SHIFT) & IOREDTBL_DEST_MASK_LOGICAL);

    int spl = cpu_splhi();
    spinlock_lock(&io_apic.lock);
    io_apic_redtbl_set(index, entry);
    spinlock_unlock(&io_apic.lock);
    cpu_splx(spl);
}

void io_apic_disable_irq(uint8_t irq)
{
    uint32_t index = io_apic_interrupt_override(irq);

    int spl = cpu_splhi();
    spinlock_lock(&io_apic.lock);
    uint64_t entry = io_apic_redtbl_get(index);
    io_apic_redtbl_set(index, entry | IOREDTBL_INTERRUPT_OFF);
    spinlock_unlock(&io_apic.lock);
    cpu_splx(spl);
}

void io_apic_init()
{
    vm_boot_map_range((uintptr_t)io_apic.io_apic_addr,
                      (uintptr_t)io_apic.io_apic_addr,
                      0x1000);

    io_apic.id = (io_apic_read(IOAPICID) >> IOAPICID_SHIFT) & IOAPICID_MASK;
    uint32_t ver_reg = io_apic_read(IOAPICVER);
    io_apic.ver = ver_reg & IOAPICVER_VERSION_MASK;
    io_apic.num_redtbl
        = ((ver_reg >> IOAPICVER_MAX_REDTBL_SHIFT) & IOAPICVER_MAX_REDTBL_MASK) + 1;

    //printf("io_apic: %016lx id %d ver %d num_redtbl %d\n",
    //    (uintptr_t)io_apic.io_apic_addr, id, ver, num_redtbl);

    // mask all interrupts
    for (uint32_t i = 0; i < io_apic.num_redtbl; ++i) {
        uint64_t entry = IOREDTBL_INTERRUPT_OFF;
        io_apic_redtbl_set(i, entry);
    }
}
