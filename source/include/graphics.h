#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

struct Framebuffer {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
};

extern Framebuffer fb;

extern "C" {
    void graphics_put_pixel(int x, int y, uint32_t color);
    void graphics_clear_screen(uint32_t color);
    void graphics_draw_char(int x, int y, unsigned char c, uint32_t color);
    void graphics_print(int x, int y, const char* str, uint32_t color);
    void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
    void graphics_draw_bmp();
    void graphics_draw_bmp_centered();
}

//Graphics utilities
void draw_status_bar();

#endif