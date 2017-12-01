#include "local_apic.h"
#include "kernel.h"
#include "cpu.h"
#include "vm_boot.h"
#include "stdio.h"

#define LAPIC_ID                    0x0020  // (rw) local apic id
#define LAPIC_VER                   0x0030  // (ro) version
#define LAPIC_TPR                   0x0080  // (rw) task priority register
#define LAPIC_APR                   0x0090  // (ro) arbitration priority
#define LAPIC_PPR                   0x00a0  // (ro) processor priority register
#define LAPIC_EOI                   0x00b0  // (wo) EOI
#define LAPIC_RRD                   0x00c0  // (ro) remote read register
#define LAPIC_LDR                   0x00d0  // (rw) logical destination register
#define LAPIC_DFR                   0x00e0  // (rw) destination format register
#define LAPIC_SVR                   0x00f0  // (rw) spurious interrupt vector register
#define LAPIC_ISR                   0x0100  // (ro) in-service register (x8)
#define LAPIC_TMR                   0x0180  // (ro) trigger mode register (x8)
#define LAPIC_IRR                   0x0200  // (ro) interrupt request register (x8)
#define LAPIC_ESR                   0x0280  // (ro) error status register
#define LAPIC_LVT_CMCI              0x02f0  // (rw) LVT CMCI register
#define LAPIC_ICRLO                 0x0300  // (rw) interrupt command register [31:0]
#define LAPIC_ICRHI                 0x0310  // (rw) interrupt command register [63:32]
#define LAPIC_LVT_TIMER             0x0320  // (rw) LVT timer register
#define LAPIC_LVT_THERMAL_SENSOR    0x0330  // (rw) LVT thermal sensor register
#define LAPIC_LVT_PERFMON_COUNTER   0x0340  // (rw) LVT performance monitoring counters
#define LAPIC_LVT_LINT0             0x0350  // (rw) LVT LINT0 register
#define LAPIC_LVT_LINT1             0x0360  // (rw) LVT LINT1 register
#define LAPIC_LVT_ERROR             0x0370  // (rw) LVT error register
#define LAPIC_TIMER_INITIAL_COUNT   0x0380  // (rw) timer initial count
#define LAPIC_TIMER_CURRENT_COUNT   0x0390  // (ro) timer current count
#define LAPIC_TIMER_DIVIDE_CONFIG   0x03e0  // (rw) timer divide configuration

// local apic id register
#define ID_SHIFT    24

// local apic version register
#define VER_MASK                0xff
#define VER_MAX_LVT_SHIFT       16
#define VER_MAX_LVT_MASK        0xff
#define VER_EOI_BCAST_SUPP      (1<<24) // EOI-broadcast suppression supported

// interrupt command register

// (rw) interrupt vector number
#define ICR_VECTOR_MASK         0xff
// (rw) delivery mode
#define ICR_FIXED               0x00000000
#define ICR_LOWEST              0x00000100
#define ICR_SMI                 0x00000200
#define ICR_NMI                 0x00000400
#define ICR_INIT                0x00000500
#define ICR_STARTUP             0x00000600
// (rw) destination mode
#define ICR_PHYSICAL            0x00000000
#define ICR_LOGICAL             0x00000800
// (ro) delivery status
#define ICR_IDLE                0x00000000
#define ICR_SEND_PENDING        0x00001000
// (rw) level
#define ICR_DEASSERT            0x00000000
#define ICR_ASSERT              0x00004000
// (rw) trigger mode
#define ICR_EDGE                0x00000000
#define ICR_LEVEL               0x00008000
// (rw) destination shorthand
#define ICR_NO_SHORTHAND        0x00000000
#define ICR_SELF                0x00040000
#define ICR_ALL_INCLUDING_SELF  0x00080000
#define ICR_ALL_EXCLUDING_SELF  0x000c0000
// (rw) destination field
#define ICR_DESTINATION_SHIFT   24
#define ICR_DESTINATION_MASK    0xff

// logical destination register
#define LDR_LOGICAL_LAPIC_SHIFT 24
#define LDR_LOGICAL_LAPIC_MASK  0xff

// destination format register
#define DFR_CLUSTER_MODEL       0x0fffffff
#define DFR_FLAT_MODEL          0xffffffff

// arbitration priority register
#define APR_PRI_SHIFT           4   // 7:4 priority
#define APR_PRI_MASK            0xf
#define APR_PRI_SUBCLASS_MASK   0xf // 3:0 priority sub-class

