#include "graphics.h"
#include "io.h"
#include "mouse.h"
#include "kernel.h"


int32_t mouse_x = 640;
int32_t mouse_y = 360;
int32_t old_mouse_x = 640;
int32_t old_mouse_y = 360;
uint8_t mouse_cycle = 0;
uint8_t mouse_byte[3];
uint32_t mouse_back_buffer[16 * 16];

int32_t old_x = 640;
int32_t old_y = 360;

bool mouse_left_pressed = false;
bool mouse_right_pressed = false;



void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}


uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}


void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

void mouse_init() {
    mouse_wait(1);
    outb(0x64, 0xA8); // Włącz mysz

    mouse_wait(1);
    outb(0x64, 0x20); // Pobierz Command Byte
    mouse_wait(0);
    uint8_t status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60); // Zapisz Command Byte
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6); // Defaulty
    inb(0x60); // ACK
    mouse_write(0xF4); // Enable streaming
    inb(0x60); // ACK

    mouse_write(0xF4); // Enable streaming
    uint8_t ack = mouse_read();
    if (ack != 0xFA) {
        write_serial_string("[MOUSE] Blad inicjalizacji myszy! ACK: ");
        write_serial_hex(ack);
        write_serial_string("\n");
    } else {
        write_serial_string("[MOUSE] Myszka zainicjalizowana pomyslnie!\n");
    }

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
}


void mouse_draw_cursor(int32_t x, int32_t y, bool restore) {
    if (restore) {
        // Przywracamy stare tło z bufora
    }
    
    // Zapisujemy nowe tło przed narysowaniem kursora
    
    // Rysujemy kursor (np. prosty biały trójkąt/kwadrat)
    graphics_draw_rect(x, y, 10, 10, 0xFFFFFF); 
}


void update_mouse_on_screen() {
    // Rysujemy ładną strzałkę zamiast kropki
    draw_cursor_shape(mouse_x, mouse_y);
}

void draw_cursor_shape(int x, int y) {
    // Prosta strzałka 8x12
    const char* arrow[] = {
        "X       ",
        "XX      ",
        "X.X     ",
        "X..X    ",
        "X...X   ",
        "X....X  ",
        "X.....X ",
        "X..XXXXX",
        "X.X     ",
        "XX      ",
        "X       ",
        "        "
    };

    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 8; j++) {
            if (arrow[i][j] == 'X') graphics_put_pixel(x + j, y + i, 0xFFFFFF); // Biała krawędź
            else if (arrow[i][j] == '.') graphics_put_pixel(x + j, y + i, 0x000000); // Czarne wypełnienie
        }
    }
}


