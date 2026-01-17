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