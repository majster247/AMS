#include "io.h"
#include "kernel.h"
#include <stdint.h>

// Zapowiedzi symboli zewnętrznych
extern "C" void isr_keyboard_stub(); 
extern "C" void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

// Tablica mapowania scancode -> ASCII
static char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

char cmd_buffer[128];
int cmd_index = 0;
bool line_ready = false;

extern "C" void keyboard_handler() {
    uint8_t scancode = inb(0x60);
    
    // Zmieniamy kolor tła drugiego znaku na ekranie przy każdym IRQ
    *((uint8_t*)0xB8003) = *((uint8_t*)0xB8003) + 1;

    if (scancode < 0x80) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            // WYŚLIJ NA SERIAL - to zawsze działa, niezależnie od VGA
            outb(0x3F8, c); 

            // Jeśli masz terminal_putchar, użyj go:
            terminal_putchar(c); 

            if (c == '\n') {
                cmd_buffer[cmd_index] = '\0';
                line_ready = true;
            } else if (cmd_index < 127) {
                cmd_buffer[cmd_index++] = c;
            }
        }
    }
    outb(0x20, 0x20);
}

extern "C" void keyboard_init() {
    // Rejestrujemy wektor 33 (IRQ1 + offset 32)
    outb(0x21, inb(0x21) & ~(1 << 1)); // Wyzerowanie bitu 1 odmaskowuje IRQ1
    idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);
}