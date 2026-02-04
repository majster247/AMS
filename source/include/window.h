#pragma once
#include <stdint.h>

struct Window {
    int32_t x, y;
    int32_t width, height;
    uint32_t color;
    const char* title;
    bool is_dragging;
};

void draw_window(Window* win);