// task priority register
#define TPR_ENABLE_ALL_INTS     0x00
#define TPR_DISABLE_EXTERN_INTS 0x0f
#define TPR_DISABLE_ALL_INTS    0xff

// spurious-interrupt vector register
#define SVR_SPURIOUS_VEC_MASK   0xff
#define SVR_APIC_ENABLE         (1<<8)
#define SVR_APIC_DISABLE        0x0
#define SVR_FOCUS_CHECK_ENABLE  (1<<9)
#define SVR_FOCUS_CHECK_DISABLE 0x0
#define SVR_EOI_BCAST_SUPP_ON   (1<<12)
#define SVR_EOI_BCAST_SUPP_OFF  0x0

// LVT registers

// (rw) interrupt vector number
#define LVT_VECTOR_MASK         0xff
// (rw) delivery mode
#define LVT_FIXED               0x000
#define LVT_SMI                 0x200
#define LVT_NMI                 0x400
#define LVT_EXTINT              0x700
#define LVT_INIT                0x500
// (ro) delivery status
#define LVT_DELIVS_IDLE         0x0
#define LVT_DELIVS_SEND_PENDING (1<<12)
// (rw) interrupt input pin polarity
#define LVT_INTPOL_ACTIVE_HIGH  0x0
#define LVT_INTPOL_ACTIVE_LOW   (1<<13)
// (ro) remote IRR flag
#define LVT_REMOTE_IRR          (1<<14)
// (rw) trigger mode
#define LVT_TRIGGER_EDGE        0x0
#define LVT_TRIGGER_LEVEL       (1<<15)
// (rw) interrupt mask
#define LVT_INTERRUPT_ON        0x0
#define LVT_INTERRUPT_OFF       (1<<16)
// (rw) timer mode
#define LVT_TIMER_ONE_SHOT      0x00000
#define LVT_TIMER_PERIODIC      0x20000
#define LVT_TIMER_TSC_DEADLINE  0x40000

// error status register
#define ESR_SEND_CHECKSUM_ERR   (1<<0)
#define ESR_RECV_CHECKSUM_ERR   (1<<1)
#define ESR_SEND_ACCEPT_ERR     (1<<2)
#define ESR_RECV_ACCEPT_ERR     (1<<3)
#define ESR_REDIRECTABLE_IPI    (1<<4)
#define ESR_SEND_ILLEGAL_VECT   (1<<5)
#define ESR_RECV_ILLEGAL_VECT   (1<<6)
#define ESR_ILLEGAL_REG_ADDR    (1<<7)

// timer divide configuration register
#define TIMER_DIVIDE_CONF_2     0x00
#define TIMER_DIVIER_CONF_4     0x01
#define TIMER_DIVIER_CONF_8     0x02
#define TIMER_DIVIER_CONF_16    0x03
#define TIMER_DIVIER_CONF_32    0x08
#define TIMER_DIVIER_CONF_64    0x09
#define TIMER_DIVIER_CONF_128   0x0a
#define TIMER_DIVIER_CONF_1     0x0b


struct local_apic_t local_apic;

void local_apic_register(uintptr_t addr)
{
    local_apic.local_apic_addr = (uint8_t*)addr;
}

void local_apic_add_cpu(uint8_t apic_id)
{
    check(local_apic.num_cpus < MAX_CPUS);
    struct local_apic_cpu_t* cpu = &local_apic.cpus[local_apic.num_cpus++];
    cpu->apic_id = apic_id;
    cpu->flags = 0;
}

static uint32_t local_apic_read(unsigned int reg)
{
    return mmio_read_u32(local_apic.local_apic_addr + reg);
}

static void local_apic_write(unsigned int reg, uint32_t data)
{
    mmio_write_u32(local_apic.local_apic_addr + reg, data);
}

static void local_apic_lvt_enable(unsigned int reg, uint32_t intvec)
{
    uint32_t data = (intvec & LVT_VECTOR_MASK)
                  | LVT_FIXED
                  | LVT_INTPOL_ACTIVE_HIGH
                  | LVT_TRIGGER_EDGE
                  | LVT_INTERRUPT_ON;
    local_apic_write(reg, data);
}

static void local_apic_lvt_disable(unsigned int reg)
{
    uint32_t data = local_apic_read(reg);
    local_apic_write(reg, data | LVT_INTERRUPT_OFF);
}

