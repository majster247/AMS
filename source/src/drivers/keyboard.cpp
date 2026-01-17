#include "io.h"
#include "kernel.h"

// Bardzo prosta mapa scancode -> ASCII
const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

extern "C" void keyboard_handler() {
    uint8_t scancode = inb(0x60);
    if (!(scancode & 0x80)) { // Wciśnięcie
        if (scancode == 0x0E) { // Scancode dla Backspace
            terminal_putchar('\b');
        } else if (scancode < 58) {
            char c = scancode_to_ascii[scancode];
            if (c > 0) terminal_putchar(c);
        }
    }
    outb(0x20, 0x20);
}