#include "gui.h"
#include "kernel.h"
#include "graphics.h"
#include "heap.h" // Potrzebne do kmalloc/kfree

// Konstruktor z obsługą pliku
NotepadWindow::NotepadWindow(int x, int y, vfs_node* file) 
    : Window(x, y, 600, 450, "AMS Note") {
    
    // Zerowanie
    for(int i=0; i<100; i++) for(int j=0; j<80; j++) text_buffer[i][j] = 0;
    lines_count = 0; cursor_x = 0; cursor_y = 0; scroll_y = 0;
    
    if (file) {
        // Tytuł okna
        int i=0; const char* p = "Edit: "; while(p[i]) { title[i] = p[i]; i++; }
        int j=0; while(file->name[j] && i<60) title[i++] = file->name[j++]; title[i]=0;

        // Czytanie
        uint32_t size = file->size;
        if (size > 8000) size = 8000;

        char* temp = (char*)kmalloc(size + 1);
        if (temp) {
            // !!! FIX: Używamy uniwersalnego readera !!!
            if (file->read) {
                file->read(file, 0, size, (uint8_t*)temp);
                temp[size] = 0;

                // Parsowanie do bufora 2D
                int r=0, c=0;
                for(uint32_t k=0; k<size; k++) {
                    if(temp[k] == '\n') { r++; c=0; if(r>=100) break; }
                    else if(temp[k] >= 32 && temp[k] <= 126) {
                        if(c<79) text_buffer[r][c++] = temp[k];
                    }
                }
                lines_count = r + 1;
            }
            kfree(temp);
        }
    } else {
        lines_count = 1;
    }
}

void NotepadWindow::Draw() {
    // === FIX MINIMALIZACJI ===
    // Jeśli okno jest zminimalizowane, przerywamy natychmiast!
    // Inaczej Window::Draw() nic nie narysuje, a my narysujemy tekst "w powietrzu".
    if (minimized) return; 

    Window::Draw(); // Rysuje ramkę
    
    int content_x = x + 45; 
    int content_y = y + TITLE_BAR_HEIGHT + 5;
    int visible_lines = (height - TITLE_BAR_HEIGHT - 25) / 16;

    // Tło paska bocznego
    graphics_draw_rect(x + 2, content_y - 2, 40, height - TITLE_BAR_HEIGHT - 25, COL_NORD1);
    graphics_draw_rect(x + 42, content_y - 2, 1, height - TITLE_BAR_HEIGHT - 25, COL_NORD3);

    // Tło tekstu
    graphics_draw_rect(content_x, content_y - 2, width - 60, height - TITLE_BAR_HEIGHT - 25, COL_NORD6);

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = scroll_y + i;
        if (line_idx >= 100) break;

        int draw_y = content_y + (i * 16);

        // Numer linii
        char num_buf[8];
        int n = line_idx + 1;
        // Bieda-sprintf dla liczb (chyba że masz działający itoa/sprintf)
        if (n < 10) { num_buf[0] = '0' + n; num_buf[1] = 0; }
        else if (n < 100) { num_buf[0] = '0' + (n/10); num_buf[1] = '0' + (n%10); num_buf[2] = 0; }
        else { num_buf[0] = '9'; num_buf[1] = '9'; num_buf[2] = 0; }

        graphics_print(x + 5, draw_y, num_buf, COL_NORD4); 
        graphics_print(content_x + 5, draw_y, text_buffer[line_idx], 0x000000);

        // Kursor
        static int blink = 0; blink++;
        if (line_idx == cursor_y && (blink % 60 < 30) && is_focused) {
            int cx = content_x + 5 + (cursor_x * 8);
            graphics_draw_rect(cx, draw_y, 2, 16, 0x000000);
        }
    }

    // Status Bar
    int status_y = y + height - 20;
    graphics_draw_rect(x + 2, status_y, width - 4, 18, COL_NORD1);
    graphics_print(x + 10, status_y + 2, open_file_node ? "Mode: Read/Write" : "Mode: New File", COL_NORD4);
}

void NotepadWindow::OnKeyboard(char c) {
    // ... (Twoja obsługa klawiatury bez zmian) ...
    // Skopiuj to z poprzedniego pliku, bo tu się nic nie zmienia w logice pisania
    if (c == '\n') {
        if (cursor_y < 99) { cursor_y++; cursor_x = 0; if (cursor_y > lines_count) lines_count = cursor_y; }
    } else if (c == '\b') {
        if (cursor_x > 0) { cursor_x--; text_buffer[cursor_y][cursor_x] = 0; }
    } else if (c >= 32 && c <= 126) {
        if (cursor_x < 78) { text_buffer[cursor_y][cursor_x] = c; cursor_x++; text_buffer[cursor_y][cursor_x] = 0; }
    }
    if (cursor_y >= scroll_y + 20) scroll_y = cursor_y - 19;
}