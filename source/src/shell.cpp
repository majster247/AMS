#include "shell.h"
#include "vfs.h"
#include "kernel.h"
#include "io.h"
#include "graphics.h"

extern volatile char cmd_buffer[128];
extern volatile int cmd_index;
extern volatile bool line_ready;

// --- Funkcje pomocnicze kolorów ---

void set_shell_color(uint8_t fg, uint8_t bg) {
    terminal_set_color(fg, bg);
}

void reset_shell_color() {
    terminal_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void shell_print(const char* str) {
    terminal_writestring(str);
    write_serial_string(str);
}

// Rysuje prompt w stylu segmentowym (jak na obrazku)
void shell_print_prompt() {
    int h, m, s;
    get_time(h, m, s);

    shell_print("\n");
    // Segment użytkownika
    set_shell_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    shell_print(" [kg@AMSOS] ");

    // Wyświetlanie czasu (na szaro, po prawej stronie lub w prompcie)
    set_shell_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    shell_print(" [");
    // Tu dodaj logikę dopisywania zera przed liczbą (np. 09:05:01)
    terminal_write_dec(h); shell_print(":");
    terminal_write_dec(m); shell_print(":");
    terminal_write_dec(s);
    shell_print("] ");

    reset_shell_color();
    shell_print("\n[$] ");
}

// --- Aplikacje / Komendy ---

void cmd_ls() {
    vfs_node* curr = vfs_root;
    shell_print("total 0\n");

    while (curr) {
        // Uprawnienia (symulowane) i kolory
        set_shell_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        shell_print(curr->type == FS_DIRECTORY ? "drwxr-xr-x " : "-rw-r--r-- ");
        
        set_shell_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        shell_print("ams  staff  ");

        // Rozmiar
        set_shell_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        char size_buf[16];
        // Tutaj Twoja logika konwersji size -> string (size_buf)
        
        int len = 0;
        uint32_t size = curr->size;
        if (size == 0) {
            size_buf[len++] = '0';
        } else {
            char rev_buf[16];
            int rev_len = 0;
            while (size > 0) {
                rev_buf[rev_len++] = '0' + (size % 10);
                size /= 10;
            }
            for (int i = rev_len - 1; i >= 0; i--) {
                size_buf[len++] = rev_buf[i];
            }
        }
        size_buf[len] = '\0';
        shell_print(size_buf);
        shell_print(" ");


        // Nazwa pliku z kolorem
        if (curr->type == FS_DIRECTORY) {
            set_shell_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        } else {
            set_shell_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
        shell_print(curr->name);
        shell_print("\n");

        curr = curr->next;
    }
    reset_shell_color();
}

void cmd_cat(char* filename) {
    vfs_node* file = vfs_find_node(vfs_root, filename);
    if (file) {
        uint8_t buf[1024];
        uint32_t read_bytes;
        uint32_t offset = 0;
        while ((read_bytes = vfs_read(file, offset, 1024, buf)) > 0) {
            for(uint32_t i=0; i<read_bytes; i++) {
                terminal_putchar(buf[i]);
            }
            offset += read_bytes;
        }
        shell_print("\n");
    } else {
        set_shell_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        shell_print("cat: error: file not found\n");
        reset_shell_color();
    }
}

void cmd_clear() {
    terminal_clear();
}

void cmd_whoami() {
    shell_print("majster (kernel mode)\n");
}

void shell_execute(char* cmd) {
    if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "ls -lah") == 0) {
        cmd_ls();
    } 
    else if (strncmp(cmd, "cat ", 4) == 0) {
        cmd_cat(cmd + 4);
    } 
    else if (strcmp(cmd, "clear") == 0) {
        cmd_clear();
    } 
    else if (strcmp(cmd, "whoami") == 0) {
        cmd_whoami();
    }
    else if (strcmp(cmd, "help") == 0) {
        shell_print("AMS-OS Commands: ls, cat, clear, whoami, help, edit, matrix\n");
    } 
    else if (strncmp(cmd, "edit ", 5) == 0) {
        cmd_edit(cmd + 5);
    }
    else if (strcmp(cmd, "matrix") == 0) {
        cmd_matrix();
    }
    else if (strcmp(cmd, "shutdown") == 0) {
        shell_print("Shutting down...\n");
        asm volatile ("cli"); // Wyłącz przerwania
        outb(0xB004, 0x2000); // Sygnalizuj ACPI o wyłączeniu
        while (1) { asm volatile ("hlt"); } // Zatrzymaj CPU
        
    }
    else if (strcmp(cmd, "reboot") == 0) {
        shell_print("Rebooting...\n");
        asm volatile ("cli"); // Wyłącz przerwania
        outb(0x64, 0xFE); // Wyślij sygnał resetu do kontrolera klawiatury
        while (1) { asm volatile ("hlt"); } // Zatrzymaj CPU
    }
    else if (strcmp(cmd, "sleep") == 0) {
        // usypiamy system włączając tryb oszczędzania energii i wielki zegar
        cmd_big_time();
    }
    else if (strcmp(cmd, "gop_test") == 0) {
        cmd_gop_test();
    }
    else if (cmd[0] == '\0') {
        // Nic nie rób przy samym enterze
    }
    else {
        set_shell_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        shell_print("ams-sh: command not found: ");
        shell_print(cmd);
        shell_print("\n");
        reset_shell_color();
    }
}

