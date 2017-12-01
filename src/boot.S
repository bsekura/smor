.set KERNEL_VMA, 0xffffffff80000000

.section .multiboot
.align 4
mb_header_start:
    .long 0xE85250D6
    .long 0
    .long mb_header_end - mb_header_start
    .long -(0xE85250D6 + 0 + (mb_header_end - mb_header_start))

    .word 5
    .word 0
    .long 20
    .long 1024
    .long 768
    .long 16

    .align 8
    .word 0
    .word 0
    .long 8
mb_header_end:

.set MULTIBOOT2_BOOTLOADER_MAGIC, 0x36d76289


.set STACK_TOP,     0x90000
.set BOOT_PAGES,    0x10000

.set BOOT_PML4,     0x10000
.set BOOT_PD,       0x11000
.set BOOT_PDP_HI,   0x12000
.set BOOT_PDP_LO,   0x13000

# low mem usable start: 0x07E00
.set BOOT_PARAMS,   0x0A000
.set MB2_INFO,      0x0B000
.set IDT_TABLE,     0x0F000

.section .data
.align 4096
.global gdt
gdt:
    .quad 0x0
    .quad (1<<44) | (1<<47) | (1<<41) | (1<<43) | (1<<53)
    .quad (1<<44) | (1<<47) | (1<<41) | (1<<53)
    .quad 0x0
    .quad 0x0

gdt_ptr:
    .word 3 * 8 - 1
    .quad gdt - KERNEL_VMA

stack_top:
    .quad KERNEL_VMA|STACK_TOP

.section .text
.code32
.global bootstrap
bootstrap:
    cli
    cld

    # multiboot info can be placed anywhere
    # copy it to low address to make things simple
    cmpl $(MULTIBOOT2_BOOTLOADER_MAGIC), %eax
    jnz no_multiboot

    movl %eax, BOOT_PARAMS
    movl $(MB2_INFO), %edi # mb info addr after copy
    movl %edi, BOOT_PARAMS + 4
    movl (%ebx), %ecx # first uint32 is mb info size
    movl %ecx, BOOT_PARAMS + 8
    movl %ebx, %esi
    rep movsb

    jmp 1f

no_multiboot:

    xorl %eax, %eax
    movl $(BOOT_PARAMS), %edi
    movl %eax, 0(%edi)
    movl %eax, 4(%edi)
    movl %eax, 8(%edi)

1:
    # cpu flag: 0 if bps, otherwise ap
    xorl %eax, %eax
    movl %eax, BOOT_PARAMS + 12

    # copy 16-bit ap bootstrap to 0x8000
    movl $(boot_ap_end - boot_ap), %ecx
    shrl $2, %ecx
    movl $(boot_ap - KERNEL_VMA), %esi
    movl $0x8000, %edi
    rep movsl

    # clear 2x 64K at 0x10000
    # this is where our boot page dirs are
    cld 
    movl $0x10000, %edi
    movl $(0x20000 >> 2), %ecx
    xorl %eax, %eax
    rep stosl

    # determine the number of 2M pages (rounded) we need to map
    # result in %ecx
    movl $(_end - KERNEL_VMA), %ecx
    addl $(0x200000 - 1), %ecx
    shrl $21, %ecx

    # fill boot page directory
    movl $0x83, %eax    # addr 0x0 + page flags (present+write+2M)
    movl $0, %esi
map_loop:
    movl %eax, BOOT_PD(%esi)
    addl $0x8, %esi
    addl $0x200000, %eax
    loop map_loop

    # page dir 0 - 2M (read write, huge page on)
    #movl $0b10000011, BOOT_PD

    movl $(BOOT_PD + 0x003), BOOT_PDP_LO
    movl $(BOOT_PD + 0x003), BOOT_PDP_HI + 510 * 8 # kernel at top 2G of this 512G region

    movl $(BOOT_PDP_LO + 0x003), BOOT_PML4
    movl $(BOOT_PDP_HI + 0x003), BOOT_PML4 + 511 * 8 # last 512G of 256T space

code_32:
    movl $(BOOT_PML4), %ecx
    movl %ecx, %cr3
    
    movl %cr4, %ecx
    orl $1<<5, %ecx
    movl %ecx, %cr4

    movl $0xC0000080, %ecx
    rdmsr
    orl $1<<8, %eax
    wrmsr

    movl %cr0, %eax
    orl $1<<31, %eax
    movl %eax, %cr0
 
    #lgdt _gdt_32_ptr - KERNEL_VMA

    lgdt gdt_ptr - KERNEL_VMA
    ljmp $0x08, $(long_mode - KERNEL_VMA)

.code64
.align 8
long_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movl BOOT_PARAMS + 12, %eax
    cmp $0x0, %eax
    jnz long_mode_ap

    # setup stack pointer
    movq $-0x4000, %rsp
    xadd %rsp, stack_top

    # long jump to high kernel address (reload %rip)
    movq $(really_long), %rax 
    pushq $0x08 
    pushq %rax 
    lretq 

really_long:
    # remove low identity mapping
    #movq $0x0, BOOT_PML4
    #mov %cr3, %rcx
    #mov %rcx, %cr3

    call kernel_main

1:  hlt
    jmp 1b

long_mode_ap:
    # setup stack
    movq $-0x4000, %rsp
    lock
    xadd %rsp, stack_top

    # long jump to high kernel address (reload %rip)
    movq $(really_long_ap), %rax 
    pushq $0x08 
    pushq %rax 
    lretq 

really_long_ap:
    call cpu_init_ap

1:
    hlt
    jmp 1b


.code16
.global boot_ap
boot_ap:
    cli
    cld
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # enable A20
    in $0x92, %al
    or $0x02, %al
    out %al, $0x92

    jmp 1f

.align 4
gdt_32:
    .quad 0x0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x0
gdt_32_ptr:
    .word 3 * 8 - 1
    .long gdt_32 - boot_ap + 0x8000

1:
    lgdt gdt_32_ptr - boot_ap + 0x8000

    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0

    ljmp $0x08, $(ap32 - boot_ap + 0x8000)

.code32
ap32:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    ljmp $0x08, $(code_32 - KERNEL_VMA)

1:  hlt
    jmp 1b
    .long 0x0
boot_ap_end:
    nop
