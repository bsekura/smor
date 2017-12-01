#define PUSH_REGS   \
    pushq %rax;  \
    pushq %rcx;  \
    pushq %rdx;  \
    pushq %r8;  \
    pushq %r9;   \
    pushq %r10;  \
    pushq %r11;

#define POP_REGS    \
    popq %r11;   \
    popq %r10;   \
    popq %r9;    \
    popq %r8;    \
    popq %rdx;   \
    popq %rcx;   \
    popq %rax;

#define TRAP(n, t) \
    .global _##n; \
    .align 16; \
_##n##:; \
    push $0; \
    push $(t); \
    cld; \
    PUSH_REGS \
    call cpu_exception; \
    jmp _isr_ret

#define TRAP_ERR(n, t) \
    .global _##n; \
    .align 16; \
_##n##:; \
    push $(t); \
    cld; \
    PUSH_REGS \
    call cpu_exception; \
    jmp _isr_ret

#define IRQ(n, t) \
    .global _##n; \
    .align 16; \
_##n##:; \
    push $0; \
    push $(t); \
    cld; \
    PUSH_REGS \
    call cpu_interrupt; \
    jmp _isr_ret

#define LINT(n, t) \
    .global _##n; \
    .align 16; \
_##n##:; \
    push $0; \
    push $(t); \
    cld; \
    PUSH_REGS \
    call cpu_local_interrupt; \
    jmp _isr_ret

    .global _isr_ret
    .align 16
_isr_ret:
    POP_REGS
    addq $16, %rsp
    iretq

    .global _isr_spurious
    .align  16
_isr_spurious:
    iretq


TRAP(trap_0, 0)
TRAP(trap_1, 1)
TRAP(trap_2, 2)
TRAP(trap_3, 3)
TRAP(trap_4, 4)
TRAP(trap_5, 5)
TRAP(trap_6, 6)
TRAP(trap_7, 7)
TRAP_ERR(trap_8, 8)
TRAP(trap_9, 9);
TRAP_ERR(trap_10, 10)
TRAP_ERR(trap_11, 11)
TRAP_ERR(trap_12, 12)
TRAP_ERR(trap_13, 13)
TRAP_ERR(trap_14, 14)
// 15 reserved
TRAP(trap_16, 16)
TRAP_ERR(trap_17, 17)
TRAP(trap_18, 18)
TRAP(trap_19, 19)
TRAP(trap_20, 20)
// 21-29 reserved
TRAP_ERR(trap_30, 30)
// 31 reserved


IRQ(irq_0, 0)
IRQ(irq_1, 1)
IRQ(irq_2, 2)
IRQ(irq_3, 3)
IRQ(irq_4, 4)
IRQ(irq_5, 5)
IRQ(irq_6, 6)
IRQ(irq_7, 7)
IRQ(irq_8, 8)
IRQ(irq_9, 9)
IRQ(irq_10, 10)
IRQ(irq_11, 11)
IRQ(irq_12, 12)
IRQ(irq_13, 13)
IRQ(irq_14, 14)
IRQ(irq_15, 15)

LINT(lint_0, 0)
LINT(lint_1, 1)
LINT(lint_2, 2)
LINT(lint_3, 3)
LINT(lint_4, 4)
LINT(lint_5, 5)
LINT(lint_6, 6)
LINT(lint_7, 7)
LINT(lint_8, 8)


#
# void context_switch(struct switch_context_t** old_ctx,
#                     struct switch_context_t* new_ctx);
#

    .global context_switch
    .align 16
context_switch:
    pushq %rbx
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)   # %rdi = old_ctx
    movq %rsi, %rsp     # %rsi = new_ctx
    
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rsi
    popq %rdi
    popq %rbp
    popq %rbx

    retq

    .global idle_loop
    .align 16
idle_loop:
    hlt
    jmp idle_loop
