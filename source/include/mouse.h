/**
 * @file mouse.h
 * @author Majster
 * @brief Sterownik myszy PS/2 z obsługą kursora i double-bufferingu.
 */

#pragma once
#include <stdint.h>

/** @brief Aktualna pozycja X kursora na ekranie */
extern int32_t mouse_x;
/** @brief Aktualna pozycja Y kursora na ekranie */
extern int32_t mouse_y;
/** @brief Poprzednia pozycja X (używana do czyszczenia śladu) */
extern int32_t old_mouse_x;
/** @brief Poprzednia pozycja Y (używana do czyszczenia śladu) */
extern int32_t old_mouse_y;

/** @brief Aktualny krok w cyklu pakietu PS/2 (0-2) */
extern uint8_t mouse_cycle;
/** @brief Bufor na 3-bajtowy pakiet danych z myszy */
extern uint8_t mouse_byte[3];
/** @brief Bufor przechowujący piksele tła pod kursorem */
extern uint32_t mouse_back_buffer[16 * 16];

extern int32_t old_x;
extern int32_t old_y;

/** @brief Flaga naciśnięcia lewego przycisku myszy */
extern volatile bool mouse_left_pressed;
/** @brief Flaga naciśnięcia prawego przycisku myszy */
extern volatile bool mouse_right_pressed;
/** @brief Flaga informująca kernel o zmianie pozycji (wymusza odświeżenie GUI) */
extern volatile bool mouse_moved;

/** @brief Inicjalizuje kontroler PS/2 i włącza raportowanie danych myszy */
void mouse_init();
/** @brief Aktualizuje logikę pozycji myszy na ekranie */
void update_mouse_on_screen();
/** @brief Główny handler przerwania IRQ12 */
extern "C" void mouse_handler(struct regs *r);
/** @brief Wysyła bajt sterujący bezpośrednio do myszy */
extern "C" void mouse_write(uint8_t data);
/** @brief Odczytuje bajt danych z portu myszy */
extern "C" uint8_t mouse_read();

/** @brief Kopiuje tło z ekranu do mouse_back_buffer przed narysowaniem kursora */
extern "C" void save_background(int x, int y);
/** @brief Przywraca tło z mouse_back_buffer na ekran (mazanie kursora) */
extern "C" void restore_background(int x, int y);
/** @brief Rysuje kształt kursora (bitmapę) na ekranie */
extern "C" void draw_cursor_shape(int x, int y);

/** @brief Wysokopoziomowa funkcja usuwająca kursor z ekranu */
void mouse_erase();
/** @brief Wysokopoziomowa funkcja rysująca kursor (save -> draw) */
void mouse_draw();