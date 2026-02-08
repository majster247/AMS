#include "gui.h"
#include "kernel.h"
#include "graphics.h"

// Konstruktor
FileManagerWindow::FileManagerWindow(int x, int y) 
    : Window(x, y, 640, 400, "File System") {
    extern vfs_node* vfs_root;
    current_path = vfs_root;
    selected_idx = -1;
    hover_idx = -1;
}

// Metoda Rysowania
void FileManagerWindow::Draw() {
    if(minimized) return;
    Window::Draw(); // Rysuje ramkę bazową
    
    // Pasek narzędzi (Toolbar)
    int tb_y = y + TITLE_BAR_HEIGHT;
    graphics_draw_rect(x + 2, tb_y, width - 4, 30, COL_NORD1); // Tło paska
    graphics_draw_rect(x + 2, tb_y + 29, width - 4, 1, COL_NORD3); // Linia oddzielająca
    
    // Breadcrumbs / Path
    graphics_print(x + 10, tb_y + 8, "Path: / (Root)", COL_NORD4);

    // Obszar plików (Grid View)
    int start_x = x + 10;
    int start_y = tb_y + 40;
    int item_w = 80;
    int item_h = 90;
    int cols = (width - 20) / item_w;
    
    extern vfs_node* vfs_root;
    vfs_node* node = vfs_root;
    int i = 0;

    while (node) {
        int row = i / cols;
        int col = i % cols;
        int dx = start_x + (col * item_w);
        int dy = start_y + (row * item_h);

        // Hover effect
        bool is_hover = (hover_idx == i);
        if (is_hover || selected_idx == i) {
            // Używamy DrawRectAlpha dla efektu zaznaczenia
            graphics_draw_rect_alpha(dx, dy, item_w - 5, item_h - 5, COL_NORD8, 50); 
            graphics_draw_rect(dx, dy, item_w - 5, item_h - 5, COL_NORD8); // Obramowanie
        }

        // IKONA (Prosta grafika kodem)
        uint32_t icon_color = (node->type == FS_DIRECTORY) ? COL_NORD13 : COL_NORD6; // Żółty folder, biały plik
        
        // Rysujemy kartkę/folder
        graphics_draw_rect(dx + 20, dy + 10, 30, 40, icon_color);
        graphics_draw_rect(dx + 20, dy + 10, 30, 40, COL_NORD3); // Obrys ikony
        
        // Jeśli to plik tekstowy -> linie imitujące tekst
        if (node->type == FS_FILE) {
            graphics_draw_rect(dx + 24, dy + 20, 22, 2, COL_NORD3);
            graphics_draw_rect(dx + 24, dy + 26, 22, 2, COL_NORD3);
            graphics_draw_rect(dx + 24, dy + 32, 15, 2, COL_NORD3);
        }

        // Podpis (Nazwa pliku)
        char name_short[16];
        int nlen = 0; 
        while(node->name[nlen] && nlen < 12) { 
            name_short[nlen] = node->name[nlen]; 
            nlen++; 
        }
        name_short[nlen] = 0;
        graphics_print(dx + 5, dy + 60, name_short, COL_NORD4); // Jasny tekst

        // Rozmiar pliku
        char size_buf[32];
        if (node->size > 1024*1024) sprintf(size_buf, "%d MB", node->size / (1024*1024));
        else if (node->size > 1024) sprintf(size_buf, "%d KB", node->size / 1024);
        else sprintf(size_buf, "%d B", node->size);
        
        graphics_print(dx + 10, dy + 75, size_buf, COL_NORD3); // Szary tekst rozmiaru

        node = node->next;
        i++;
    }
}

// Obsługa ruchu myszy (Hover)
void FileManagerWindow::OnMouseMove(int rel_x, int rel_y) {
    // Obliczanie, nad którym plikiem jest mysz
    int start_y = TITLE_BAR_HEIGHT + 40;
    if (rel_y < start_y) { hover_idx = -1; return; }
    
    int item_w = 80; 
    int item_h = 90;
    int cols = (width - 20) / item_w;
    
    int col = (rel_x - 10) / item_w;
    int row = (rel_y - start_y) / item_h;
    
    if (col >= 0 && col < cols && row >= 0) {
        hover_idx = row * cols + col;
    } else {
        hover_idx = -1;
    }
}

// Obsługa kliknięcia (Otwieranie)
void FileManagerWindow::OnMouseDown(int rel_x, int rel_y) {
    Window::OnMouseDown(rel_x, rel_y); // Obsługa standardowa (przeciąganie, zamykanie)
    
    if (hover_idx != -1) {
        selected_idx = hover_idx;
        // Znajdź ten węzeł w liście VFS
        extern vfs_node* vfs_root;
        vfs_node* node = vfs_root;
        for(int i=0; i<selected_idx && node; i++) node = node->next;
        
        if (node) OpenFile(node);
    }
}

void FileManagerWindow::OpenFile(vfs_node* node) {
    write_serial_string("[FileMgr] Opening: ");
    write_serial_string(node->name);
    write_serial_string("\n");

    char* ext = nullptr;
    // Znajdź ostatnią kropkę
    for(int i=0; node->name[i]; i++) {
        if(node->name[i] == '.') ext = &node->name[i];
    }

    extern Desktop* desktop;

    // Sprawdź czy node->size > 0. Jeśli nie, EXT2 driver może źle czytać inode
    if (node->size == 0) {
        write_serial_string("[FileMgr] Warning: File size is 0!\n");
    }

    if (ext) {
        if (strcmp(ext, ".vid") == 0) {
            desktop->AddWindow(new VideoPlayerWindow(100, 100, node->name));
        }
        // Dodaj obsługę wielkich/małych liter lub innych rozszerzeń
        else if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".h") == 0) {
            // WAŻNE: Tutaj przekazujemy wskaźnik 'node'
            desktop->AddWindow(new NotepadWindow(150, 150, node));
        }
        else {
            // Nieznany plik? Otwórz w notatniku jako fallback (może to plik tekstowy bez rozszerzenia)
            desktop->AddWindow(new NotepadWindow(150, 150, node));
        }
    } else {
        // Brak rozszerzenia - też otwórz w notatniku
        desktop->AddWindow(new NotepadWindow(150, 150, node));
    }
}