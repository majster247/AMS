#include "mouse.h"
#include "io.h"
#include "graphics.h"
#include "kernel.h"
#include "evdev.h"


// === ZMIENNE GLOBALNE ===
volatile bool mouse_left_pressed = false;
volatile bool mouse_right_pressed = false;
volatile bool mouse_moved = false;

// Pozycja kursora
int32_t mouse_x = 640;
int32_t mouse_y = 360;
int32_t old_mouse_x = 640;
int32_t old_mouse_y = 360;

// Logika pakietów
uint8_t mouse_cycle = 0;
uint8_t mouse_byte[3];

// Ignorowanie śmieci na starcie
static int discard_packets = 10; 

// === FIX DPI: SENSITIVITY & ACCUMULATOR ===
// MOUSE_SCALE: Dzielnik ruchu. 
// 1 = Raw input (bardzo szybko dla nowoczesnych myszek)
// 2 = Pół prędkości (zalecane dla QEMU)
// 3 lub 4 = Wolniej, większa precyzja
#define MOUSE_SCALE 1

// Akumulatory resztek (żeby nie gubić precyzji przy dzieleniu)
static int32_t mouse_acc_x = 0;
static int32_t mouse_acc_y = 0;
static uint64_t mouse_event_queue[256];
static int mouse_ev_head = 0;
static int mouse_ev_tail = 0;

// Bufor tła
uint32_t mouse_back_buffer[16 * 16];

static void enqueue_mouse_event(uint8_t buttons, uint8_t flags) {
    int next = (mouse_ev_head + 1) % 256;
    if (next == mouse_ev_tail) return;

    uint64_t x = (uint64_t)((uint16_t)(mouse_x & 0xFFFF));
    uint64_t y = (uint64_t)((uint16_t)(mouse_y & 0xFFFF));
    uint64_t ev = x | (y << 16) | ((uint64_t)buttons << 32) | ((uint64_t)flags << 40);
    mouse_event_queue[mouse_ev_head] = ev;
    mouse_ev_head = next;
}

// Helpery
void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    uint8_t status;

    // 1. Inicjalizacja kontrolera
    mouse_wait(1);
    outb(0x64, 0xA8); 

    mouse_wait(1);
    outb(0x64, 0x20); 
    mouse_wait(0);
    status = inb(0x60) | 2; 
    mouse_wait(1);
    outb(0x64, 0x60); 
    mouse_wait(1);
    outb(0x60, status);

    // 2. Reset myszy
    mouse_write(0xF6); 
    mouse_read();      

    mouse_write(0xF4); 
    mouse_read();      

    // 3. Reset zmiennych
    mouse_x = 640; 
    mouse_y = 360;
    mouse_cycle = 0;
    discard_packets = 10;
    mouse_acc_x = 0;
    mouse_acc_y = 0;
    mouse_ev_head = 0;
    mouse_ev_tail = 0;

    write_serial_string("[MOUSE] Zinicjalizowana.\n");
}

extern "C" void mouse_handler(struct regs *r) {
    (void)r;
    uint8_t status = inb(0x64);
    
    if (!(status & 0x01) || !(status & 0x20)) return; 

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            if ((data & 0x08) == 0) { mouse_cycle = 0; return; }
            if (data & 0xC0) { mouse_cycle = 0; return; } // Overflow check
            
            mouse_byte[0] = data;
            mouse_cycle++;
            break;
            
        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
            
        case 2:
            mouse_byte[2] = data;
            mouse_cycle = 0;

            if (discard_packets > 0) {
                discard_packets--;
                return;
            }

            // === RAW INPUT ===
            int8_t raw_dx = (int8_t)mouse_byte[1];
            int8_t raw_dy = (int8_t)mouse_byte[2];

            // === INTELLIGENT SCALING ===
            // Dodajemy nowy ruch do akumulatora
            mouse_acc_x += raw_dx;
            mouse_acc_y += raw_dy;

            // Obliczamy ile pełnych pikseli mamy do przesunięcia (dzielenie całkowite)
            int32_t move_x = mouse_acc_x / MOUSE_SCALE;
            int32_t move_y = mouse_acc_y / MOUSE_SCALE;

            // Zostawiamy resztę w akumulatorze na następny cykl (modulo)
            mouse_acc_x %= MOUSE_SCALE;
            mouse_acc_y %= MOUSE_SCALE;

            // === APLIKOWANIE RUCHU ===
            // Użytkownik chce osie odwrotnie względem obecnego zachowania.
            mouse_x += move_x;
            mouse_y -= move_y;

            // Clamp
            extern uint32_t fb_width;
            extern uint32_t fb_height;
            int32_t max_w = (fb_width > 0) ? (int32_t)fb_width : 1280;
            int32_t max_h = (fb_height > 0) ? (int32_t)fb_height : 720;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= max_w) mouse_x = max_w - 1;
            if (mouse_y >= max_h) mouse_y = max_h - 1;

            bool left = (mouse_byte[0] & 0x01);
            bool right = (mouse_byte[0] & 0x02);
            
            // Reagujemy tylko jeśli nastąpił faktyczny ruch pikselowy lub kliknięcie
            if (left != mouse_left_pressed || right != mouse_right_pressed || move_x != 0 || move_y != 0) {
                /* Push to evdev for libinput */
                if (move_x != 0 || move_y != 0) {
                    evdev_push_mouse_abs(mouse_x, mouse_y);
                }
                if (left != mouse_left_pressed) {
                    evdev_push_mouse_button(BTN_LEFT, left ? 1 : 0);
                }
                if (right != mouse_right_pressed) {
                    evdev_push_mouse_button(BTN_RIGHT, right ? 1 : 0);
                }

                mouse_left_pressed = left;
                mouse_right_pressed = right;
                mouse_moved = true; 
                uint8_t buttons = 0;
                if (left) buttons |= 0x1;
                if (right) buttons |= 0x2;
                uint8_t flags = 0;
                if (move_x != 0 || move_y != 0) flags |= 0x1;
                if (left || right) flags |= 0x2;
                enqueue_mouse_event(buttons, flags);
            }
            break;
    }
}

