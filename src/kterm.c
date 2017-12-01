#include "kterm.h"
#include "kernel.h"
#include "stdio.h"
#include "string.h"
#include "kbd_8042.h"
#include "thread.h"
#include "console.h"
#include "cpu.h"
#include "io_apic.h"
#include "local_apic.h"
#include "imps.h"
#include "vm_boot.h"
#include "vm_page.h"

typedef void (*kterm_cmd_fn)(int argc, const char* argv[]);

struct kterm_cmd_t {
    const char* name;
    kterm_cmd_fn cmd_fn;
};

#define MAX_KTERM_CMDS 16
static struct kterm_cmd_t commands[MAX_KTERM_CMDS];
static uint32_t num_commands;

static void kterm_add_cmd(const char* name, kterm_cmd_fn fn)
{
    check(num_commands < MAX_KTERM_CMDS);
    struct kterm_cmd_t* cmd = &commands[num_commands++];
    cmd->name = name;
    cmd->cmd_fn = fn;
}

static void kterm_exec(const char* str)
{
    char tmp[64];
    strcpy(tmp, str);

    int argc = 0;
    const char* args[32];
    char* p = tmp;
    while (*p) {
        while (*p && *p == ' ')
            *p++ = 0;
        args[argc++] = p;
        while (*p && *p != ' ')
            ++p;
    }

    if (argc > 0) {
        for (uint32_t i = 0; i < num_commands; ++i) {
            struct kterm_cmd_t* cmd = &commands[i];
            if (!strcmp(cmd->name, args[0])) {
                cmd->cmd_fn(argc, args);
                return;
            }
        }
    }

    printf("unknown command\n");
}

static void pmap_show_cmd(int argc, const char* argv[])
{
    struct boot_info_t* boot_info = kernel_boot_info();
    for (uint32_t i = 0; i < boot_info->num_mmap; ++i) {
        const struct boot_info_mmap_t* m = &boot_info->mmap[i];
        printf("%16lx:%16lx %2d\n", m->addr, m->size, m->flags);
    }    
}

static void io_apic_show_cmd(int argc, const char* argv[])
{
    printf("io_apic: %016lx id: %d ver: %d num_redtbl: %d gsi_base: %d\n",
        (uintptr_t)io_apic.io_apic_addr,
        io_apic.id, io_apic.ver, io_apic.num_redtbl, io_apic.gsi_base);
}

static void local_apic_show_cmd(int argc, const char* argv[])
{

}

static void ipi_test_cmd(int argc, const char* argv[])
{
    if (argc > 1) {
        if (!strcmp(argv[1], "-self"))
            local_apic_ipi_self(0xf0);
        else if (!strcmp(argv[1], "-bcast"))
            local_apic_ipi_broadcast(0xf0);
        else if (!strcmp(argv[1], "-all"))
            local_apic_ipi_all(0xf0);
        else
            printf("bad option");
    } else {
        printf("ipi [-self,-bcast,-all]\n");
    }
}

static void vm_boot_dump_cmd(int argc, const char* argv[])
{
    vm_boot_dump();
}

static void vm_page_dump_cmd(int argc, const char* argv[])
{
    if (argc > 1) {
        if (!strcmp(argv[1], "-r"))
            page_db_dump_ranges(page_db);
        else if (!strcmp(argv[1], "-f"))
            page_db_dump_free_list(page_db);
    }
}

static void fb_info_cmd(int argc, const char* argv[])
{
    struct boot_info_t* boot_info = kernel_boot_info();
    printf("fb: %016lx pitch: %d width: %d height: %d bpp: %d\n",
        boot_info->fb_addr,
        boot_info->fb_pitch,
        boot_info->fb_width,
        boot_info->fb_height,
        boot_info->fb_bpp);
}

static void kterm_run(void);

void kterm_start()
{
    kterm_add_cmd("cpu", cpu_show_cmd);
    kterm_add_cmd("pmap", pmap_show_cmd);
    kterm_add_cmd("io_apic", io_apic_show_cmd);
    kterm_add_cmd("ipi", ipi_test_cmd);
    kterm_add_cmd("imps", imps_show_cmd);
    kterm_add_cmd("vm_boot", vm_boot_dump_cmd);
    kterm_add_cmd("vm_page", vm_page_dump_cmd);
    kterm_add_cmd("fb_info", fb_info_cmd);

    thread_create(kterm_run, 0x4000, 1);
}

#define MAX_CMD_LINE    256
#define MAX_TEMP_BUF    64

static void kterm_run()
{
    printf("\nEntering kernel terminal\n");

    char cmd_line[MAX_CMD_LINE] = "";
    int cmd_pos = 0;
    printf("# _");
    while (1) {
        char tmp[MAX_TEMP_BUF];
        char buf[MAX_TEMP_BUF];
        int n = kbd_read(buf);
        char* p = tmp;
        for (int i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == '\r') {
                /*
                if (cmd_pos > 0) {
                    cmd_line[cmd_pos] = 0;
                    cmd_pos = 0;
                    printf("\b\n%s\n# _", cmd_line);
                } else {
                    printf("\b\n# _");
                }*/  
                if (cmd_pos > 0) {
                    cmd_line[cmd_pos] = 0;
                    printf("\b\n");
                    kterm_exec(cmd_line);
                    cmd_pos = 0;
                    printf("# _");
                } else {
                    printf("\b\n# _");
                }
                p = tmp;
            } else if (ch == '\b') {
                if (cmd_pos > 0) {
                    --cmd_pos;
                    cmd_line[cmd_pos] = 0;
                    console_putstr("\b\b_");
                }
            } else {
                *p++ = ch;
                cmd_line[cmd_pos++] = ch;
            }
        }
        if (p > tmp) {
            *p = 0;
            printf("\b%s_", tmp);
        }
    }    
}
