cmake_minimum_required(VERSION 3.0)
project (smor C ASM)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_COMPILER "x86_64-pc-elf-gcc")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin")

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

set_property(SOURCE src/boot.S PROPERTY LANGUAGE C)
set_property(SOURCE src/isr.S PROPERTY LANGUAGE C)

include_directories(src)

add_executable(kernel
    src/boot.S
    src/isr.S
    src/pci.c
    src/pic.c
    src/pit.c
    src/acpi.c
    src/io_apic.c
    src/local_apic.c
    src/imps.c
    src/vm_boot.c
    src/vm_hat.c
    src/vm_page.c
    src/vm_cache.c
    src/vm_alloc.c
    src/vm_mmap.c
    src/cpu.c
    src/cpu_exception.c
    src/interrupt.c
    src/thread.c
    src/sched.c
    src/cond.c
    src/semaphore.c
    src/slab.c
    src/kmalloc.c
    src/console.c
    src/vga_cons.c
    src/fb_cons.c
    src/fb_font.c
    src/kbd_8042.c
    src/ata.c
    src/string.c
    src/vsprintf.c
    src/multiboot.c
    src/kernel.c
    src/kterm.c
    src/main.c)

target_compile_options(kernel PUBLIC
    -c -Os -Wall -Wmissing-prototypes 
    -Werror-implicit-function-declaration
    -Wno-unused-function
    -fno-strict-aliasing -fno-common
    -ffreestanding -mcmodel=kernel -mno-red-zone)

SET(CMAKE_C_LINK_FLAGS "-ffreestanding -mcmodel=kernel -mno-red-zone -T ../link.ld -n -nostdlib -Wl,--build-id=none")

add_custom_command(TARGET kernel
        POST_BUILD
        COMMAND /bin/cp -rf ../bin/kernel ../bin/iso/kernel
        COMMAND grub-mkrescue -o ../bin/grub.iso ../bin/iso)

