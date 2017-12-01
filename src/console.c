#include "types.h"
#include "console.h"
#include "string.h"
#include "kernel.h"
#include "spinlock.h"
#include "vga_cons.h"
#include "fb_cons.h"

static void (*console_putchar_fn)(unsigned char);
static void (*console_clear_char_fn)(unsigned int);
static void (*console_clear_line_fn)(void);

static struct spinlock_t lock;

void console_init()
{
    fb_cons_init();
    fb_cons_clear();

    console_putchar_fn = fb_cons_putchar;
    console_clear_char_fn = fb_cons_clear_char;
    console_clear_line_fn = fb_cons_clear_line;
}

void console_putstr(const char* str)
{
    int spl = cpu_splhi();
    spinlock_lock(&lock);

    while (*str)
        console_putchar_fn(*str++);

    spinlock_unlock(&lock);
    cpu_splx(spl);
}

void console_putchar(unsigned char ch)
{
    int spl = cpu_splhi();
    spinlock_lock(&lock);

    console_putchar_fn(ch);

    spinlock_unlock(&lock);
    cpu_splx(spl);
}

void console_clear_char(unsigned int num)
{
    int spl = cpu_splhi();
    spinlock_lock(&lock);

    console_clear_char_fn(num);

    spinlock_unlock(&lock);
    cpu_splx(spl);
}

void console_clear_line()
{
    int spl = cpu_splhi();
    spinlock_lock(&lock);

    console_clear_line_fn();

    spinlock_unlock(&lock);
    cpu_splx(spl);
}

#define MAX_CONSOLE_BUF 65536

struct cons_buffer_t {
    uint32_t pos;
    uint8_t buf[MAX_CONSOLE_BUF];
};

static struct cons_buffer_t cons_buf;

static uint32_t cons_buffer_alloc(uint32_t size)
{
    if (cons_buf.pos + size <= MAX_CONSOLE_BUF) {
        uint32_t pos = cons_buf.pos;
        cons_buf.pos += size;
        return pos;
    }

    check(size < MAX_CONSOLE_BUF);
    cons_buf.pos = 0;
    return 0;
}

struct cons_msg_t {
    uint32_t pos;
    uint32_t size;
};

#define MAX_MSG_QUEUE   32
struct cons_msg_queue_t {
    struct cons_msg_t msg[MAX_MSG_QUEUE];
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    volatile uint32_t run_lock;
    struct spinlock_t lock;
};

static struct cons_msg_queue_t cons_msg_queue;

static void cons_msg_push(uint32_t pos, uint32_t size)
{
    uint32_t write_pos = cons_msg_queue.write_pos++;
    uint32_t index = write_pos & (MAX_MSG_QUEUE-1);

    cons_msg_queue.msg[index].pos = pos;
    cons_msg_queue.msg[index].size = size;
}

static struct cons_msg_t* cons_msg_pop()
{
    if (cons_msg_queue.read_pos < cons_msg_queue.write_pos) {
        uint32_t read_pos = cons_msg_queue.read_pos++;
        uint32_t index = read_pos & (MAX_MSG_QUEUE-1);

        return &cons_msg_queue.msg[index];
    }

    return NULL;
}

static void cons_msg_putstr(const char* str, uint32_t len)
{
    uint32_t pos = cons_buffer_alloc(len);
    memcpy(&cons_buf.buf[pos], str, len);
    cons_msg_push(pos, len);
}

static void cons_putstr(const char* str, uint32_t len)
{
    int spl = cpu_splhi();
    spinlock_lock(&cons_msg_queue.lock);
    cons_msg_putstr(str, len);
    spinlock_unlock(&cons_msg_queue.lock);
    cpu_splx(spl);

    if (compare_and_swap_32(&cons_msg_queue.run_lock, 0, 1) != 0)
        return;

    if (!spl)
        cpu_enable_interrupts();

    while(1) {
        int s = cpu_splhi();
        spinlock_lock(&cons_msg_queue.lock);
        struct cons_msg_t* msg = cons_msg_pop();
        if (!msg) {
            compare_and_swap_32(&cons_msg_queue.run_lock, 1, 0);
            spinlock_unlock(&cons_msg_queue.lock);
            cpu_splx(spl);
            return;
        }
        spinlock_unlock(&cons_msg_queue.lock);
        cpu_splx(s);

        // draw
    }
}
