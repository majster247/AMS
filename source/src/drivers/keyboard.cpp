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

volatile char cmd_buffer[128];
volatile int cmd_index = 0;
volatile bool line_ready = false;

extern "C" void keyboard_handler() {
    uint8_t scancode = inb(0x60);
    
    // Debug wizualny (niebieski kwadracik) - zostawiamy
    *((uint8_t*)0xB8001) = *((uint8_t*)0xB8001) + 1;

    if (scancode < 0x80) {
        char c = scancode_to_ascii[scancode];
        
        if (c == '\n') {
            line_ready = true;
            terminal_putchar('\n');
            // Opcjonalnie: debug na serial
            write_serial_string("\n[KEY] Enter pressed\n");
        } 
        else if (c == '\b') {
            if (cmd_index > 0) {
                // Usuwamy ze zmiennych volatile
                int idx = cmd_index - 1;
                cmd_buffer[idx] = '\0';
                cmd_index = idx;
                terminal_putchar('\b');
            }
        } 
        else if (c != 0) { // Zabezpieczenie przed pustymi znakami
            if (cmd_index < 127) {
                int idx = cmd_index;
                cmd_buffer[idx] = c;
                cmd_buffer[idx + 1] = '\0';
                cmd_index = idx + 1;
                write_serial_string(&c); // Opcjonalnie: debug na serial
                terminal_putchar(c); // ECHO na ekran
            }
        }
    }
    
    outb(0x20, 0x20); // EOI
}

extern "C" void keyboard_init() {
    // Rejestrujemy wektor 33 (IRQ1 + offset 32)
    outb(0x21, inb(0x21) & ~(1 << 1)); // Wyzerowanie bitu 1 odmaskowuje IRQ1
    idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);
}