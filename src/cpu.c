#include "cpu.h"
#include "interrupt.h"
#include "types.h"
#include "stdio.h"
#include "kernel.h"
#include "io_apic.h"
#include "local_apic.h"
#include "sched.h"
#include "vm_boot.h"

struct cpu_desc_t cpus[MAX_CPUS];
uint32_t num_cpus;

void cpu_init_ap(void);

struct segment_desc_t {
    uint16_t limit;
    uint64_t base;
} PACKED;

struct idt_entry_t {
    uint16_t offset0;
    uint16_t selector;
    uint16_t type;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t reserved;
} PACKED;

#define IDT_TABLE (KERNEL_BASE|0x0F000)

static void idt_set(unsigned int index, uint16_t type, void(*handler)())
{
    struct idt_entry_t* entry = (struct idt_entry_t*)IDT_TABLE + index;
    uint64_t base = (uint64_t)handler;

    entry->offset0 = (uint16_t)base;
    entry->selector = 0x08;
    entry->type = type;
    entry->offset1 = (uint16_t)(base >> 16);
    entry->offset2 = (uint32_t)(base >> 32);
    entry->reserved = 0;
}

#define IDT_INTERRUPT_GATE      0x8e00
#define IDT_TRAP_GATE           0x8f00

static void idt_load()
{
    struct segment_desc_t idt_desc = {
        .limit = 256 * sizeof(struct idt_entry_t) - 1,
        .base = IDT_TABLE
    };
    asm volatile("lidt %0" : : "m"(idt_desc) : "memory");
}

extern void _trap_0();
extern void _trap_1();
extern void _trap_2();
extern void _trap_3();
extern void _trap_4();
extern void _trap_5();
extern void _trap_6();
extern void _trap_7();
extern void _trap_8();
extern void _trap_9();
extern void _trap_10();
extern void _trap_11();
extern void _trap_12();
extern void _trap_13();
extern void _trap_14();
// 15 reserved
extern void _trap_16();
extern void _trap_17();
extern void _trap_18();
extern void _trap_19();
extern void _trap_20();
// 21-29 reserved
extern void _trap_30();
// 31 reserved

extern void _isr_spurious();

static void idt_init()
{
    void(*cpu_exceptions[32])(void) = {
        _trap_0,
        _trap_1,
        _trap_2,
        _trap_3,
        _trap_4,
        _trap_5,
        _trap_6,
        _trap_7,
        _trap_8,
        _trap_9,
        _trap_10,
        _trap_11,
        _trap_12,
        _trap_13,
        _trap_14,
        _isr_spurious, // 15 reserved
        _trap_16,
        _trap_17,
        _trap_18,
        _trap_19,
        _trap_20,
        _isr_spurious, // 21-29 reserved
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _isr_spurious,
        _trap_30,
        _isr_spurious // 31 reserved
    };

    for (unsigned int i = 0; i < 32; ++i)
        idt_set(i, IDT_TRAP_GATE, cpu_exceptions[i]);

    for (unsigned int i = 32; i < 256; ++i)
        idt_set(i, IDT_INTERRUPT_GATE, _isr_spurious);

    idt_load();
}

void cpu_interrupt_set(uint8_t vector, irq_handler_fn handler)
{
    idt_set(vector, IDT_INTERRUPT_GATE, handler);
}

void cpu_interrupt_unset(uint8_t vector)
{
    idt_set(vector, IDT_INTERRUPT_GATE, _isr_spurious);
}

#define GDT_ACCESS_RW       (1UL<<41)
#define GDT_ACCESS_EXEC     (1UL<<43)
#define GDT_ACCESS_SET      (1UL<<44)
#define GDT_ACCESS_USER     (1UL<<46)
#define GDT_ACCESS_PRESENT  (1UL<<47)

#define GDT_FLAGS_X86_64    (1UL<<53)

struct gdt_table_t {
    uint64_t unused;
    uint64_t kernel_code;
    uint64_t kernel_data;
    uint64_t user_code;
    uint64_t user_data;
};

extern uint8_t gdt;

static void gdt_load()
{
    struct gdt_table_t* gdt_table = (struct gdt_table_t*)&gdt;
    struct segment_desc_t gdt_desc = {
        .limit = 5 * sizeof(uint64_t) - 1,
        .base = (uint64_t)gdt_table
    };
    asm volatile("lgdt %0" : : "m"(gdt_desc) : "memory");
}

static void gdt_init()
{
    struct gdt_table_t* gdt_table = (struct gdt_table_t*)&gdt;

    uint64_t base = GDT_ACCESS_RW
                  | GDT_ACCESS_SET
                  | GDT_ACCESS_PRESENT
                  | GDT_FLAGS_X86_64;

    gdt_table->kernel_code = base | GDT_ACCESS_EXEC;
    gdt_table->kernel_data = base;
    gdt_table->user_code = base | GDT_ACCESS_USER | GDT_ACCESS_EXEC;
    gdt_table->user_data = base | GDT_ACCESS_USER;

    gdt_load();
}

