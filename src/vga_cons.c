#include "vga_cons.h"
#include "types.h"
#include "kernel.h"
#include "string.h"

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
    return (uint16_t) uc | (uint16_t) color << 8;
}

struct console_t {
    uint16_t* fb;
    uint16_t x_pos;
    uint16_t y_pos;
    uint16_t width;
    uint16_t height;
    uint8_t color;
};

static struct console_t console;

void vga_cons_init()
{
    console.fb = (uint16_t*)(KERNEL_BASE|0xB8000);
    console.x_pos = 0;
    console.y_pos = 0;
    console.width = 80;
    console.height = 25;
    console.color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void vga_cons_putchar(unsigned char ch)
{
    if (console.x_pos >= console.width) {
        console.x_pos = 0;
        ++console.y_pos;
    }
    if (ch == '\t')
        console.x_pos += 4;
    if (ch == '\n') {
        console.x_pos = 0;
        ++console.y_pos;
    }
    if (console.y_pos >= console.height) {
        memcpy(console.fb, 
               console.fb + console.width,
               console.width * (console.height - 1) * sizeof(uint16_t));
        console.y_pos = console.height-1;
        uint16_t* p = console.fb + console.y_pos * console.width;
        for (int i = 0; i < console.width; ++i)
            p[i] = 0;
    }
    
    if (ch >= 32) 
        console.fb[console.y_pos * console.width + console.x_pos++] = vga_entry(ch, console.color);
}

void vga_cons_clear()
{
}