void local_apic_init()
{
    vm_boot_map_range((uintptr_t)local_apic.local_apic_addr,
                      (uintptr_t)local_apic.local_apic_addr,
                      0x1000);

    uint32_t apic_id = local_apic_read(LAPIC_ID) >> ID_SHIFT;
    uint32_t logical_apic_id = (1 << apic_id) & LDR_LOGICAL_LAPIC_MASK;
    
    local_apic_write(LAPIC_TPR, TPR_ENABLE_ALL_INTS);
    local_apic_write(LAPIC_DFR, DFR_FLAT_MODEL);
    local_apic_write(LAPIC_LDR, logical_apic_id << LDR_LOGICAL_LAPIC_SHIFT);
    local_apic_write(LAPIC_SVR, SVR_APIC_ENABLE | 0xFF);

    //local_apic_lvt_enable(LAPIC_LVT_LINT0, 0xf0);
    //local_apic_lvt_enable(LAPIC_LVT_LINT1, 0xf1);
    //local_apic_lvt_enable(LAPIC_LVT_ERROR, 0xf2);
}

void local_apic_timer_init()
{
    // measure time using perf counter
    int spl = cpu_splhi();
    local_apic_write(LAPIC_TIMER_DIVIDE_CONFIG, TIMER_DIVIER_CONF_16);
    local_apic_write(LAPIC_LVT_TIMER, LVT_TIMER_ONE_SHOT | LVT_INTERRUPT_OFF);
    local_apic_write(LAPIC_TIMER_INITIAL_COUNT, 0xffffffff);
    cpu_splx(spl);

    cpu_wait(64);

    spl = cpu_splhi();
    uint32_t count = local_apic_read(LAPIC_TIMER_CURRENT_COUNT);
    cpu_splx(spl);

    uint32_t ticks = 0xffffffff - count;

    printf("apic_timer: %u ticks\n", (ticks*16)/64);
}

uint32_t local_apic_id()
{
    int spl = cpu_splhi();
    uint32_t id = local_apic_read(LAPIC_ID) >> ID_SHIFT;
    cpu_splx(spl);

    return id;
}

void local_apic_eoi()
{
    //int spl = cpu_splhi();
    local_apic_write(LAPIC_EOI, 0);
    //cpu_splx(spl);
}

void local_apic_ipi_init(uint32_t apic_id)
{
    int spl = cpu_splhi();

    local_apic_write(LAPIC_ICRHI, apic_id << ICR_DESTINATION_SHIFT);
    local_apic_write(LAPIC_ICRLO, ICR_INIT | ICR_PHYSICAL
                     | ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND);

    while (local_apic_read(LAPIC_ICRLO) & ICR_SEND_PENDING)
    {}

    cpu_splx(spl);
}

void local_apic_ipi_start(uint32_t apic_id)
{
    int spl = cpu_splhi();

    uint32_t vector = 0x8;

    local_apic_write(LAPIC_ICRHI, apic_id << ICR_DESTINATION_SHIFT);
    local_apic_write(LAPIC_ICRLO, vector | ICR_STARTUP
                        | ICR_PHYSICAL | ICR_ASSERT | ICR_EDGE | ICR_NO_SHORTHAND);

    while (local_apic_read(LAPIC_ICRLO) & ICR_SEND_PENDING)
    {}

    cpu_splx(spl);
}

static void local_apic_ipi_shorthand(uint32_t vector, uint32_t shorthand)
{
    int spl = cpu_splhi();

    local_apic_write(LAPIC_ICRHI, 0);
    local_apic_write(LAPIC_ICRLO, (vector & ICR_VECTOR_MASK)
                        | ICR_FIXED | ICR_ASSERT | ICR_EDGE | shorthand);

    while (local_apic_read(LAPIC_ICRLO) & ICR_SEND_PENDING)
    {}

    cpu_splx(spl);
}

void local_apic_ipi_self(uint32_t vector)
{
    local_apic_ipi_shorthand(vector, ICR_SELF);
}

void local_apic_ipi_broadcast(uint32_t vector)
{
    local_apic_ipi_shorthand(vector, ICR_ALL_EXCLUDING_SELF);
}

void local_apic_ipi_all(uint32_t vector)
{
    local_apic_ipi_shorthand(vector, ICR_ALL_INCLUDING_SELF);
}