void cpu_boot_init()
{
    gdt_init();
    idt_init();
}

static void timer_irq_handler(struct isr_frame_t* frame)
{
    struct cpu_desc_t* cpu = cpu_lock();
    
    cpu->ticks++;
    sched_tick(cpu);
    
    cpu_unlock(cpu);
}

static void lapic_irq_handler(struct isr_frame_t* frame)
{
    struct cpu_desc_t* cpu = get_cpu();
    printf("lapic_irq(): [%d] %d err %016lx\n", 
            cpu->apic_id, frame->trap_num, frame->trap_err);
    printf("lapic_irq(): rip %016lx rflags %016lx rsp %016lx cs %08x\n", 
            frame->rip, frame->rflags, frame->rsp, frame->cs);
}

static void cpu_init_desc()
{
    uint32_t apic_id = local_apic_id();
    struct cpu_desc_t* cpu = &cpus[apic_id];
    cpu->self = cpu;
    cpu->apic_id = apic_id;

    asm volatile("movl %0,%%fs; movl %0,%%gs" :: "r"(0));
    wrmsr(MSR_GS_BASE, (uintptr_t)cpu);
}

extern void idle_loop(void);

void cpu_init_ap()
{
    fetch_and_add_32(&num_cpus, 1);

    // sync boot pages before tlb shootdown is ready
    //vm_boot_flush();

    gdt_load();
    idt_load();

    local_apic_init();

    cpu_init_desc();
    struct cpu_desc_t* cpu = get_cpu();
    cpu->flags = CPU_FLAGS_ACTIVE;

    sched_init_cpu(cpu);
    cpu_enable_interrupts();

    asm volatile("int $3");
    idle_loop();
}

static void boot_enable_ap()
{
    struct boot_info_t* boot_info = kernel_boot_info();
    boot_info->boot_ap = 1;
}

static void cpu_smp_init()
{
    num_cpus = 1;
    boot_enable_ap();

    uint32_t bsp_apic_id = local_apic_id();

    // issue init IPI to all except self
    uint8_t cpu_mask = 1 << bsp_apic_id;
    for (uint32_t i = 0; i < local_apic.num_cpus; ++i) {
        uint32_t apic_id = local_apic.cpus[i].apic_id;
        if (apic_id != bsp_apic_id) {
            local_apic_ipi_init(apic_id);
            cpu_mask |= 1 << apic_id;
        }
    }

    // wait 10 ms
    cpu_wait(10);

    // issue startup IPI to all except self
    for (uint32_t i = 0; i < local_apic.num_cpus; ++i) {
        uint32_t apic_id = local_apic.cpus[i].apic_id;
        if (apic_id != bsp_apic_id)
            local_apic_ipi_start(apic_id);
    }

    // wait 200 ms
    cpu_wait(200);

    while (num_cpus != local_apic.num_cpus)
        cpu_wait(1);

    printf("%d cpu(s) online\n", num_cpus);

    // enable timer on all cpus
    intr_irq_enable(IRQ_TIMER, cpu_mask);

    cpu_wait(10); // no reason whatsoever

    for (uint32_t i = 0; i < num_cpus; ++i)
        printf("cpu[%d]: %02x %d\n", i, cpus[i].flags, cpus[i].ticks);
}

// main init - called from BSP
void cpu_init()
{
    io_apic_init();
    local_apic_init();
    intr_init();

    cpu_init_desc();
    struct cpu_desc_t* cpu = get_cpu();

    cpu->flags = CPU_FLAGS_ACTIVE | CPU_FLAGS_BSP;
    sched_init_cpu(cpu);

    intr_register_local_irq_handler(0, 0xf0, lapic_irq_handler);

    // setup timer on bsp only for now
    intr_register_irq_handler(IRQ_TIMER, timer_irq_handler);
    intr_irq_enable(IRQ_TIMER, 1 << cpu->apic_id);
    cpu_enable_interrupts();
    
    // test
    //local_apic_ipi_self(0xf0);
    local_apic_timer_init();

    cpu_smp_init();
}

// TEMP: used during boot only
void __attribute__((optimize("O0"))) cpu_wait(uint32_t ms)
{
    struct cpu_desc_t* cpu = get_cpu();
    volatile uint64_t ticks = cpu->ticks;
    while (1) {
        uint32_t n = (uint32_t)(cpu->ticks - ticks);
        if (n >= ms)
            break;
    }
}

void cpu_show_cmd(int argc, const char* argv[])
{
    for (uint32_t i = 0; i < num_cpus; ++i) {
        struct cpu_desc_t* cpu = cpu_lock_smp(i);
        printf("[%d]: %02x %ld ", i, cpu->flags, cpu->ticks);
        struct thread_t* t = cpu->threads;
        do {
            printf("%d[%d:%d] ", t->id, t->state, t->ticks);
            t = t->next;
        } while (t != cpu->threads);
        printf("\n");
        cpu_unlock_smp(cpu);
    }
}