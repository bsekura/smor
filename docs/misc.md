Devices 

In general, there's 3 different types of devices:
ISA Devices With Fixed IRQs - these devices always have the same ISA IRQs numbers for historical reasons (backwards compatability). This includes the PS/2 controller (IRQ 1 and IRQ 12), the PIT chip, the CMOS/RTC, the first floppy controller, the first 2 serial ports and the first parallel port. The first 2 PATA/ATAPI/SATA hard disk contollers might also fit in this category - modern hard disk controllers have a "legacy mode" and a "native mode" where they use ISA IRQ 14 or 15 in legacy mode and behave like PCI devices in native mode.
ISA Devices Without Fixed IRQs - these devices use ISA IRQs but there are no predefined IRQs for them. Examples include network cards, sound cards, SCSI controllers, video cards, additional serial ports, additional parallel ports, additional floppy controllers and additional PATA/ATAPI/SATA hard disk controllers; but only if they aren't PCI devices. For these devices you might be able to use "ISA Plug and Play" to detect (and reconfigure) the resources they use, but in general you need to use configuration scripts or something that are setup by the end-user.
PCI Devices - for these devices the BIOS/firmware/OS can automatically detect (and reconfigure) resources the devices use. For the sake of completeness, MCA devices also fit in this category (I'm not sure about EISA).


The I/O APIC 

For the I/O APIC, the MP specification and/or ACPI tables will tell you which IRQ each PCI device uses. This means you'll find an "I/O Interrupt Assignment Entry" for every PCI device (and if 2 or more PCI devices share the same IRQ line you'll get 2 or more "I/O Interrupt Assignment Entries" for that IRQ line rather than just one). 

In addition, you will (should) also find an "I/O Interrupt Assignment Entry" for every possible ISA IRQ line, regardless of how many devices are connected to that ISA IRQ line (if any). 

How To Make Sense Of It - The Lazy Way 

The lazy way to do things is to do very little during boot. When an ISA device driver wants to install an IRQ handler you search the MP specification and/or ACPI tables for the "I/O Interrupt Assignment Entry" that corresponds to the ISA IRQ, then install the IRQ handler and enable the IRQ in the I/O APIC. 

When a PCI device driver wants to install an IRQ handler you can do something very similar - search for the PCI device in the MP specification and/or ACPI tables (e.g. using it's "bus:device:function") to find out which I/O APIC input it's using, and then either (if nothing is already using the IRQ) install a new IRQ handler and enable the IRQ in the I/O APIC, or (if something is already using the IRQ) find the existing IRQ handler and add the new device's IRQ handler to it. [Note: I'm not going to describe methods of handling PCI IRQ sharing here] 

How To Make Sense Of It - The Compatible Way 

Another way to do things would be to setup dummy IRQ handlers for ISA devices during boot, so that (for example) if the PIC chips are being used then ISA IRQs generate interrupts 0x20 to 0x2F, and if I/O APICs are being used then ISA IRQs still generate interrupts 0x20 to 0x2F. In this case, an ISA device driver (e.g. a keyboard driver) just needs to install an IRQ handler (e.g. interrupt 0x21) without caring if the OS is using PIC chips or I/O APICs (except for the EOI). 

You could handle PCI devices the same way as you would for the lazy way, or you could install dummy handlers and configure these in the I/O APIC during boot too. 

Notes 

Copy the information you need from the MP specification and/or ACPI tables into your own data structure/s so that you don't need to care which (MP or ACPI) was used after boot, and can reclaim (free and reuse) any RAM that the ACPI tables were using. I'd recommend a hierarchical tree with one entry for each device that describes everything about the device (I/O ports, IRQs, DMA channels, device driver name, status, etc). 

