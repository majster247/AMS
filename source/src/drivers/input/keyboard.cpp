#include "kernel.h"
#include "io.h"
#include "task.h"


volatile bool key_ctrl_pressed = false;
volatile bool key_shift_pressed = false;
volatile bool key_alt_pressed = false;

// Deklarujemy jeden, spójny bufor i wskaźniki
static char keyboard_queue[256];
static int k_head = 0;
static int k_tail = 0;

// Tablica scancode (uproszczona - dopasuj do swojej)
static char my_scancode_table[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void keyboard_init() {
    k_head = 0;
    k_tail = 0;
    // Tutaj ewentualna konfiguracja kontrolera PS/2
}

extern "C" void keyboard_handler(registers* r) {
    (void)r;
    uint8_t scancode = inb(0x60);
    
    // Obsługa wciśnięcia (Press)
    if (scancode < 0x80) {
        if (scancode == 0x38) key_alt_pressed = true;   // Alt Press
        if (scancode == 0x1D) key_ctrl_pressed = true;  // Ctrl Press
        if (scancode == 0x2A) key_shift_pressed = true; // Shift Press

        char c = my_scancode_table[scancode];
        if (c != 0) {
            keyboard_queue[k_head] = c;
            k_head = (k_head + 1) % 256;
            write_serial_char(c);
        }
    } 
    // Obsługa puszczenia (Release) - scancode + 0x80
    else {
        uint8_t release_code = scancode - 0x80;
        if (release_code == 0x38) key_alt_pressed = false;   // Alt Release
        if (release_code == 0x1D) key_ctrl_pressed = false;  // Ctrl Release
        if (release_code == 0x2A) key_shift_pressed = false; // Shift Release
    }
}

// Implementacja zgodna z Twoją deklaracją w kernel.h (uint8_t)
extern "C" uint8_t keyboard_get_char() {
    if (k_head == k_tail) return 0;
    
    uint8_t c = (uint8_t)keyboard_queue[k_tail];
    k_tail = (k_tail + 1) % 256;
    return c;
}

// Syscall użyje tej samej logiki
extern "C" uint64_t sys_get_key() {
    return (uint64_t)keyboard_get_char();
}