void shell_init() {
    terminal_clear();
    set_shell_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    shell_print("--- AMS-OS v0.1 Shell ---\n");
    shell_print("Type 'help' to see available commands.\n");
    reset_shell_color();
    shell_print_prompt();
}

void shell_update() {
    if (line_ready) {
        char cmd[128];
        int i = 0;
        while(cmd_buffer[i] != '\0' && i < 127) {
            cmd[i] = cmd_buffer[i];
            i++;
        }
        cmd[i] = '\0';

        cmd_index = 0;
        cmd_buffer[0] = '\0';
        line_ready = false; 

        shell_print("\n"); // Echo new line
        shell_execute(cmd);
        shell_print_prompt();
    }

    /*// Wejście z Serial (zdalne)
    if (serial_received()) {
        char c = serial_read();
        if (c == '\r' || c == '\n') {
            write_serial_char('\n'); // Echo new line
            shell_execute(serial_cmd_buffer); // Wykonaj to co przyszło przez RS232
            serial_clear_buffer();
        } else {
            write_serial_char(c); // Echo
            // Dodaj do serial_cmd_buffer...
        }
    }*/

}

void cmd_edit(char* filename) {
    terminal_clear();
    int cursor_x = 0;
    int cursor_y = 1;
    bool running = true;

    // 1. Rysujemy górny pasek (Status)
    for(int i=0; i<80; i++) terminal_put_at(i, 0, ' ', 0x70); // Szare tło
    terminal_writestring_at(" AMS-EDIT v0.1 | File: ", 0, 0);
    terminal_writestring_at(filename, 23, 0);
    terminal_writestring_at(" [ESC to Exit]", 64, 0);

    // 2. Rysujemy dolny pasek (Skróty)
    for(int i=0; i<80; i++) terminal_put_at(i, 24, ' ', 0x70);
    terminal_writestring_at(" ^O Write Out  ^X Exit    ^C Cancel", 2, 24);

    while(running) {
        terminal_set_cursor(cursor_x, cursor_y); // Przesuwamy hardware cursor
        
        char c = keyboard_get_char(); // Twoja funkcja czytająca jeden znak

        if (c == 27) { // ESC
            running = false;
        } else if (c == '\n') {
            cursor_y++;
            cursor_x = 0;
        } else if (c == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
                terminal_put_at(cursor_x, cursor_y, ' ', 0x0F);
            }
        } else {
            terminal_put_at(cursor_x++, cursor_y, c, 0x0F);
            if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
        }

        if (cursor_y >= 24) cursor_y = 23; // Prymitywny scroll-lock
    }

    terminal_clear();
    shell_init(); // Powrót do shella
}

bool check_esc_key() {
    // Implementacja zależy od Twojego systemu wejścia
    // Powinna zwracać true jeśli ESC zostało naciśnięte
    return false; // Tymczasowo zawsze false
}

//cdm matrix, prosty efekt matrixa w shellu spadające znaki jak w filmie
void cmd_matrix() {
    terminal_clear();
    const int width = 80;
    const int height = 25;
    int drops[80];

    // Inicjalizacja - każda kolumna zaczyna w innym miejscu
    for (int i = 0; i < width; i++) {
        drops[i] = rand() % height;
    }

    bool running = true;
    while (running) {
        for (int i = 0; i < width; i++) {
            // 1. Rysujemy nowy, jasny znak na dole kolumny
            char c = (rand() % 94) + 33;
            // 0x0A to jasny zielony, 0x02 to ciemny zielony
            terminal_put_at(i, drops[i], c, 0x0A);

            // 2. Co kilka klatek czyścimy znak wyżej, żeby nie zostawiać śladu
            // albo rysujemy tam ciemniejszy zielony dla efektu ogona
            int tail = (drops[i] - 1 + height) % height;
            terminal_put_at(i, tail, (rand() % 94) + 33, 0x02);

            // 3. Całkowite czyszczenie starego ogona (np. 5 znaków wyżej)
            int old_tail = (drops[i] - 5 + height) % height;
            terminal_put_at(i, old_tail, ' ', 0x07);

            // Przesuwamy drop w dół
            drops[i] = (drops[i] + 1) % height;
        }

        // KLUCZ: Krótkie opóźnienie, żeby oko zarejestrowało ruch
        for(volatile int delay=0; delay<1000000; delay++);

        // Sprawdź ESC (musisz użyć wersji NIEBLOKUJĄCEJ)
        // Jeśli keyboard_get_char() czeka na znak, Matrix nie ruszy!
        if(keyboard_get_char() == 27) { // 27 to kod ASCII dla ESC
            running = false;
        }
    }

    terminal_clear();
    shell_init();
}

