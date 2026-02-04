#include "io.h"
#include <stdint.h>

#define PORT 0x3f8          // COM1

extern "C" int init_serial() {
   outb(PORT + 1, 0x00);    // Wyłącz przerwania
   outb(PORT + 3, 0x80);    // Włącz DLAB (ustawienie prędkości)
   outb(PORT + 0, 0x03);    // Dzielnik 3 (38400 baud) - low byte
   outb(PORT + 1, 0x00);    // Dzielnik 3 - high byte
   outb(PORT + 3, 0x03);    // 8 bitów, brak parzystości, 1 bit stopu
   outb(PORT + 2, 0xC7);    // Włącz FIFO, wyczyść, próg 14 bajtów
   outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
   return 0;
}

// Sprawdź, czy bufor nadawania jest pusty
int is_transmit_empty() {
   return inb(PORT + 5) & 0x20;
}

extern "C" void write_serial(char a) {
   while (is_transmit_empty() == 0);
   outb(PORT, a);
}

extern "C" void write_serial_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        write_serial(str[i]);
    }
}

extern "C" void write_serial_hex(uint64_t val) {
    write_serial_string("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0xF;
        if (nibble < 10) write_serial('0' + nibble);
        else write_serial('A' + (nibble - 10));
    }
}

extern "C" void write_serial_dec(uint64_t val) {
    if (val == 0) {
        write_serial('0');
        return;
    }
    char buffer[20];
    int index = 0;
    while (val > 0) {
        buffer[index++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = index - 1; i >= 0; i--) {
        write_serial(buffer[i]);
    }
}

extern "C"{

void write_serial_char(char c) {
    write_serial(c);
}

bool serial_received() {
    return inb(0x3F8 + 5) & 1; // Sprawdź bit 0 w Line Status Register
}

char serial_read() {
    while (!serial_received()); // Czekaj na bajt
    return inb(0x3F8);
}

}