#include "kernel.h"
#include "io.h"
#include "task.h"
#include "gui.h"


volatile bool key_ctrl_pressed = false;
volatile bool key_shift_pressed = false;
volatile bool key_alt_pressed = false;

static char keyboard_queue[256];
static int k_head = 0;
static int k_tail = 0;

extern Desktop* main_desktop;

static char my_scancode_table[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void keyboard_init() {
    k_head = 0;
    k_tail = 0;
}

extern "C" void keyboard_handler(registers* r) {
    (void)r;
    uint8_t scancode = inb(0x60);
    
    if (scancode < 0x80) {
        if (scancode == 0x38) key_alt_pressed = true;
        if (scancode == 0x1D) key_ctrl_pressed = true;
        if (scancode == 0x2A) key_shift_pressed = true;

        char c = my_scancode_table[scancode];
        
        // Obsługa Shift (wielkie litery) - PROSTA WERSJA
        if (key_shift_pressed && c >= 'a' && c <= 'z') {
            c -= 32;
        }

        if (c != 0) {
            // 1. Zapisz do bufora (dla syscalli/konsoli tekstowej)
            keyboard_queue[k_head] = c;
            k_head = (k_head + 1) % 256;
            
            // 2. Debug na serial
            write_serial_char(c);

            // 3. --- TO JEST TO CZEGO BRAKOWAŁO! ---
            // Przekaż znak bezpośrednio do systemu okienkowego
            if (main_desktop) {
                main_desktop->HandleKeyboard(c);
            }
            // --------------------------------------
        }
    } else{
        uint8_t release_code = scancode - 0x80;
        if (release_code == 0x38) key_alt_pressed = false;
        if (release_code == 0x1D) key_ctrl_pressed = false;
        if (release_code == 0x2A) key_shift_pressed = false;
    }
}

extern "C" uint8_t keyboard_get_char() {
    if (k_head == k_tail) return 0;
    uint8_t c = (uint8_t)keyboard_queue[k_tail];
    k_tail = (k_tail + 1) % 256;
    return c;
}

extern "C" uint64_t sys_get_key() {
    return (uint64_t)keyboard_get_char();
}