### Interrupts

Each interrupt vector is an 8-bit value. The interrupt-priority class is the value of bits 7:4 of the interrupt vector. The lowest interrupt-priority class is 1 and the highest is 15; interrupts with vectors in the range 0–15 (with inter- rupt-priority class 0) are illegal and are never delivered. Because vectors 0–31 are reserved for dedicated uses by the Intel 64 and IA-32 architectures, software should configure interrupt vectors to use interrupt-priority classes in the range 2–15.

10.8.6 Task Priority in IA-32e Mode
In IA-32e mode, operating systems can manage the 16 interrupt-priority classes (see Section 10.8.3, “Interrupt, Task, and Processor Priority”) explicitly using the task priority register (TPR). Operating systems can use the TPR to temporarily block specific (low-priority) interrupts from interrupting a high-priority task. This is done by loading TPR with a value in which the task-priority class corresponds to the highest interrupt-priority class that is to be blocked. For example:
• Loading the TPR with a task-priority class of 8 (01000B) blocks all interrupts with an interrupt-priority class of 8 or less while allowing all interrupts with an interrupt-priority class of 9 or more to be recognized.
• Loading the TPR with a task-priority class of 0 enables all external interrupts.
• Loading the TPR with a task-priority class of 0FH (01111B) disables all external interrupts.
The TPR (shown in Figure 10-18) is cleared to 0 on reset. In 64-bit mode, software can read and write the TPR using an alternate interface, MOV CR8 instruction. The new task-priority class is established when the MOV CR8 instruction completes execution. Software does not need to force serialization after loading the TPR using MOV CR8.

intvec:     0x00 - 0xFF
reserved:   0x00 - 0x1F // priority class 0,1
            0x20 - 0x2F // priority class 2
                        // etc

TPR = priority class (0x02 - 0x0F)

### References

https://github.com/brho/akaros/tree/master/kern/arch/x86
smp.c
apic.c

https://github.com/brho/akaros/blob/b91cfeb70668868b550096f435abde9b1f68baac/kern/arch/x86/trap.h

/* 240-255 are LAPIC vectors (0xf0-0xff), hightest priority class */
#define IdtLAPIC				240
#define IdtLAPIC_TIMER			(IdtLAPIC + 0)
#define IdtLAPIC_THERMAL		(IdtLAPIC + 1)
#define IdtLAPIC_PCINT			(IdtLAPIC + 2)
#define IdtLAPIC_LINT0			(IdtLAPIC + 3)
#define IdtLAPIC_LINT1			(IdtLAPIC + 4)
#define IdtLAPIC_ERROR			(IdtLAPIC + 5)

Yes. Typically LINT1 will remain NMI, while the OS will mask LINT0 and
configure the IOAPIC to deliver ISA interrupts directly to a local APIC.