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

extern uint32_t* backbuffer;

extern "C" {
    void graphics_put_pixel(int x, int y, uint32_t color);
    void graphics_clear_screen(uint32_t color);
    void graphics_draw_char(int x, int y, unsigned char c, uint32_t color);
    void graphics_print(int x, int y, const char* str, uint32_t color);
    void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
    void graphics_draw_bmp();
    void graphics_draw_bmp_centered();
    void graphics_draw_bmp_part(int x_start, int y_start, int w, int h);
    void graphics_init_double_buffer();
    void graphics_flip();

    void graphics_acquire();
    void graphics_release();

    void clear_window_area(int x_start, int y_start, int w, int h);
    void graphics_draw_bmp_part_offset(int x_start, int y_start, int w, int h, int off_x, int off_y);
}

//Graphics utilities
void draw_status_bar();
void update_clock_display();
void graphics_get_block(int x, int y, int w, int h, uint32_t* buffer);
void graphics_put_block(int x, int y, int w, int h, uint32_t* buffer);
void fill_screen(uint32_t color);


#endif