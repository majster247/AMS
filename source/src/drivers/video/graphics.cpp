#include "kernel.h"
#include "graphics.h"
#include "heap.h"

Framebuffer fb;

// Informujemy kompilator, że ta tablica istnieje w innym pliku (font_data.cpp)
extern unsigned char IBM_VGA_8x16_bin[];
extern unsigned char wallpaper_bmp[];

uint32_t* backbuffer = nullptr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern "C" {


void graphics_init_double_buffer() {
    fb_width = fb.width;  
    fb_height = fb.height;
    uint64_t buffer_size = fb_width * fb_height * 4; // 4 bajty na piksel

    backbuffer = (uint32_t*)malloc(buffer_size); // Dynamicznie alokowany backbuffer
    if(backbuffer == nullptr) {
        write_serial_string("[GRAPHICS] Błąd alokacji backbuffera!\n");
        return;
    }
    memset(backbuffer, 0, buffer_size);

    write_serial_string("[GRAPHICS] Malloc dynamicznego backbuffera pod adresem: ");
    write_serial_hex((uint64_t)backbuffer);
    write_serial_string("\n");
    //Rozmiar
    write_serial_string("[GRAPHICS] Rozmiar backbuffera (w KB): ");
    write_serial_dec(buffer_size/1024);
    write_serial_string(" KB\n");
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
    if (x < 0 || x >= (uint32_t)fb.width || y < 0 || y >= (uint32_t)fb.height) return;
    if (!backbuffer) return; // Bezpiecznik
    backbuffer[y * fb.width + x] = color;
}

void graphics_flip() {
    // Sprawdzamy, czy bufor jest ciągły w pamięci (bez przerw na końcach linii)
    // Pitch to długość linii w bajtach.
    if (fb.pitch == fb.width * 4) {
        // FAST PATH: Kopiujemy 3.6 MB jednym strzałem
        fast_memcpy64((void*)fb.address, backbuffer, (fb.width * fb.height * 4) / 8);
    } else {
        // SLOW PATH: Kopiowanie linia po linii (jeśli karta graficzna ma padding)
        for (uint32_t y = 0; y < fb.height; y++) {
            fast_memcpy64(
                (void*)((uint8_t*)fb.address + y * fb.pitch), // Cel z uwzględnieniem pitch
                backbuffer + y * fb.width,                    // Źródło (ciągłe)
                (fb.width * 4) / 8                            // Ilość 64-bitowych słów w linii
            );
        }
    }
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
        memset(backbuffer, 0, fb.width * fb.height * 4);
    } else {
        // Dla innych kolorów musisz użyć pętli, ale upewnij się, że jest zoptymalizowana
        uint32_t total = fb.width * fb.height;
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
            if (x >= (uint32_t)fb.width || y >= (int32_t)fb.height) continue;

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
            if (screen_x < 0 || screen_x >= (uint32_t)fb.width || 
                screen_y < 0 || screen_y >= (uint32_t)fb.height) {
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
    //while (__sync_lock_test_and_set(&screen_lock, 1)) {
        // Czekaj, aż inny proces puści ekran (spin-lock)
//}
}

void graphics_release() {
   // __sync_lock_release(&screen_lock);
}

void graphics_get_block(int x, int y, int w, int h, uint32_t* buffer) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int src_x = x + i;
            int src_y = y + j;
            if (src_x >= 0 && src_x < (uint32_t)fb.width && src_y >= 0 && src_y < (uint32_t)fb.height) {
                // Pobieramy z BUFORA
                buffer[j * w + i] = backbuffer[src_y * fb.width + src_x];
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
            if (dest_x < 0 || dest_x >= (uint32_t)fb.width || dest_y < 0 || dest_y >= (uint32_t)fb.height) continue;

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
    // 1. Najpierw czyścimy tłem (bezpiecznik)
    graphics_draw_rect(x_start, y_start, w, h, 0x1D1D1D);

    // 2. POBIERZ PRAWDZIWE WYMIARY Z NAGŁÓWKA BMP!
    int32_t img_w = *(int32_t*)&wallpaper_bmp[18];
    int32_t img_h = *(int32_t*)&wallpaper_bmp[22];

    // 3. Obliczamy faktyczną pozycję tapety na ekranie (Centrowanie)
    // Używamy fb.width i fb.height zamiast sztywnych 1280/720 dla elastyczności
    int bmp_screen_x = (fb.width - img_w) / 2;
    int bmp_screen_y = (fb.height - img_h) / 2;

    // 4. Sprawdzamy kolizję (Prostokąt okna vs Prostokąt tapety)
    // Jeśli okno jest poza tapetą, nie ma sensu jej rysować
    if (x_start + w <= bmp_screen_x || x_start >= bmp_screen_x + img_w ||
        y_start + h <= bmp_screen_y || y_start >= bmp_screen_y + img_h) {
        return; 
    }

    // 5. Rysujemy fragment tapety z poprawnym offsetem
    graphics_draw_bmp_part_offset(x_start, y_start, w, h, bmp_screen_x, bmp_screen_y);
}

void graphics_draw_bmp_part_offset(int x_start, int y_start, int w, int h, int off_x, int off_y) {
    uint32_t data_offset = *(uint32_t*)&wallpaper_bmp[10];
    int32_t img_w = *(int32_t*)&wallpaper_bmp[18];
    int32_t img_h = *(int32_t*)&wallpaper_bmp[22];
    
    // Pobieramy surowe dane
    unsigned char* pixel_data = &wallpaper_bmp[data_offset];
    
    // Zabezpieczenie: backbuffer musi istnieć
    if (!backbuffer) return;

    for (int y = 0; y < h; y++) {
        int screen_y = y_start + y;
        int bmp_y = screen_y - off_y;

        // Clip Y: Jeśli linia poza ekranem lub poza BMP -> pomiń całą linię
        if (screen_y < 0 || screen_y >= (int32_t)fb.height || bmp_y < 0 || bmp_y >= img_h) continue;

        // BMP Flip Y: Obliczamy, który to wiersz w pliku BMP (od dołu)
        int flipped_y = (img_h - 1) - bmp_y;

        // === OPTYMALIZACJA WSKAŹNIKÓW ===
        
        // 1. Wskaźnik na początek linii w Backbufferze (Cel)
        // Mamy 32-bitowy buffer (4 bajty na pixel), więc uint32_t*
        uint32_t* dest_line_ptr = backbuffer + (screen_y * fb.width); 

        // 2. Wskaźnik na początek linii w BMP (Źródło)
        // BMP 24-bit ma 3 bajty na pixel, więc unsigned char*
        unsigned char* src_line_ptr = pixel_data + ((uint64_t)flipped_y * img_w * 3);

        for (int x = 0; x < w; x++) {
            int screen_x = x_start + x;
            int bmp_x = screen_x - off_x;

            // Clip X: Pomiń piksel, jeśli poza zakresem
            if (screen_x < 0 || screen_x >= (int32_t)fb.width || bmp_x < 0 || bmp_x >= img_w) continue;

            // Zamiast liczyć (y*w+x) * 3, używamy offsetu w linii
            int src_offset = bmp_x * 3;
            
            // Czytamy B, G, R bezpośrednio
            uint8_t b = src_line_ptr[src_offset];
            uint8_t g = src_line_ptr[src_offset + 1];
            uint8_t r = src_line_ptr[src_offset + 2];

            // Składamy kolor i zapisujemy wprost do pamięci
            // dest_line_ptr[screen_x] to to samo co backbuffer[screen_y*fb.width + screen_x]
            dest_line_ptr[screen_x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

void fill_screen(uint32_t color) {
    if (!backbuffer) return;
    uint32_t total_pixels = fb.width * fb.height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        backbuffer[i] = color;
    }
}


void graphics_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height) return;
    
    // Pobierz tło z backbuffera
    uint32_t bg = backbuffer[y * fb.width + x];
    
    uint8_t r_bg = (bg >> 16) & 0xFF;
    uint8_t g_bg = (bg >> 8) & 0xFF;
    uint8_t b_bg = bg & 0xFF;
    
    uint8_t r_fg = (color >> 16) & 0xFF;
    uint8_t g_fg = (color >> 8) & 0xFF;
    uint8_t b_fg = color & 0xFF;
    
    // Szybka matematyka stałoprzecinkowa (dzielenie przez 255 -> >> 8)
    uint8_t r = (r_fg * alpha + r_bg * (255 - alpha)) >> 8;
    uint8_t g = (g_fg * alpha + g_bg * (255 - alpha)) >> 8;
    uint8_t b = (b_fg * alpha + b_bg * (255 - alpha)) >> 8;
    
    backbuffer[y * fb.width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
}

extern "C" void graphics_draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            graphics_put_pixel_alpha(x + i, y + j, color, alpha);
        }
    }
}


