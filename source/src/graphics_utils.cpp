#include "graphics.h"
#include "kernel.h"

void draw_status_bar() {
    uint32_t bar_color = 0x00AAAA; // Klasyczny Cyan z DOS-a
    uint32_t text_color = 0xFFFFFF;

    // Rysujemy tło paska na dole
    graphics_draw_rect(0, fb.height - 20, fb.width, 20, bar_color);

    // Używamy znaków specjalnych CP437 do ozdoby (np. 0x10 to strzałka ►)
    char status[128];
    // Tu Twoja funkcja sprintf lub ręczne budowanie stringa
    
    graphics_print(10, fb.height - 18, "AMS-OS 64-bit | GOP HD Mode | Dziala!", 0xFFFFFF);

    //wyświetlanie po prawej stronie aktualnego czasu
    // Pobieramy czas z get_time() (z kernel.h)
    int h, m, s;
    get_time(h, m, s);
    char time_str[16];
    // Ręczne formatowanie czasu HH:MM:SS
    time_str[0] = '0' + (h / 10);
    time_str[1] = '0' + (h % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (m / 10);
    time_str[4] = '0' + (m % 10);
    time_str[5] = ':';
    time_str[6] = '0' + (s / 10);
    time_str[7] = '0' + (s % 10);
    time_str[8] = '\0';
    int time_str_len = 8;
    graphics_print(fb.width - 10 - (time_str_len * 8), fb.height - 18, time_str, 0xFFFFFF);
}