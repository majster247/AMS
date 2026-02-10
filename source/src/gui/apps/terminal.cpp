#include "gui.h"
#include "kernel.h"
#include "heap.h"
#include "vfs.h"
#include "ext2.h"

extern "C" int sys_exec(const char* path, int argc, char** argv);

int strlen(const char* s) { int l=0; while(s[l]) l++; return l; }
bool str_starts(const char* str, const char* prefix) {
    while(*prefix) if(*str++ != *prefix++) return false;
    return true;
}

TerminalWindow::TerminalWindow(int x, int y) : Window(x, y, 640, 420, "Terminal") {
    Clear();
    cmd_idx = 0;
    for(int i=0; i<128; i++) cmd_buffer[i] = 0;
    
    WriteString("AMS-OS x64 Shell v1.0\n");
    WriteString("Type 'help' for commands.\n");
    WriteString("root@ams:~$ ");
}

void TerminalWindow::Clear() {
    for(int i=0; i<25; i++) for(int j=0; j<80; j++) buffer[i][j] = ' ';
    cursor_row = 0; cursor_col = 0;
}

void TerminalWindow::WriteChar(char c) {
    if (c == '\n') { cursor_row++; cursor_col = 0; }
    else if (c == '\b') { 
        if(cursor_col > 0) { 
            cursor_col--; 
            buffer[cursor_row][cursor_col] = ' '; 
        } 
    }
    else if (c >= 32 && c <= 126) {
        if (cursor_col < 80) buffer[cursor_row][cursor_col++] = c;
    }
    // Scroll
    if (cursor_row >= 25) {
        for(int i=0; i<24; i++) for(int j=0; j<80; j++) buffer[i][j] = buffer[i+1][j];
        for(int j=0; j<80; j++) buffer[24][j] = ' ';
        cursor_row = 24;
    }
}

void TerminalWindow::WriteString(const char* str) { while(*str) WriteChar(*str++); }

void TerminalWindow::OnKeyboard(char c) {
    // Enter = Wykonaj
    if (c == '\n') {
        WriteChar('\n');
        cmd_buffer[cmd_idx] = 0; // Null terminator
        ExecuteCommand();
        cmd_idx = 0; // Reset bufora
        for(int i=0; i<128; i++) cmd_buffer[i] = 0;
        WriteString("root@ams:~$ ");
    } 
    // Backspace
    else if (c == '\b') {
        if (cmd_idx > 0) {
            cmd_idx--;
            cmd_buffer[cmd_idx] = 0;
            WriteChar('\b');
        }
    } 
    // Znaki drukowalne
    else if (c >= 32 && c <= 126) {
        if (cmd_idx < 127) {
            cmd_buffer[cmd_idx++] = c;
            WriteChar(c);
        }
    }
}
void TerminalWindow::ExecuteCommand() {
    if (cmd_idx == 0) return;

    // 1. Podział na komendę i argumenty (prosty parser)
    char* cmd = cmd_buffer;
    int argc = 0;
    char* argv[16];
    
    // Szukamy spacji
    char* p = cmd_buffer;
    while (*p && argc < 16) {
        // Pomiń spacje przed argumentem
        while (*p == ' ') p++;
        if (*p == 0) break;

        argv[argc++] = p; // Zapisz początek argumentu

        // Znajdź koniec argumentu
        while (*p && *p != ' ') p++;
        if (*p == ' ') {
            *p = 0; // Wstaw null-terminator w miejsce spacji
            p++;
        }
    }

    // 2. Obsługa komend wbudowanych
    if (strcmp(cmd, "help") == 0) {
        WriteString("Commands: ls, cat, clear, reboot, [path_to_program]\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        Clear(); cursor_row = -1;
    }
    else if (strcmp(cmd, "reboot") == 0) {
        // sys_reboot(); // Jeśli masz taki syscall
    }
    else if (strcmp(cmd, "ls") == 0) {
        // Twoja implementacja LS (bez zmian)
        extern vfs_node* vfs_root;
        vfs_node* node = vfs_root;
        while(node) {
            if (node->type == FS_DIRECTORY) WriteString("[DIR]  ");
            else WriteString("[FILE] ");
            WriteString(node->name);
            WriteString("\n");
            node = node->next;
        }
    }
    else if (strcmp(cmd, "cat") == 0) {
        if (argc < 2) { WriteString("Usage: cat <filename>\n"); return; }
        // Twoja implementacja CAT (użyj zmiennej 'argv[1]' jako nazwy pliku)
        // ... (tutaj wklej swoją logikę cat, ale użyj zmiennej argv[1] zamiast hardcodowanej nazwy)
    }
    // 3. PRÓBA URUCHOMIENIA PROGRAMU (np. /tcc)
    else {
        // Sprawdzamy czy to plik (zaczyna się od / lub ma nazwę)
        // Musisz mieć zaimplementowany syscall sys_exec w jądrze!
        
        // Przygotowanie argumentów dla exec (prosta wersja: 1 argument)
        
        WriteString("Executing: "); WriteString(cmd); WriteString("\n");
        
        int ret = sys_exec(argv[0], argc, argv);
        
        if (ret == -1) {
            WriteString("Error: Unknown command or file not found.\n");
        } else {
            WriteString("Process finished.\n");
        }
    }
}

void TerminalWindow::Draw() {
    if (minimized) return;
    Window::Draw(); // Ramka
    
    // Tło terminala (Półprzezroczyste)
    int cx = x + 5, cy = y + TITLE_BAR_HEIGHT + 5;
    graphics_draw_rect_alpha(cx, cy, width - 10, height - TITLE_BAR_HEIGHT - 10, 0x101010, 220);

    for(int i=0; i<25; i++) {
        for(int j=0; j<80; j++) {
            if (buffer[i][j] != ' ') {
                graphics_draw_char(cx + j*8, cy + i*16, buffer[i][j], COL_NORD4);
            }
        }
    }
    
    // Kursor
    static int blink = 0; blink++;
    if ((blink % 60) < 30 && is_focused) {
        graphics_draw_rect(cx + cursor_col*8, cy + cursor_row*16 + 12, 8, 2, COL_NORD8);
    }
}

void TerminalWindow::HandleInput(char c) {
    //pełna obsługa klawiatury jest w OnKeyboard, ale można tu dodać dodatkowe skróty klawiszowe itp.
    //Trzeba też z tego miejsca przekierować do OnKeyboard, żeby nie psuć istniejącej logiki. Na razie po prostu przekazujemy dalej:
    OnKeyboard(c);
}