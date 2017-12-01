#include "kbd_8042.h"
#include "cpu.h"
#include "interrupt.h"
#include "local_apic.h"
#include "spinlock.h"
#include "cond.h"
#include "stdio.h"

#define KBD_BUFSIZE 16

struct keyboard_t {
    struct spinlock_t lock;
    struct condition_t cond;
    volatile uint32_t read_pos;
    volatile uint32_t write_pos;
    uint8_t buf[KBD_BUFSIZE];
    uint32_t shift:1;
    uint32_t ctrl:1;
    uint32_t alt:1;
    uint32_t caps:1;
    uint32_t num:1;
};

static struct keyboard_t kbd;

static char normal[] = {
    0x00,0x1B,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']',0x0D,0x80,
    'a','s','d','f','g','h','j','k','l',';',047,0140,0x80,
    0134,'z','x','c','v','b','n','m',',','.','/',0x80,
    '*',0x80,' ',0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
    0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
    0x80,0x80,0x80,'0',0177
};


static char shifted[] = {
    0,033,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}',015,0x80,
    'A','S','D','F','G','H','J','K','L',':',042,'~',0x80,
    '|','Z','X','C','V','B','N','M','<','>','?',0x80,
    '*',0x80,' ',0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
    0x80,0x80,0x80,0x80,'7','8','9',0x80,'4','5','6',0x80,
    '1','2','3','0',177
};

static char kbd_decode(uint8_t code)
{
    if (code == 72)
        return '*';

    if (kbd.caps) {
        char tmp = normal[code];
        if (tmp >= 'a' && tmp <= 'z')
            return (kbd.shift ? normal[code] : shifted[code]);
        else
            return (kbd.shift ? shifted[code] : normal[code]);
    }
    return (kbd.shift ? shifted[code] : normal[code]);
}

static bool kbd_check_special(uint8_t key)
{
    switch(key) {
        case 0x36:
        case 0x2A: 
            kbd.shift = 1;
            break;
        case 0xB6:
        case 0xAA:
            kbd.shift = 0;
            break;
        case 0x1D:
            kbd.ctrl = 1;
            break;
        case 0x9D:
            kbd.ctrl = 0;
            break;
        case 0x38:
            kbd.alt = 1;
            break;
        case 0xB8:
            kbd.alt = 0;
            break;
        case 0x3A:
        case 0x45:
            break;
        case 0xBA:
            kbd.caps = !kbd.caps;
            break;
        case 0xC5:
            kbd.num = !kbd.num;
            break;
        case 0xE0:
            break;
        default:
            return false;
    }

    return true;
}

static void irq_handler(struct isr_frame_t* frame)
{
    uint8_t code = inb(0x60);
    if (kbd_check_special(code) || code & 0x80)
        return;

    // we're already at splhi
    spinlock_lock(&kbd.lock);

    kbd.buf[(kbd.write_pos++) % KBD_BUFSIZE] = code;

    cond_signal(&kbd.cond);
    spinlock_unlock(&kbd.lock);
}

void kbd_8042_init()
{
    spinlock_init(&kbd.lock);
    cond_init(&kbd.cond);

    uint32_t apic_id = local_apic_id();
    intr_register_irq_handler(IRQ_KEYBOARD, irq_handler);
    intr_irq_enable(IRQ_KEYBOARD, 1 << apic_id);
}

int kbd_read(char* out_buf)
{
    int spl = spinlock_lock_splhi(&kbd.lock);
    while (kbd.read_pos >= kbd.write_pos)
        cond_wait(&kbd.cond, &kbd.lock);

    int n = 0;
    uint8_t buf[KBD_BUFSIZE];
    while (kbd.read_pos < kbd.write_pos) {
        uint8_t code = kbd.buf[(kbd.read_pos++) % KBD_BUFSIZE];
        buf[n++] = code;
    }

    spinlock_unlock_splx(&kbd.lock, spl);
    
    for (int i = 0; i < n; ++i)
        out_buf[i] = kbd_decode(buf[i]);

    return n;
}