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

    static char scancode_to_ascii_shift[] = {
        0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
    };

    static bool shift_pressed = false;

    volatile char cmd_buffer[128];
    volatile int cmd_index = 0;
    volatile bool line_ready = false;

extern "C" void keyboard_handler() {
        uint8_t scancode = inb(0x60);

        if (scancode == 0x2A || scancode == 0x36) { // Left/Right Shift pressed
            shift_pressed = true;
            return;
        }
        if (scancode == 0xAA || scancode == 0xB6) { // Left/Right Shift released
            shift_pressed = false;
            return;
        }
        if (scancode < 0x80) { // Key pressed
            char c = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
            if (c) {
                if (c == '\n') {
                    line_ready = true;
                } else if (c == '\b') {
                    if (cmd_index > 0) {
                        cmd_index--;
                        cmd_buffer[cmd_index] = '\0';
                        terminal_putchar('\b');
                    }
                } else if (c != 0 && cmd_index < 127) {
                    cmd_buffer[cmd_index++] = c;
                    cmd_buffer[cmd_index] = '\0';
                    terminal_putchar(c);
                }
            }
        }
        outb(0x20, 0x20); 
    }

    void keyboard_init() {
        // Odmaskuj IRQ1 w PIC1
        uint8_t mask = inb(0x21);
        outb(0x21, mask & ~0x02); 
        write_serial_string("[KEY] IRQ1 Unmasked\n");
        idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);
        write_serial_string("[KEY] Keyboard ISR registered at vector 33\n");
    }

    uint8_t keyboard_get_char() {
        while (!line_ready) {
            asm volatile("hlt"); 
        }
        char c = cmd_buffer[0];
        // Przesuń bufor w lewo
        for (int i = 0; i < cmd_index - 1; i++) {
            cmd_buffer[i] = cmd_buffer[i + 1];
        }
        cmd_index--;
        cmd_buffer[cmd_index] = '\0';
        if (cmd_index == 0) {
            line_ready = false;
        }
        return c;
    }

  
    uint8_t keyboard_get_char_nonblocking() {
        if (cmd_index == 0 && !line_ready) return 0; // Bufor pusty
       
        char last_char = cmd_buffer[0];
        // Przesuń bufor w lewo
        for (int i = 0; i < cmd_index - 1; i++) {
            cmd_buffer[i] = cmd_buffer[i + 1];
        }
        cmd_index--;
        cmd_buffer[cmd_index] = '\0';
        if (cmd_index == 0) {
            line_ready = false;
        }

        return last_char; 
    }