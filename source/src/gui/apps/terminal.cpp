#include "gui.h"
#include "kernel.h"
#include "heap.h"
#include "vfs.h"
#include "ext2.h"


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

    // ... (obsługa help, clear, info bez zmian) ...
    if (strcmp(cmd_buffer, "help") == 0) {
        WriteString("Available commands: ls, cat, clear, info, reboot\n");
    }
    else if (strcmp(cmd_buffer, "clear") == 0) {
        Clear(); cursor_row = -1;
    }
    else if (strcmp(cmd_buffer, "reboot") == 0) {
        // outb(0x64, 0xFE); // Reset CPU
    }
    // === LS ===
    else if (strcmp(cmd_buffer, "ls") == 0) {
        extern vfs_node* vfs_root;
        vfs_node* node = vfs_root;
        while(node) {
            if (node->type == FS_DIRECTORY) WriteString("[DIR]  ");
            else WriteString("[FILE] ");
            
            WriteString(node->name);
            char sz[32]; 
            // Prosta konwersja rozmiaru
            if (node->size < 1024) sprintf(sz, " (%d B)", node->size);
            else sprintf(sz, " (%d KB)", node->size/1024);
            WriteString(sz);
            WriteString("\n");
            node = node->next;
        }
    }
    // === CAT (FIX CRASH) ===
    else if (str_starts(cmd_buffer, "cat ")) {
        char* filename = cmd_buffer + 4; 
        
        extern vfs_node* vfs_root;
        vfs_node* node = vfs_root;
        bool found = false;
        
        while(node) {
            if (strcmp(node->name, filename) == 0) {
                found = true;
                if (node->type == FS_DIRECTORY) {
                    WriteString("Error: Is a directory.\n");
                } else {
                    // CZYTANIE PLIKU (Fix)
                    uint32_t size = node->size;
                    if (size > 4000) size = 4000; // Limit wyświetlania w terminalu
                    
                    char* buf = (char*)kmalloc(size + 1);
                    if (buf) {
                        // !!! UŻYWAMY WSKAŹNIKA NA FUNKCJĘ Z WĘZŁA !!!
                        // node->read wskazuje na tar_read (dla initrd) lub ext2_read_node (dla dysku)
                        if (node->read) {
                            node->read(node, 0, size, (uint8_t*)buf);
                            buf[size] = 0;
                            WriteString(buf);
                            WriteString("\n");
                        } else {
                            WriteString("Error: File not readable (no driver).\n");
                        }
                        kfree(buf);
                    } else {
                        WriteString("Error: Out of memory.\n");
                    }
                }
                break;
            }
            node = node->next;
        }
        if (!found) WriteString("Error: File not found.\n");
    }
    else {
        WriteString("Unknown command.\n");
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