/**
 * @file graphics.h
 * @author Majster
 * @brief Silnik graficzny 2D, obsługa prymitywów i double-bufferingu.
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

/** @brief Struktura opisująca parametry fizycznego bufora ekranu (LFB) */
struct Framebuffer {
    uint64_t address; /**< Adres fizyczny w pamięci wideo */
    uint32_t width;   /**< Szerokość w pikselach */
    uint32_t height;  /**< Wysokość w pikselach */
    uint32_t pitch;   /**< Liczba bajtów na jedną linię poziomą */
    uint8_t  bpp;     /**< Bits Per Pixel (zazwyczaj 32) */
};

extern Framebuffer fb;
extern uint32_t fb_width;
extern uint32_t fb_height;
/** @brief Wskaźnik do drugiego bufora w RAM (Double Buffering) */
extern uint32_t* backbuffer;

extern "C" {
    /** @brief Rysuje pojedynczy piksel na backbufferze */
    void graphics_put_pixel(int x, int y, uint32_t color);
    /** @brief Rysuje piksel z uwzględnieniem kanału Alpha (przezroczystość) */
    void graphics_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);
    /** @brief Rysuje prostokąt z wypełnieniem przezroczystym kolorem */
    void graphics_draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
    /** @brief Wypełnia cały ekran jednym kolorem */
    void graphics_clear_screen(uint32_t color);
    /** @brief Rysuje pojedynczy znak czcionki bitmapowej */
    void graphics_draw_char(int x, int y, unsigned char c, uint32_t color);
    /** @brief Wyświetla ciąg znaków na ekranie */
    void graphics_print(int x, int y, const char* str, uint32_t color);
    /** @brief Rysuje prostokąt z wypełnieniem */
    void graphics_draw_rect(int x, int y, int w, int h, uint32_t color);
    
    /** @brief Rysuje wbudowaną bitmapę tapety (fullscreen) */
    void graphics_draw_bmp();
    /** @brief Rysuje tapetę wycentrowaną na ekranie */
    void graphics_draw_bmp_centered();
    /** @brief Rysuje fragment tapety pod wskazanymi współrzędnymi */
    void graphics_draw_bmp_part(int x_start, int y_start, int w, int h);
    
    /** @brief Alokuje pamięć na backbuffer i przygotowuje system do double-bufferingu */
    void graphics_init_double_buffer();
    /** @brief Kopiuje zawartość backbuffera do fizycznej pamięci VRAM (odświeżenie ekranu) */
    void graphics_flip();

    /** @brief Blokuje dostęp do grafiki (Mutex - zapobiega rwaniu obrazu) */
    void graphics_acquire();
    /** @brief Zwalnia blokadę grafiki */
    void graphics_release();

    /** @brief Czyści obszar okna (przywraca tło tapety w danym miejscu) */
    void clear_window_area(int x_start, int y_start, int w, int h);
    /** @brief Zaawansowane rysowanie fragmentu z przesunięciem */
    void graphics_draw_bmp_part_offset(int x_start, int y_start, int w, int h, int off_x, int off_y);
}

/** @brief Rysuje dolny pasek zadań AMS-OS */
void draw_status_bar();
/** @brief Odświeża zegar systemowy na pasku zadań */
void update_clock_display();
/** @brief Kopiuje blok pikseli z ekranu do bufora */
void graphics_get_block(int x, int y, int w, int h, uint32_t* buffer);
/** @brief Wkleja blok pikseli z bufora na ekran */
void graphics_put_block(int x, int y, int w, int h, uint32_t* buffer);
/** @brief Wypełnia fizyczny ekran (bez backbuffera) kolorem */
void fill_screen(uint32_t color);

#endif