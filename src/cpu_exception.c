#include "cpu_exception.h"
#include "stdio.h"

#define EXCEPTION_DIVIDE_BY_ZERO            0x0
#define EXCEPTION_DEBUG                     0x1
#define EXCEPTION_NMI                       0x2
#define EXCEPTION_BREAKPOINT                0x3
#define EXCEPTION_OVERFLOW                  0x4
#define EXCEPTION_BOUND_RANGE_EXCEEDED      0x5
#define EXCEPTION_INVALID_OPCODE            0x6
#define EXCEPTION_DEVICE_NOT_AVAILABLE      0x7
#define EXCEPTION_DOUBLE_FAULT              0x8
#define EXCEPTION_INVALID_TSS               0xA
#define EXCEPTION_SEGMENT_NOT_PRESENT       0xB
#define EXCEPTION_STACK_SEGMENT_FAULT       0xC
#define EXCEPTION_GENERAL_PROTECTION_FAULT  0xD
#define EXCEPTION_PAGE_FAULT                0xE
#define EXCEPTION_FP_EXCEPTION              0x10
#define EXCEPTION_ALIGNMENT_CHECK           0x11
#define EXCEPTION_MACHINE_CHECK             0x12
#define EXCEPTION_SIMD_EXCEPTION            0x13
#define EXCEPTION_VIRTUALIZATION_EXCEPTION  0x14
#define EXCEPTION_SECURITY_EXCEPTION        0x1E

static void handle_page_fault(struct isr_frame_t* frame)
{
    printf("pf: %016lx\n", get_cr2());
    cpu_disable_interrupts();
    cpu_halt();
}

void cpu_exception(struct isr_frame_t frame)
{
    //if (frame.rflags & RFLAGS_IF)
    //    cpu_enable_interrupts();

    struct cpu_desc_t* cpu = get_cpu();
    printf("cpu_exception(): [%d] %d err %016lx\n", 
            cpu->apic_id, frame.trap_num, frame.trap_err);
    printf("cpu_exception(): rip %016lx rflags %016lx rsp %016lx cs %08x\n", 
            frame.rip, frame.rflags, frame.rsp, frame.cs);

    switch (frame.trap_num) {
        case EXCEPTION_BREAKPOINT:
            break;
        case EXCEPTION_PAGE_FAULT:
            handle_page_fault(&frame);
            break;
        default:
            printf("unhandled\n");
            cpu_halt();
            break;
    }
}