extern "C" uint64_t sys_get_mouse_event() {
    if (mouse_ev_head == mouse_ev_tail) return 0;
    uint64_t ev = mouse_event_queue[mouse_ev_tail];
    mouse_ev_tail = (mouse_ev_tail + 1) % 256;
    return ev;
}

// === RYSOWANIE KURSORA ===
extern uint32_t* backbuffer;
extern uint32_t fb_width;

void save_background(int x, int y) {
    if (!backbuffer) return;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            int target_x = x + j;
            int target_y = y + i;
            if (target_x < 1280 && target_y < 720 && target_x >= 0 && target_y >= 0) {
                 mouse_back_buffer[i * 16 + j] = backbuffer[target_y * fb_width + target_x];
            } else {
                 mouse_back_buffer[i * 16 + j] = 0; 
            }
        }
    }
}

void restore_background(int x, int y) {
    if (!backbuffer) return;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
             int target_x = x + j;
             int target_y = y + i;
             if (target_x < 1280 && target_y < 720 && target_x >= 0 && target_y >= 0) {
                 backbuffer[target_y * fb_width + target_x] = mouse_back_buffer[i * 16 + j];
             }
        }
    }
}

void draw_cursor_shape(int x, int y) {
    static const int cursor_w = 12;
    static const int cursor_h = 19;
    static const int cursor_bitmap[19][12] = {
        {2,2,0,0,0,0,0,0,0,0,0,0}, {2,1,2,0,0,0,0,0,0,0,0,0}, {2,1,1,2,0,0,0,0,0,0,0,0},
        {2,1,1,1,2,0,0,0,0,0,0,0}, {2,1,1,1,1,2,0,0,0,0,0,0}, {2,1,1,1,1,1,2,0,0,0,0,0},
        {2,1,1,1,1,1,1,2,0,0,0,0}, {2,1,1,1,1,1,1,1,2,0,0,0}, {2,1,1,1,1,1,1,1,1,2,0,0},
        {2,1,1,1,1,1,2,2,2,2,2,0}, {2,1,1,2,1,1,2,0,0,0,0,0}, {2,1,2,0,2,1,1,2,0,0,0,0},
        {2,2,0,0,2,1,1,2,0,0,0,0}, {2,0,0,0,0,2,1,1,2,0,0,0}, {0,0,0,0,0,2,1,1,2,0,0,0},
        {0,0,0,0,0,0,2,2,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0}
    };

    if (!backbuffer) return;

    for(int i=0; i<cursor_h; i++) {
        for(int j=0; j<cursor_w; j++) {
            int px = x + j;
            int py = y + i;
            if(px >= 1280 || py >= 720) continue;
            int type = cursor_bitmap[i][j];
            if(type == 1) backbuffer[py * fb_width + px] = 0x1A1A1A; 
            if(type == 2) backbuffer[py * fb_width + px] = 0xFFFFFF; 
        }
    }
}

void mouse_draw() {
    // 1. Jeśli kursor się nie ruszył, nic nie rób (opcjonalna optymalizacja)
    // if (!mouse_moved) return; 
    // mouse_moved = false;

    // 2. Przywróć tło w starym miejscu (wymazanie starego kursora)
    restore_background(old_mouse_x, old_mouse_y);

    // 3. Zapisz tło w nowym miejscu
    save_background(mouse_x, mouse_y);

    // 4. Narysuj kursor w nowym miejscu
    draw_cursor_shape(mouse_x, mouse_y);

    // 5. Zaktualizuj "starą" pozycję
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
}