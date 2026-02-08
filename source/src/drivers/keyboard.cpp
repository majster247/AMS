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

    bool shift_pressed = false;
    bool key_alt_pressed = false;

    volatile uint8_t cmd_buffer[128];
    volatile int cmd_index = 0;
    volatile bool line_ready = false;
    volatile uint8_t kb_buffer[256];
    volatile uint32_t kb_read_ptr = 0;
    volatile uint32_t kb_write_ptr = 0;
    volatile uint32_t KB_BUFFER_SIZE = 256;

extern "C" void keyboard_handler(struct regs *r) {
        uint8_t scancode = inb(0x60);

        // Obsługa Shift
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
            return;
        } else if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = false;
            return;
        }
        else if (scancode == 0x38) { // Alt
            key_alt_pressed = true;
            return;
        }
        else if (scancode == 0xB8) { // Alt zwolniony
            key_alt_pressed = false;
            return;
        }

        // Jeśli klawisz został zwolniony, ignorujemy go
        if (scancode & 0x80) {
            return;
        }

        char ascii_char = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
        
        if (ascii_char) {
            kb_buffer[kb_write_ptr] = ascii_char;
            kb_write_ptr = (kb_write_ptr + 1) % KB_BUFFER_SIZE;

            // Echo do terminala
            terminal_putchar(ascii_char);
        }    
    }

    void keyboard_init() {
        // Odmaskuj IRQ1 w PIC1
        uint8_t mask = inb(0x21);
        outb(0x21, mask & ~0x02); 
        //write_serial_string("[KEY] IRQ1 Unmasked\n");
        //idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);
        //write_serial_string("[KEY] Keyboard ISR registered at vector 33\n");
    }

    uint8_t keyboard_get_char() {
        if (kb_read_ptr == kb_write_ptr) {
            return 0; // Pusto
        }

        char c = kb_buffer[kb_read_ptr];
        kb_read_ptr = (kb_read_ptr + 1) % KB_BUFFER_SIZE;
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