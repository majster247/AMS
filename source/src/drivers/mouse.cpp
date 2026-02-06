#include "graphics.h"
#include "io.h"
#include "mouse.h"
#include "kernel.h"

// Zmienne globalne
int32_t mouse_x = 640;
int32_t mouse_y = 360;
int32_t old_mouse_x = 640;
int32_t old_mouse_y = 360;
int32_t drawn_x = 640, drawn_y = 360;

uint8_t mouse_cycle = 0;
uint8_t mouse_byte[3];

// Bufor na tło pod kursorem
uint32_t mouse_back_buffer[16 * 16];

volatile bool mouse_left_pressed = false;
volatile bool mouse_right_pressed = false;
volatile bool mouse_moved = false;

// Pomocnicza funkcja do pobierania piksela z backbuffera
uint32_t get_pixel(int x, int y) {
    if (x < 0 || y < 0 || x >= 1280 || y >= 720) return 0;
    // Odwołujemy się bezpośrednio do backbuffera z graphics.cpp
    extern uint32_t* backbuffer; 
    if (!backbuffer) return 0;
    return backbuffer[y * 1280 + x];
}

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    uint8_t status;

    // 1. Włącz port myszy (Auxiliary Device)
    mouse_wait(1);
    outb(0x64, 0xA8);

    // 2. Włącz przerwania IRQ12 (Bit 1 w Compaq Status Byte)
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2); // Force Enable IRQ12
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    // 3. Ustawienia domyślne (Standard PS/2)
    mouse_write(0xF6);
    mouse_read();  // ACK

    // 4. Włącz strumieniowanie danych
    mouse_write(0xF4);
    mouse_read();  // ACK

    // Inicjalizacja zmiennych
    mouse_cycle = 0;
    mouse_x = 640; // Środek ekranu
    mouse_y = 360;
    old_mouse_x = 640;
    old_mouse_y = 360;
    
    // Wyczyść bufor tła na start
    for(int i=0; i<256; i++) mouse_back_buffer[i] = 0x1D1D1D;
}

extern "C" void mouse_handler(struct regs *r) {
    uint8_t status = inb(0x64);
    
    // Jeśli nie ma danych w buforze wyjściowym, to pewnie spurious IRQ.
    // Ale dla pewności wyślij EOI.
    if (!(status & 0x01)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }
    
    // CZYTAJ DANE (To czyści rejestr kontrolera)
    uint8_t data = inb(0x60);

    // Jeśli dane nie są od myszy (Bit 5 w statusie = 0), to klawiatura lub inne ustrojstwo
    if (!(status & 0x20)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    // --- LOGIKA MYSZY ---
    
    switch (mouse_cycle) {
        case 0:
            // Bajt 1: Musi mieć bit 3 (0x08) ustawiony.
            // ORAZ nie może mieć przepełnienia (0xC0).
            // Jeśli to nie jest nagłówek -> czekamy dalej na właściwy bajt.
            if ((data & 0x08) == 0) {
                // To nie jest pierwszy bajt pakietu! Ignorujemy i szukamy dalej.
                // Nie resetujemy mouse_cycle, bo i tak jest 0.
                break; 
            }
            mouse_byte[0] = data;
            mouse_cycle++;
            break;
            
        case 1:
            // Bajt 2: Delta X
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
            
        case 2:
            // Bajt 3: Delta Y
            mouse_byte[2] = data;
            mouse_cycle = 0; // Reset cyklu, bo mamy komplet danych

            uint8_t flags = mouse_byte[0];

            bool is_left_pressed = flags & 0x01;
            bool is_right_pressed = flags & 0x02;

            if(is_left_pressed != mouse_left_pressed) {
                mouse_left_pressed = is_left_pressed;
                if(mouse_left_pressed) {
                    // Lewy przycisk wciśnięty
                    //outb(0x3F8, 'L'); // Debug: Wyślij 'L' na serial przy naciśnięciu
                } else {
                    // Lewy przycisk puszczony
                    //outb(0x3F8, 'l'); // Debug: Wyślij 'l' na serial przy puszczeniu
                }
            }

            if(is_right_pressed != mouse_right_pressed) {
                mouse_right_pressed = is_right_pressed;
                if(mouse_right_pressed) {
                    // Prawy przycisk wciśnięty
                    //outb(0x3F8, 'R'); // Debug: Wyślij 'R' na serial przy naciśnięciu
                } else {
                    // Prawy przycisk puszczony
                    //outb(0x3F8, 'r'); // Debug: Wyślij 'r' na serial przy puszczeniu
                }
            }
            
            // Mamy komplet! Przetwarzamy.
            // WAŻNE: Używamy zmiennych lokalnych, żeby nie śmiecić w globalach w razie błędu
            {
                int8_t dx = (int8_t)mouse_byte[1];
                int8_t dy = (int8_t)mouse_byte[2];

                // Zapobiegamy "teleportacji" przy błędnym odczycie
                // Jeśli ruch jest nienaturalnie duży (np. > 50px na jedno przerwanie), ignoruj
                if (dx > 50 || dx < -50 || dy > 50 || dy < -50) {
                     mouse_cycle = 0;
                     break; 
                }

                mouse_x -= dx;
                mouse_y += dy; // Y odwrócone

                // Clamp (Ściany ekranu)
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_x >= 1279) mouse_x = 1279;
                if (mouse_y >= 719) mouse_y = 719;

                mouse_moved = true;
                
                // Debug na serialu - kropka przy udanym ruchu
                //outb(0x3F8, '.'); 
            }
            
            // Reset cyklu - czekamy na nowy pakiet
            mouse_cycle = 0;
            break;
    }

    // === EOI (ZAWSZE!) ===
    outb(0xA0, 0x20); // Slave
    outb(0x20, 0x20); // Master
}

