#include "kernel.h"
#include "io.h"
#include "task.h"
#include "gui.h"
#include "evdev.h"

extern "C" void evdev_push_scancode(uint8_t scancode);


volatile bool key_ctrl_pressed = false;
volatile bool key_shift_pressed = false;
volatile bool key_alt_pressed = false;

static char keyboard_queue[256];
static int k_head = 0;
static int k_tail = 0;
static int16_t keyboard_event_queue[256];
static int ev_head = 0;
static int ev_tail = 0;

extern Desktop* main_desktop;
extern task* current_task;
extern task* kernel_task;

static char my_scancode_table[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void keyboard_init() {
    k_head = 0;
    k_tail = 0;
    ev_head = 0;
    ev_tail = 0;
}

static void enqueue_key_event(int16_t ev) {
    int next = (ev_head + 1) % 256;
    if (next == ev_tail) return; // drop when full
    keyboard_event_queue[ev_head] = ev;
    ev_head = next;
}

extern "C" void keyboard_handler(registers* r) {
    (void)r;
    uint8_t scancode = inb(0x60);

    /* Forward raw scancode to evdev layer for /dev/input/event0 */
    evdev_push_scancode(scancode);
    
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
            enqueue_key_event((int16_t)c); // key down
            
            // 2. Debug na serial
            write_serial_char(c);

            // 3. Przekazuj klawiaturę do GUI tylko gdy działa kernel desktop.
            if (main_desktop && current_task == kernel_task) {
                main_desktop->HandleKeyboard(c);
            }
        }
    } else{
        uint8_t release_code = scancode - 0x80;
        if (release_code == 0x38) key_alt_pressed = false;
        if (release_code == 0x1D) key_ctrl_pressed = false;
        if (release_code == 0x2A) key_shift_pressed = false;

        char c = my_scancode_table[release_code];
        if (key_shift_pressed && c >= 'a' && c <= 'z') c -= 32;
        if (c != 0) enqueue_key_event((int16_t)(-((int)c))); // key up
    }
}

extern "C" uint8_t keyboard_get_char() {
    if (k_head == k_tail) return 0;
    uint8_t c = (uint8_t)keyboard_queue[k_tail];
    k_tail = (k_tail + 1) % 256;
    return c;
}

extern "C" uint64_t sys_get_key() {
    if (ev_head == ev_tail) return 0;
    int16_t ev = keyboard_event_queue[ev_tail];
    ev_tail = (ev_tail + 1) % 256;
    return (uint64_t)(int64_t)ev;
}