Next, realise that (if you're using I/O APICs) the interrupt vector you set (and the IDT entry you use) determines the priority of the interrupt (but makes no difference otherwise). For this reason the "Compatible Way" is probably a bad way. For example, where possible you might want to ensure that some IRQs are higher priority than others (e.g. for me the RTC/CMOS IRQ is the highest priority IRQ when I/O APICs are used). 

Also, some PCI devices support MSI (Message Signalled Interrupts) where the device can send an IRQ directly to the CPU/s (without using an I/O APIC input or being configured in the I/O APIC). I assume this is intended to reduce the need for IRQ sharing. 

Lastly, for NUMA and SMP systems you might want to consider some intelligent IRQ balancing. There's several conflicting ideas here - attempt to reduce the overhead of interrupts on CPUs running high priority tasks by sending IRQs to CPUs running low priority tasks; make sure IRQs are sent to CPUs where the corresponding IRQ handler and it's data is still in the CPUs cache; don't send IRQs to sleeping/halted CPUs to avoid waking them up (to improve power management/consumption). Stock Linux kernels are crap when it comes to IRQ balancing (there isn't any), but people (AFAIK Intel is a contributor) are trying to fix it with an add-on daemon called "irqbalance". The irqbalance website does have good ideas and explanations that are worth reading (but I wouldn't recommend fixing bad design with add-on daemons). 



LINT0/LINT1/Local APIC Timer/Error interrupt

https://software.intel.com/sites/default/files/managed/a4/60/325384-sdm-vol-3abcd.pdf

CHAPTER 10
ADVANCED PROGRAMMABLE
INTERRUPT CONTROLLER (APIC)


# NOTES

use boot page dirs

setup page_db
    page_reserve
    
hat_t
    for mapping

regions with cache


## Memory

all 2M pages on a global hash

slab_list:
    16K

slab_list for smaller things using, uses slab_list above


0-512           1Gb 0-512G
    0-512       2M  0-1G
        0-512   4K  0-2M


# TODO

- in boot.S
    calc end address by adding multiboot struct to _end.
- vm
    vm_page pdb page defines
    vm_mmap regions, mmap
    vm_kern boot, kern setup, etc
    
- vm_boot_map
    simple helper to map pages using boot page dir

- kb interrupt
    kb support
    simple kernel shell


# booting ap

- keep low identity mapping
- ap needs it to init into long mode
- remove low mapping after all APs are running
- put AP index at BOOT_PARAMS + 8
- use to compute stack


# x86_64

If the interrupt handler code is simply a stub that forwards to C code, you don't need to save all of the registers. You can assume that the C compiler will generate code that will be preserving rbx, rbp, rsi, rdi, and r12 thru r15. You should only need to save and restore rax, rcx, rdx, and r8 thru r11.

The segment registers hold no value in long mode (if the interrupted thread is running in 32-bit compatibility mode you must preserve the segment registers, thanks ughoavgfhw). Actually, they got rid of most of the segmentation in long mode, but FS is still reserved for operating systems to use as a base address for thread local data. The register value itself doesn't matter, the base of FS and GS are set through MSRs 0xC0000100 and 0xC0000101. Assuming you won't be using FS you don't need to worry about it, just remember that any thread local data accessed by the C code could be using any random thread's TLS. Be careful of that because C runtime libraries use TLS for some functionality (example: strtok typically uses TLS).

There is a special instruction, SWAPGS, which swaps the values of the FS/GS segment base MSRs, you may want to use that to switch over to the kernel TLS for the current thread. The idea is to have the user mode TLS in FS, and the kernel mode TLS for that thread at GS, protected by page permissions, and use SWAPGS to switch over so FS contains the kernel mode TLS base in kernel mode, then when restoring context, SWAPGS it back so user mode TLS is back in FS and kernel mode TLS base is back in GS.

http://wiki.osdev.org/Calling_Conventions

# Links

http://lxr.free-electrons.com/source/include/asm-x86_64/system.h?v=2.4.37
https://github.com/pdoane/osdev

https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/arch/x86/lib/memcpy_64.S?id=HEAD

http://wiki.xomb.org/index.php?title=I/O_Advanced_Programmable_Interrupt_Controller

course
https://www.cs.usfca.edu/~cruse/cs630f06/

linux cross reference
http://lxr.free-electrons.com/source/arch/x86/kernel/apic/apic.c

bsd
https://svnweb.freebsd.org/base/head/sys/amd64/amd64/

### Scheduling

all cores enter idle thread

```
1:  hlt
    jmp 1b

newly created thread
    alloc stack
    fill it:
        trap_frame
        rsp -> trap_return
        ctx
        eip -> start_thread

        start_thread()
        {
            // do some init things
            unlock(); // release lock

        } // ret here will call trap_return

context_switch(ctx old, ctx new)
{
    swap contexts
}

cpu_t {
    thread_list;    // always has idle thread at top
                    // idle thread is always runnable
};

clock_irq()
{
    lock();
    schedule();
    unlock();
}

schedule()
{
    next_thread = pick_thread();
    if (next_thread == cur_thread)
        return;

    save_thread = cur_thread;
    cur_thread = thread;
    context_switch(save_thread->ctx, cur_thread->ctx);
}

yield()
{
    lock();
    schedule();
    unlock();
}
```

thread:

    list of threads
    two lists: runnable, sleeping
    state: runnable, waiting, num_quanta
    idle thread always first on the list, always runnable

    semaphore

    keyboard buffer with semaphore

    kernel console thread
        waits on keyboard semaphore

    test thread that draws things on console


LOCKING:

local_apic is per core, but writes and reads are multiple instructions;
all local_apic calls must be projectected by splhi/splx

local_io_apic is shared, need splhi/splx + spinlock

INTERRUPTS, TRAPS:

If an interrupt or exception handler is called
through an interrupt gate, the processor clears the interrupt enable (IF) flag in the EFLAGS register to prevent
subsequent interrupts from interfering with the execution of the handler. When a handler is called through a trap
gate, the state of the IF flag is not changed.

http://lxr.free-electrons.com/source/kernel/locking/semaphore.c#L40


Condition variables should be used as a place to wait and be notified. They are not the condition itself and they are not events. The condition is contained in the surrounding programming logic. The typical usage pattern of condition variables is

```C
// safely examine the condition, prevent other threads from
// altering it
pthread_mutex_lock (&lock);
while ( SOME-CONDITION is false)
    pthread_cond_wait (&cond, &lock);

// Do whatever you need to do when condition becomes true
do_stuff();
pthread_mutex_unlock (&lock);
```

On the other hand, a thread, signaling the condition variable, typically looks like

```C
// ensure we have exclusive access to whathever comprises the condition
pthread_mutex_lock (&lock);

ALTER-CONDITION

// Wakeup at least one of the threads that are waiting on the condition (if any)
pthread_cond_signal (&cond);

// allow others to proceed
pthread_mutex_unlock (&lock)
```

http://www.osdever.net/tutorials/view/multiprocessing-support-for-hobby-oses-explained

https://github.com/levex/osdev

http://download.intel.com/design/archives/processors/pro/docs/24201606.pdf
http://www.uruk.org/mps/
https://github.com/naredula-jana/Jiny-Kernel/blob/master/arch/x86_64/smp/mptables.c


https://www.amazon.com/exec/obidos/ASIN/1514111888


http://www.intel.com/content/www/us/en/io/i-o-controller-hub-7-datasheet.html

https://github.com/mallardtheduck/osdev/blob/master/src/modules/ata/dma.cpp


https://github.com/littlekernel/lk

http://akaros.cs.berkeley.edu/lxr/akaros/+code=pci_init

VSCODE
======

```
// Place your settings in this file to overwrite the default settings
{
    //"editor.fontFamily": "Roboto Mono for Powerline",
    "editor.fontFamily": "Fira Code Retina",
    //"editor.fontWeight": "400",
    "editor.fontSize": 14,
    "editor.lineHeight": 18,
    "window.zoomLevel": 0,
    "editor.cursorBlinking": "smooth",
    "editor.cursorStyle": "underline",
    "editor.renderWhitespace": "boundary",
    "editor.minimap.enabled": true,
    "editor.minimap.renderCharacters": false,
    "editor.detectIndentation": false,
    "editor.renderIndentGuides": false,
    "editor.mouseWheelZoom": true,
    "files.eol": "\n",
    "workbench.colorTheme": "Ayu Dark",
    "workbench.iconTheme": "material-icon-theme",
    "[cpp]": {
        "editor.quickSuggestions": false
    },
    "[c]": {
        "editor.quickSuggestions": false
    }
}
```

http://www.selasky.org/hans_petter/isdn4bsd/sources/module/sys/freebsd_pcireg.h


### TLB

https://github.com/yandex/smart/blob/master/arch/x86/mm/tlb.c
```smp_call_function_many```


### TTY

```C
printf(string)
{
    spinlock_lock(&queue.lock);
    queue.push_back(string)
    spinlock_unlock(&queue.lock);

    if (compare_and_swap_32(&queue.run_lock, 0, 1) != 0)
        return;

    while(1) {
        spinlock_lock(&queue.lock);
        string = queue.pop();
        if (!string) {
            compare_and_swap_32(&queue.run_lock, 1, 0);
            spinlock_unlock(&queue.lock);
            return;
        }
        spinlock_unlock(&queue.lock);

        // draw
    }
}
```


```C
void cond_wait(struct condition_t* c, struct spinlock_t* lock)
{
    struct cpu_desc_t* cpu = cpu_lock_splhi();

    spinlock_lock(&c->lock);
    thread_list_push(&c->threads, cpu->cur_thread);
    spinlock_unlock(&c->lock);

    spinlock_unlock(lock);
    sched_yield_locked(cpu);
    spinlock_lock(lock);

    cpu_unlock_splx(cpu);
}

void cond_signal(struct condition_t* c)
{
    int spl = cpu_splhi();
    spinlock_lock(&c->lock);
    if (thread_list_empty(&c->threads)) {
        spinlock_unlock(&c->lock);
        cpu_splx(spl);
        return;
    }

    struct thread_t* t = thread_list_pop(&c->threads);
    struct cpu_desc_t* cpu = cpu_lock_id(t->cpu_id);
    t->state = THREAD_STATE_RUNNING;
    cpu_unlock(cpu);

    spinlock_unlock(&c->lock);
    cpu_splx(spl);
}
```