// === RYSOWANIE ===

void save_background(int x, int y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            // Zabezpieczenie krawędzi ekranu
            if (x + j >= 1280 || y + i >= 720) {
                mouse_back_buffer[i * 16 + j] = 0x000000; // Poza ekranem = czarny
            } else {
                mouse_back_buffer[i * 16 + j] = get_pixel(x + j, y + i);
            }
        }
    }
}

void restore_background(int x, int y) {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            // Rysuj tylko, jeśli jesteśmy wewnątrz ekranu
            if (x + j < 1280 && y + i < 720) {
                graphics_put_pixel(x + j, y + i, mouse_back_buffer[i * 16 + j]);
            }
        }
    }
}

void draw_cursor_shape(int x, int y) {
    const char* arrow[] = {
        "X       ", "XX      ", "X.X     ", "X..X    ",
        "X...X   ", "X....X  ", "X.....X ", "X..XXXXX",
        "X.X     ", "XX      ", "X       ", "        "
    };

    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 8; j++) {
            // Zabezpieczenie przed rysowaniem poza ekranem
            if (x + j >= 1280 || y + i >= 720) continue;

            if (arrow[i][j] == 'X') graphics_put_pixel(x + j, y + i, 0xFFFFFF); // Biały
            else if (arrow[i][j] == '.') graphics_put_pixel(x + j, y + i, 0x000000); // Czarny
        }
    }
}

// Rozdzielamy logikę na dwie funkcje dla Kernela
void mouse_erase() {
    restore_background(drawn_x, drawn_y);
}

void mouse_draw() {
    // 1. Zapisz "czyste" tło pod NOWĄ pozycją
    save_background(mouse_x, mouse_y);
    // 2. Narysuj kursor
    draw_cursor_shape(mouse_x, mouse_y);
    // 3. Zaktualizuj pozycję "narysowaną"
    drawn_x = mouse_x;
    drawn_y = mouse_y;
}

void update_mouse_on_screen() {
    // 1. Przywróć tło w STARYM miejscu
    restore_background(old_mouse_x, old_mouse_y);
    
    // 2. Zapisz tło w NOWYM miejscu
    save_background(mouse_x, mouse_y);
    
    // 3. Narysuj kursor
    draw_cursor_shape(mouse_x, mouse_y);
    
    // 4. Zaktualizuj pozycję
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
    
    mouse_moved = false;
}