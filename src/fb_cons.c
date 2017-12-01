#include "fb_cons.h"
#include "types.h"
#include "string.h"
#include "kernel.h"
#include "vm_boot.h"
#include "cpu.h"

#define RED_MASK    0xf800  // 1111100000000000
#define GREEN_MASK  0x7e00  // 0000011111100000

#define RGB_565(r, g, b) ((b >> 3) | ((g << 3) & GREEN_MASK) | ((r << 8) & RED_MASK))

extern uint8_t glyphs[96][9*16];

struct fb_cons_t {
    uint16_t* fb_addr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint16_t x_pos;
    uint16_t y_pos;
    uint16_t font_width;
    uint16_t font_height;
    uint16_t clear_color;
};

static struct fb_cons_t cons;

void fb_cons_init()
{
    struct boot_info_t* boot_info = kernel_boot_info();

    uintptr_t fb_addr = (uintptr_t)boot_info->fb_addr;
    uint64_t fb_size = boot_info->fb_pitch * boot_info->fb_height;
    vm_boot_map_range(fb_addr, fb_addr, fb_size);

    cons.fb_addr = (uint16_t*)boot_info->fb_addr;
    cons.pitch = boot_info->fb_pitch;
    cons.width = boot_info->fb_width;
    cons.height = boot_info->fb_height;
    cons.x_pos = 0;
    cons.y_pos = 0;
    cons.font_width = 8;
    cons.font_height = 16;
    cons.clear_color = RGB_565(0,0,0xAA);
}

void fb_cons_clear()
{
    uint8_t* ptr = (uint8_t*)cons.fb_addr;
    for (uint32_t y = 0; y < cons.height; ++y) {
        uint16_t* p = (uint16_t*)ptr;
        uint32_t n = cons.width;
        while (n--)
            *p++ = cons.clear_color;

        ptr += cons.pitch;
    }
}

static void fb_draw_char(unsigned char ch)
{
    const uint16_t color = 0xFFFF;//RGB_565(0xFF,0xFF,0xFF);
    
    uint8_t* ptr = (uint8_t*)cons.fb_addr + cons.y_pos*cons.pitch + cons.x_pos*2;
    const uint8_t* glyph = glyphs[ch-31];

    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 8; ++x) {
            uint16_t* p = (uint16_t*)ptr + x;
            uint8_t g = glyph[y*9+x];
            if (g == 0xFF)
                *p = color;
            else if (g == 0xbb)
                *p = 0x0;
        }

        ptr += cons.pitch;
    }    
}

static void fb_clear_char(uint16_t x, unsigned int num)
{
    uint8_t* ptr = (uint8_t*)cons.fb_addr
                 + cons.y_pos*cons.pitch
                 + x * cons.font_width * 2;

    uint32_t w = num * cons.font_width;
    for (int y = 0; y < 16; ++y) {
        uint16_t* p = (uint16_t*)ptr;
        uint32_t n = w;
        while (n--)
            *p++ = cons.clear_color;

        ptr += cons.pitch;
    }
}

void fb_cons_putchar(unsigned char ch)
{
    if (cons.x_pos >= cons.width) {
        cons.x_pos = 0;
        cons.y_pos += cons.font_height;
    }
    if (ch == '\t') {
        cons.x_pos += 4 * cons.font_width;
        return;
    }
    if (ch == '\b') {
        fb_cons_clear_char(1);
        return;
    }
    if (ch == '\n' || ch == '\r') {
        cons.x_pos = 0;
        cons.y_pos += cons.font_height;
        return;
    }
    if (cons.y_pos >= cons.height) {
        memcpy(cons.fb_addr,
               (uint8_t*)cons.fb_addr + cons.pitch * cons.font_height,
               cons.pitch * (cons.height - cons.font_height));
        cons.y_pos = cons.height - cons.font_height;
        uint8_t* ptr = (uint8_t*)cons.fb_addr + cons.y_pos * cons.pitch;
        for (uint32_t y = 0; y < cons.font_height; ++y) {
            uint16_t* p = (uint16_t*)ptr;
            uint32_t n = cons.width;
            while (n--)
                *p++ = cons.clear_color;

            ptr += cons.pitch;
        }
    }
    
    if (ch >= 32) {
        fb_draw_char(ch);
        cons.x_pos += cons.font_width;
    }
}

void fb_cons_clear_char(unsigned int num)
{
    uint16_t x = cons.x_pos / cons.font_width;
    uint16_t x_pos = num > x ? 0 : x - num;
    unsigned int n_clear = x - x_pos;
    if (!n_clear)
        return;

    fb_clear_char(x_pos, n_clear);
    cons.x_pos = x_pos * cons.font_width;
}

void fb_cons_clear_line()
{
    unsigned int n_clear = cons.x_pos;
    if (!n_clear)
        return;

    fb_clear_char(0, n_clear);
    cons.x_pos = 0;
}
