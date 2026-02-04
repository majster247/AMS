#include "kernel.h"
#include "graphics.h"

Framebuffer fb;

// Informujemy kompilator, że ta tablica istnieje w innym pliku (font_data.cpp)
extern unsigned char IBM_VGA_8x16_bin[];
extern unsigned char wallpaper_bmp[];

extern "C" {

uint32_t* backbuffer = nullptr;
uint32_t static_backbuffer[1280 * 720] __attribute__((aligned(4096))); 

void graphics_init_double_buffer() {
    backbuffer = static_backbuffer;
    write_serial_string("[GRAPHICS] Statyczny backbuffer pod adresem: ");
    write_serial_hex((uint64_t)backbuffer);
    write_serial_string("\n");
}

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

void graphics_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= 1280 || y < 0 || y >= 720) return;
    if (!backbuffer) return; // Bezpiecznik
    backbuffer[y * 1280 + x] = color;
}

void graphics_flip() {
    graphics_acquire(); // Blokujemy dostęp innym taskom
    
    uint32_t* dest = (uint32_t*)fb.address;
    uint32_t* src = backbuffer;
    
    // Kopiowanie linia po linii (bezpieczniejsze niż jeden wielki memcpy)
    for (uint32_t y = 0; y < 720; y++) {
        memcpy(&dest[y * (fb.pitch / 4)], &src[y * 1280], 1280 * 4);
    }
    
    graphics_release(); // Zwalniamy blokadę
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

void graphics_clear_screen(uint32_t color) {
    if (!backbuffer) return;
    // Jeśli kolor to 0 (czarny), memset jest błyskawiczny
    if (color == 0) {
        memset(backbuffer, 0, 1280 * 720 * 4);
    } else {
        // Dla innych kolorów musisz użyć pętli, ale upewnij się, że jest zoptymalizowana
        uint32_t total = 1280 * 720;
        for (uint32_t i = 0; i < total; i++) backbuffer[i] = color;
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

volatile bool screen_lock = false;

void graphics_acquire() {
    while (__sync_lock_test_and_set(&screen_lock, 1)) {
        // Czekaj, aż inny proces puści ekran (spin-lock)
    }
}

void graphics_release() {
    __sync_lock_release(&screen_lock);
}

void graphics_get_block(int x, int y, int w, int h, uint32_t* buffer) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int src_x = x + i;
            int src_y = y + j;
            if (src_x >= 0 && src_x < 1280 && src_y >= 0 && src_y < 720) {
                // Pobieramy z BUFORA
                buffer[j * w + i] = backbuffer[src_y * 1280 + src_x];
            } else {
                buffer[j * w + i] = 0;
            }
        }
    }
}

void graphics_put_block(int x, int y, int w, int h, uint32_t* buffer) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            graphics_put_pixel(x + i, y + j, buffer[j * w + i]);
        }
    }
}

void graphics_draw_bmp_part(int x_start, int y_start, int w, int h) {
    uint32_t data_offset = *(uint32_t*)&wallpaper_bmp[10];
    int32_t img_w = *(int32_t*)&wallpaper_bmp[18];
    int32_t img_h = *(int32_t*)&wallpaper_bmp[22];
    uint16_t bpp = *(uint16_t*)&wallpaper_bmp[28];

    unsigned char* pixel_data = &wallpaper_bmp[data_offset];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int dest_x = x_start + x;
            int dest_y = y_start + y;

            // 1. HARD CORNER CASE: Nie wychodź poza ekran (1280x720)
            if (dest_x < 0 || dest_x >= 1280 || dest_y < 0 || dest_y >= 720) continue;

            // 2. Nie wychodź poza wymiary pliku BMP
            if (dest_x >= img_w || dest_y >= img_h) continue;

            // 3. Obliczanie pozycji w BMP (z uwzględnieniem flip_y)
            int flipped_y = (img_h - 1) - dest_y;
            
            // Bezpiecznik: jeśli y wyszedł ujemny po flipie
            if (flipped_y < 0) continue;

            uint32_t color;
            if (bpp == 24) {
                uint64_t i = ((uint64_t)flipped_y * img_w + dest_x) * 3;
                color = (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            } else {
                uint64_t i = ((uint64_t)flipped_y * img_w + dest_x) * 4;
                color = (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            }

            graphics_put_pixel(dest_x, dest_y, color);
        }
    }
}

void clear_window_area(int x_start, int y_start, int w, int h) {
    // 1. Najpierw wypełnij wszystko kolorem tła pulpitu (np. ciemnoszary)
    // To załatwi "smugi" poza obszarem tapety
    graphics_draw_rect(x_start, y_start, w, h, 0x1D1D1D);

    // 2. Teraz, jeśli ten fragment pokrywa się z tapetą, dorysuj tam tapetę
    // Obliczamy gdzie na ekranie zaczyna się tapeta (zakładając 800x600 na 1280x720)
    int bmp_screen_x = (1280 - 800) / 2;
    int bmp_screen_y = (720 - 600) / 2;
    int bmp_w = 800;
    int bmp_h = 600;

    // Sprawdzamy czy okno w ogóle nachodzi na tapetę
    if (x_start + w < bmp_screen_x || x_start > bmp_screen_x + bmp_w ||
        y_start + h < bmp_screen_y || y_start > bmp_screen_y + bmp_h) {
        return; // Okno jest całkowicie poza tapetą, wystarczy prostokąt
    }

    // Jeśli nachodzi, wołamy Twoje draw_bmp_part, ale z poprawionym offsetem!
    // Musimy przekazać funkcji informację, że współrzędne ekranowe x_start 
    // muszą zostać pomniejszone o początek tapety na ekranie
    graphics_draw_bmp_part_offset(x_start, y_start, w, h, bmp_screen_x, bmp_screen_y);
}

void graphics_draw_bmp_part_offset(int x_start, int y_start, int w, int h, int off_x, int off_y) {
    // 1. Wyciągamy dane z nagłówka BMP (podobnie jak w głównej funkcji rysującej)
    uint32_t data_offset = *(uint32_t*)&wallpaper_bmp[10];
    int32_t img_w = *(int32_t*)&wallpaper_bmp[18];
    int32_t img_h = *(int32_t*)&wallpaper_bmp[22];
    uint16_t bpp = *(uint16_t*)&wallpaper_bmp[28];

    // Tylko 24-bitowe BMP na razie (najczęstsze)
    if (bpp != 24) return;

    unsigned char* pixel_data = &wallpaper_bmp[data_offset];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int screen_x = x_start + x;
            int screen_y = y_start + y;

            // MAPOWANIE: Gdzie ten piksel ekranu jest w pliku BMP?
            int bmp_x = screen_x - off_x;
            int bmp_y = screen_y - off_y;

            // Sprawdzamy czy nie wychodzimy poza zakres obrazka BMP
            if (bmp_x < 0 || bmp_x >= img_w || bmp_y < 0 || bmp_y >= img_h) continue;
            
            // Sprawdzamy czy nie wychodzimy poza ekran (bezpiecznik przed vCRASH)
            if (screen_x < 0 || screen_x >= (int)fb.width || screen_y < 0 || screen_y >= (int)fb.height) continue;

            // BMP jest zapisane od dołu do góry
            int flipped_y = (img_h - 1) - bmp_y;
            
            // Obliczamy index (3 bajty na piksel: B, G, R)
            uint64_t i = ((uint64_t)flipped_y * img_w + bmp_x) * 3;
            
            uint32_t color = (pixel_data[i+2] << 16) | (pixel_data[i+1] << 8) | pixel_data[i];
            
            graphics_put_pixel(screen_x, screen_y, color);
        }
    }
}

