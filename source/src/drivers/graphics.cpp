#include "kernel.h"
#include "graphics.h"

Framebuffer fb;

// Informujemy kompilator, że ta tablica istnieje w innym pliku (font_data.cpp)
extern unsigned char IBM_VGA_8x16_bin[];
extern unsigned char wallpaper_bmp[];

extern "C" {

void graphics_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            graphics_put_pixel(x + j, y + i, color);
        }
    }
}

}


extern "C" void graphics_draw_char(int x, int y, unsigned char c, uint32_t color) {
    // Każdy znak to 16 bajtów. Adres początku znaku:
    unsigned char* glyph = &IBM_VGA_8x16_bin[c * 16];

    for (int i = 0; i < 16; i++) {       // 16 linii wysokości
        uint8_t row = glyph[i];          // Pobierz bajt (8 bitów) dla danej linii
        for (int j = 0; j < 8; j++) {    // Przeleć po bitach od lewej do prawej
            if (row & (0x80 >> j)) {     // Jeśli bit jest zapalony
                graphics_put_pixel(x + j, y + i, color);
            }
        }
    }
}

extern "C" void graphics_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) return;
    
    // Obliczanie adresu z uwzględnieniem pitch (kluczowe dla HD/FullHD)
    uint32_t* pixel_addr = (uint32_t*)(fb.address + (y * fb.pitch) + (x * 4));
    *pixel_addr = color;
}

extern "C" void graphics_print(int x, int y, const char* str, uint32_t color) {
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            y += 16;
            cur_x = x;
        } else {
            graphics_draw_char(cur_x, y, *str, color);
            cur_x += 8; // Szerokość fontu
        }
        str++;
    }
}

extern "C" void graphics_clear_screen(uint32_t color) {
    // Bardzo szybkie czyszczenie - traktujemy framebuffer jako długą tablicę
    uint32_t* dest = (uint32_t*)fb.address;
    uint32_t total_pixels = fb.height * (fb.pitch / 4);
    for (uint32_t i = 0; i < total_pixels; i++) {
        dest[i] = color;
    }
}

void graphics_draw_bmp() {
    // BMP Header ma 54 bajty. Dane o pixelach zaczynają się od adresu zapisanego w offset 10.
    uint32_t data_offset = *(uint32_t*)&wallpaper_bmp[10];
    int32_t width = *(int32_t*)&wallpaper_bmp[18];
    int32_t height = *(int32_t*)&wallpaper_bmp[22];
    uint16_t bpp = *(uint16_t*)&wallpaper_bmp[28];

    if (bpp != 24 && bpp != 32) return; // Obsługujemy tylko standardowe formaty

    unsigned char* pixel_data = &wallpaper_bmp[data_offset];

    // BMP jest zapisane od dołu do góry!
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x >= fb.width || y >= fb.height) continue;

            uint32_t color;
            if (bpp == 24) {
                // 24-bit BMP: B, G, R (3 bajty)
                int i = (y * width + x) * 3;
                color = (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            } else {
                // 32-bit BMP: B, G, R, A (4 bajty)
                int i = (y * width + x) * 4;
                color = (pixel_data[i+3] << 24) | (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            }

            // Rysujemy od tyłu (height - 1 - y), żeby obrazek nie był do góry nogami
            graphics_put_pixel(x, height - 1 - y, color);
        }
    }
}

void graphics_draw_bmp_centered() {
    uint32_t data_offset = *(uint32_t*)&wallpaper_bmp[10];
    int32_t img_w = *(int32_t*)&wallpaper_bmp[18];
    int32_t img_h = *(int32_t*)&wallpaper_bmp[22];
    uint16_t bpp = *(uint16_t*)&wallpaper_bmp[28];

    if (bpp != 24 && bpp != 32) return;

    // Obliczamy offsety dla centrowania
    int start_x = (fb.width - img_w) / 2;
    int start_y = (fb.height - img_h) / 2;

    unsigned char* pixel_data = &wallpaper_bmp[data_offset];

    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w; x++) {
            // Obliczamy docelowe współrzędne na ekranie
            int screen_x = start_x + x;
            int screen_y = start_y + (img_h - 1 - y); // BMP jest odwrócone pionowo

            // Sprawdzamy, czy nie wychodzimy poza krawędzie ekranu
            if (screen_x < 0 || screen_x >= (int)fb.width || 
                screen_y < 0 || screen_y >= (int)fb.height) {
                continue;
            }

            uint32_t color;
            if (bpp == 24) {
                int i = (y * img_w + x) * 3;
                color = (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            } else {
                int i = (y * img_w + x) * 4;
                color = (pixel_data[i+3] << 24) | (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            }

            graphics_put_pixel(screen_x, screen_y, color);
        }
    }
}