//------------------------------------------------------------------------------------------------------------------------
//Wygaszacz ekranu
// Definicja cyfr 0-9 w formacie 3x5
const uint16_t digits[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b010, 0b010, 0b010, 0b010}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void draw_big_digit(int x, int y, int digit, uint8_t color_bg) {
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (digits[digit][row] & (1 << (2 - col))) {
                // Rysujemy podwójną spację, żeby cyfry nie były zbyt chude
                terminal_put_at(x + col*2,     y + row, ' ', (color_bg << 4));
                terminal_put_at(x + col*2 + 1, y + row, ' ', (color_bg << 4));
            } else {
                terminal_put_at(x + col*2,     y + row, ' ', 0x00);
                terminal_put_at(x + col*2 + 1, y + row, ' ', 0x00);
            }
        }
    }
}

void cmd_big_time() {
    terminal_clear();
    bool running = true;
    
    while (running) {
        int h, m, s;
        get_time(h, m, s);

        // start_x = 10 (lewy margines), start_y = 10 (środek wysokości)
        int x = 10;
        int y = 10;

        // GODZINY
        draw_big_digit(x,      y, h / 10, VGA_COLOR_WHITE);
        draw_big_digit(x + 7,  y, h % 10, VGA_COLOR_WHITE);

        // DWUKROPEK 1
        uint8_t colon_color = (s % 2 == 0) ? 0xFF : 0x00;
        terminal_put_at(x + 14, y + 1, ' ', colon_color);
        terminal_put_at(x + 14, y + 3, ' ', colon_color);

        // MINUTY
        draw_big_digit(x + 17, y, m / 10, VGA_COLOR_WHITE);
        draw_big_digit(x + 24, y, m % 10, VGA_COLOR_WHITE);

        // DWUKROPEK 2
        terminal_put_at(x + 31, y + 1, ' ', colon_color);
        terminal_put_at(x + 31, y + 3, ' ', colon_color);

        // SEKUNDY
        draw_big_digit(x + 34, y, s / 10, VGA_COLOR_WHITE);
        draw_big_digit(x + 41, y, s % 10, VGA_COLOR_WHITE);
        
        // STOPKA (rozdzielona na dwie linie, żeby nie nakładały się na siebie)
        set_shell_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        terminal_writestring_at("AMS-OS v0.1 by Hubert Topolski", 25, 20);
        set_shell_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_writestring_at("Nacisnij ESC, aby wyjsc", 28, 22);

        sleep(200);

        if (keyboard_get_char_nonblocking() == 27) {
            running = false;
        }
    }

    terminal_clear();
    shell_init();
}

//Remote shell update 
char remote_cmd_buffer[128];
int remote_index = 0;

void shell_update_remote() {
    if (serial_received()) {
        char c = serial_read();
        
        if (c == '\r' || c == '\n') {
            remote_cmd_buffer[remote_index] = '\0';
            write_serial_string("\r\n[Remote] Executing...\r\n");
            shell_execute(remote_cmd_buffer); // Ta sama funkcja co dla klawiatury!
            
            remote_index = 0;
            write_serial_string("\r\nAMSOS-REMOTE> ");
        } else if (c == '\b' || c == 127) {
            if (remote_index > 0) remote_index--;
        } else {
            remote_cmd_buffer[remote_index++] = c;
            write_serial_char(c); // Echo - żebyś widział co piszesz w Putty
        }
    }
}


//--------------------- GOP---------------------

void cmd_gop_test() {
    // Debug info na Serial
    write_serial_string("GOP Debug Info:\n");
    write_serial_string("Addr: "); write_serial_hex(fb.address);
    write_serial_string("\nRes: "); write_serial_dec(fb.width);
    write_serial_string("x"); write_serial_dec(fb.height);
    write_serial_string("\n");

    if (fb.address == 0) {
        write_serial_string("ERROR: Framebuffer address is 0! Musisz sparsowac Multiboot tag.\n");
        return;
    }


    // UWAGA: To zadziała tylko jeśli w bootloaderze poprosiłeś o tryb graficzny!
    for(uint32_t y = 0; y < fb.height; y++) {
        for(uint32_t x = 0; x < fb.width; x++) {
            uint32_t color = ((x * 255 / fb.width) << 16) | ((y * 255 / fb.height) << 8);
            graphics_put_pixel(x, y, color);
        }
    }
    
    // Czekaj na klawisz przez Serial, żeby wrócić do trybu tekstowego
    write_serial_string("Tryb graficzny aktywny. Naciśnij klawisz na Serialu, by wyjść...\n");
    serial_read();
}