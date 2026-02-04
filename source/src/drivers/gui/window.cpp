#include "window.h"
#include "graphics.h"

void draw_window(Window* win) {
    // Ramka
    for (int x = win->x; x < win->x + win->width; x++) {
        graphics_put_pixel(x, win->y, 0x000000); // Górna krawędź
        graphics_put_pixel(x, win->y + win->height - 1, 0x000000); // Dolna krawędź
    }
    for (int y = win->y; y < win->y + win->height; y++) {
        graphics_put_pixel(win->x, y, 0x000000); // Lewa krawędź
        graphics_put_pixel(win->x + win->width - 1, y, 0x000000); // Prawa krawędź
    }

    // Wypełnienie
    for (int y = win->y + 1; y < win->y + win->height - 1; y++) {
        for (int x = win->x + 1; x < win->x + win->width - 1; x++) {
            graphics_put_pixel(x, y, win->color);
        }
    }

    // Tytuł (prosty sposób)
    int title_len = 0;
    while (win->title[title_len] != '\0') title_len++;
    int title_x = win->x + (win->width - title_len * 8) / 2; // Centrowanie tytułu
    int title_y = win->y + 4;

    for (int i = 0; i < title_len; i++) {
        // Zakładamy funkcję graphics_draw_char(x, y, char, color)
        graphics_draw_char(title_x + i * 8, title_y, win->title[i], 0xFFFFFF);
    }
}