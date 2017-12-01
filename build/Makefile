CC = x86_64-pc-elf-gcc

CFLAGS	= -c -Os
CFLAGS	+= -Wall -Wmissing-prototypes# -Wstrict-prototypes
CFLAGS	+= -Werror-implicit-function-declaration
CFLAGS	+= -fomit-frame-pointer -fno-strict-aliasing -fno-common
CFLAGS	+= -ffreestanding -mcmodel=kernel -mno-red-zone

LDFLAGS	= -ffreestanding -mcmodel=kernel -T link.ld -n -nostdlib -Wl,--build-id=none

vpath %.c ../src
vpath %.S ../src

S_SOURCES = boot.S isr.S
C_SOURCES = pic.c pit.c acpi.c apic.c vm.c vm_page.c cpu.c \
			console.c vga_cons.c fb_font.c fb_cons.c \
			kbd_8042.c string.c vsprintf.c multiboot.c kernel.c main.c

S_OBJECTS = $(patsubst %.S,obj/%.o,$(S_SOURCES))
C_OBJECTS = $(patsubst %.c,obj/%.o,$(C_SOURCES))

OBJECTS = $(S_OBJECTS) $(C_OBJECTS)

all: iso

iso: kernel
	/bin/cp -rf kernel ../bin/iso/kernel
	grub-mkrescue -o ../bin/grub.iso ../bin/iso

kernel: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -rf obj
	rm -rf kernel

$(OBJECTS): | obj

obj:
	@mkdir -p $@

obj/%.o: %.S
	$(CC) $(CFLAGS) $< -o $@
obj/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
