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

    // Obszar plików (Grid View) wyświetlany jako lista jak na windowsie z plikami jako prostokąty z nazwą i możliwośćią kliknięcia
    // Każdy plik jest buttonem, który można kliknąć, aby go otworzyć (jeśli to katalog) lub edytować (jeśli to plik)
    //Taki button wygląda jak na windowsie pliki są wyświetlane jako lista od lewej do prawej strony okna w strukturze:
    // [ Ikona ] [ Nazwa Pliku ] [Typ] [Rozmiar] (rozmiar nie jest podawany dla katalogów, jest odpowiednio konwerowany na KB, MB, GB)
    // Działa to jak lista plików, ale z ładniejszym wyglądem i możliwością kliknięcia w każdy element, aby go otworzyć lub wejść do katalogu
    int item_w = 80;
    int item_h = 90;
    int cols = (width - 20) / item_w;
    int idx = 0;
    vfs_node* node = current_path;
    int start_y = tb_y + 40;
    while(node) {
        int col = idx % cols;
        int row = idx / cols;
        int item_x = x + 10 + col * item_w;
        int item_y = start_y + row * item_h;

        // Tło elementu (podświetlenie)
        uint32_t bg_col = COL_NORD2;
        if (idx == selected_idx) bg_col = COL_NORD3; // Wybrany
        else if (idx == hover_idx) bg_col = COL_NORD1; // Hover
        
        graphics_draw_rect(item_x, item_y, item_w - 10, item_h - 10, bg_col);
        
        // Ikona (prosty kwadrat, można rozbudować o różne ikony dla katalogów/plików)
        uint32_t icon_col = (node->type == 1) ? COL_NORD8 : COL_NORD9; // Katalogi vs Pliki
        graphics_draw_rect(item_x + 10, item_y + 10, 40, 40, icon_col);
        
        // Nazwa pliku
        graphics_print(item_x + 10, item_y + 60, node->name, COL_NORD6);
        
        node = node->next;
        idx++;
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

    extern Desktop* main_desktop; // Używamy globalnego wskaźnika do Desktopu, który jest inicjalizowany w kernel.cpp

    // Sprawdź czy node->size > 0. Jeśli nie, EXT2 driver może źle czytać inode
    if (node->size == 0) {
        write_serial_string("[FileMgr] Warning: File size is 0!\n");
    }

    if (ext) {
        if (strcmp(ext, ".vid") == 0) {
            main_desktop->AddWindow(new VideoPlayerWindow(100, 100, node->name));
        }
        // Dodaj obsługę wielkich/małych liter lub innych rozszerzeń
        else if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".h") == 0) {
            // WAŻNE: Tutaj przekazujemy wskaźnik 'node'
            main_desktop->AddWindow(new NotepadWindow(150, 150, node));
        }
        else {
            // Nieznany plik? Otwórz w notatniku jako fallback (może to plik tekstowy bez rozszerzenia)
            main_desktop->AddWindow(new NotepadWindow(150, 150, node));
        }
    } else {
        // Brak rozszerzenia - też otwórz w notatniku
        main_desktop->AddWindow(new NotepadWindow(150, 150, node));
    }
}