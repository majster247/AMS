#include "gui.h"
#include "kernel.h"
#include "graphics.h"
#include "mouse.h"

extern uint64_t ram_size_mb;
extern uint32_t fb_width;
extern uint32_t fb_height;

extern "C" void graphics_draw_bmp_centered(); 

void str_copy(char* d, const char* s) {
    int i=0; while(s[i] && i<63) { d[i] = s[i]; i++; } d[i]=0;
}

// === POMOCNICZE: ZAOKRĄGLONE ROGI ===

// Rysuje wypełnione koło (ćwiartkę) dla rogów
void DrawCorner(int cx, int cy, int r, uint32_t color, int quadrant) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (y >= x) {
        // Rysujemy linie poziome, aby wypełnić koło
        if (quadrant == 1) { // Prawa-Góra
            for(int i=cx; i<=cx+x; i++) graphics_put_pixel(i, cy-y, color);
            for(int i=cx; i<=cx+y; i++) graphics_put_pixel(i, cy-x, color);
        }
        else if (quadrant == 2) { // Lewa-Góra
            for(int i=cx-x; i<=cx; i++) graphics_put_pixel(i, cy-y, color);
            for(int i=cx-y; i<=cx; i++) graphics_put_pixel(i, cy-x, color);
        }
        else if (quadrant == 3) { // Lewa-Dół
            for(int i=cx-x; i<=cx; i++) graphics_put_pixel(i, cy+y, color);
            for(int i=cx-y; i<=cx; i++) graphics_put_pixel(i, cy+x, color);
        }
        else if (quadrant == 4) { // Prawa-Dół
            for(int i=cx; i<=cx+x; i++) graphics_put_pixel(i, cy+y, color);
            for(int i=cx; i<=cx+y; i++) graphics_put_pixel(i, cy+x, color);
        }

        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}

// Główna funkcja do zaokrąglonych prostokątów
void Desktop::DrawRoundedRect(int x, int y, int w, int h, int r, uint32_t color) {
    // 1. Środek (Krzyż)
    graphics_draw_rect(x + r, y, w - 2*r, h, color); // Pionowy środek
    graphics_draw_rect(x, y + r, r, h - 2*r, color); // Lewy bok
    graphics_draw_rect(x + w - r, y + r, r, h - 2*r, color); // Prawy bok

    // 2. Rogi
    DrawCorner(x + r, y + r, r, color, 2);         // Lewa-Góra
    DrawCorner(x + w - r - 1, y + r, r, color, 1); // Prawa-Góra
    DrawCorner(x + r, y + h - r - 1, r, color, 3); // Lewa-Dół
    DrawCorner(x + w - r - 1, y + h - r - 1, r, color, 4); // Prawa-Dół
}

// Wersja z Alpha (Uproszczona: rysuje prostokąt alpha, rogi są pełne - dla wydajności w tym demie)
void Desktop::DrawRoundedRectAlpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    // Na razie użyjmy zwykłego rect alpha dla środka, rogi pomińmy w alpha blendingu dla szybkości,
    // albo użyjmy zwykłego prostokąta, bo alpha na rogach programowo jest bardzo wolna.
    graphics_draw_rect_alpha(x, y, w, h, color, alpha);
}

// === WINDOW ===

Window::Window(int x, int y, int w, int h, const char* t) {
    this->x = x; this->y = y; this->width = w; this->height = h;
    this->is_dragging = false; this->is_focused = false; this->should_close = false;
    this->minimized = false;
    for(int i=0; i<64; i++) this->title[i] = 0;
    str_copy(this->title, t);
}

void Window::Draw() {
    if (minimized) return;

    int r = 8; // Promień zaokrąglenia okna

    // Cień pod oknem (prosty)
    if (!is_focused) {
        // graphics_draw_rect_alpha(x+6, y+6, width, height, 0x000000, 40); // Cień zjada FPS, można wyłączyć
    }

    // Tło Okna (Zaokrąglone)
    Desktop::DrawRoundedRect(x, y, width, height, r, COL_NORD0);

    // Obramowanie (Border) - rysujemy nieco większy zaokrąglony prostokąt pod spodem, 
    // ale najprościej narysować go "ręcznie" jako linię.
    // Dla uproszczenia: w tym systemie tło okna robi za border, bo treść rysujemy z marginesem.

    // Pasek Tytułu (Zaokrąglony tylko u góry)
    // Rysujemy prostokąt, który ucinamy na dole
    uint32_t title_bg = COL_NORD1;
    Desktop::DrawRoundedRect(x, y, width, TITLE_BAR_HEIGHT + r, r, title_bg); // Tło tytułu wchodzi głębiej
    graphics_draw_rect(x, y + TITLE_BAR_HEIGHT, width, r, COL_NORD0); // Przykrywamy dół kolorem tła okna (to prostuje dolne rogi paska)

    // Linia oddzielająca pasek
    graphics_draw_rect(x, y + TITLE_BAR_HEIGHT, width, 1, COL_NORD3);

    // Tytuł
    uint32_t title_col = is_focused ? COL_NORD6 : COL_NORD4;
    graphics_print(x + 15, y + 8, title, title_col);

    // Kontrolki (Traffic Lights style)
    int btn_y = y + 8;
    int btn_size = 12;
    
    // Zamknij (Czerwony)
    Desktop::DrawRoundedRect(x + width - 25, btn_y, btn_size, btn_size, 4, COL_NORD11);
    // Minimalizuj (Żółty/Pomarańczowy)
    Desktop::DrawRoundedRect(x + width - 45, btn_y, btn_size, btn_size, 4, COL_NORD13);
}

void Window::OnMouseDown(int rel_x, int rel_y) {
    if (rel_y < TITLE_BAR_HEIGHT) {
        if (rel_x > width - 25) should_close = true;
        else if (rel_x > width - 45 && rel_x < width - 25) minimized = true;
    }
}
void Window::OnMouseUp() {}
void Window::OnMouseMove(int rel_x, int rel_y) { (void)rel_x; (void)rel_y; }
void Window::OnKeyboard(char c) { (void)c; }

// === DESKTOP ===

Desktop::Desktop() { 
    window_count = 0; active_window = nullptr; 
    start_menu_open = false; start_button_hover = false;
    bar_w = 900; bar_h = 50; 
}
void Desktop::Init() {}

void Desktop::AddWindow(Window* win) {
    if (window_count < MAX_WINDOWS) {
        windows[window_count++] = win;
        BringToFront(win);
    }
}

void Desktop::RemoveWindow(Window* win) {
    int idx = -1;
    for(int i=0; i<window_count; i++) if(windows[i] == win) idx = i;
    if (idx != -1) {
        // delete win; // TODO
        for(int i=idx; i<window_count-1; i++) windows[i] = windows[i+1];
        window_count--;
        if (active_window == win) {
            active_window = (window_count > 0) ? windows[window_count-1] : nullptr;
            if (active_window) active_window->is_focused = true;
        }
    }
}

void Desktop::BringToFront(Window* win) {
    if (window_count < 2) { active_window = win; win->is_focused = true; win->minimized = false; return; }
    int idx = -1;
    for(int i=0; i<window_count; i++) if(windows[i] == win) idx = i;
    if (idx != -1) {
        for(int i=idx; i<window_count-1; i++) windows[i] = windows[i+1];
        windows[window_count-1] = win;
    }
    if (active_window) active_window->is_focused = false;
    active_window = win;
    win->is_focused = true;
    win->minimized = false;
}

void Desktop::DrawBackgroundGrid() {
    // 1. CZYŚCIMY CAŁY EKRAN KOLOREM TŁA (To usuwa duchy i artefakty paska)
    fill_screen(COL_NORD0); 
    
    // 2. Rysujemy Tapetę (centrujemy ją)
    graphics_draw_bmp_centered();
}

void Desktop::DrawTaskbar() {
    // Obliczamy pozycję
    bar_x = (fb_width - bar_w) / 2;
    
    extern int32_t mouse_y;
    int target_y = (mouse_y > fb_height - 70) ? fb_height - bar_h - 15 : fb_height - 5; // Prawie znika
    
    static int current_bar_y = fb_height - 5;
    
    // Płynna animacja
    if (current_bar_y < target_y) current_bar_y += 3;
    if (current_bar_y > target_y) current_bar_y -= 3;
    
    bar_y = current_bar_y;

    // Tło Paska (Zaokrąglone, Pływające)
    // Rysujemy cień
    DrawRoundedRectAlpha(bar_x + 4, bar_y + 4, bar_w, bar_h, 12, 0x000000, 80);
    // Rysujemy właściwy pasek (ciemny, Nord1)
    DrawRoundedRect(bar_x, bar_y, bar_w, bar_h, 12, COL_NORD1);
    
    // Subtelny obrys
    // (Można pominąć dla czystszego wyglądu, lub użyć Rect bez wypełnienia)

    // Przycisk Start (Kółko po lewej)
    int start_dim = 36;
    int start_x = bar_x + 10;
    int start_y = bar_y + (bar_h - start_dim)/2;
    DrawRoundedRect(start_x, start_y, start_dim, start_dim, 10, 0xFF4500); // Orange
    graphics_print(start_x + 12, start_y + 10, "A", 0xFFFFFF);

    // === TASK SWITCHING (Karty Okien) ===
    int task_x = bar_x + 60;
    int task_w = 40;
    int task_h = 40;
    int task_y = bar_y + (bar_h - task_h)/2;

    for (int i = 0; i < window_count; i++) {
        Window* w = windows[i];
        
        uint32_t bg_col = COL_NORD2;
        if (w == active_window && !w->minimized) bg_col = COL_NORD3; // Jaśniejszy jeśli aktywny
        if (w->minimized) bg_col = COL_NORD0; // Ciemniejszy jeśli zminimalizowany

        DrawRoundedRect(task_x, task_y, task_w, task_h, 8, bg_col);
        
        char initial[2] = { w->title[0], 0 };
        graphics_print(task_x + 15, task_y + 12, initial, COL_NORD6);

        // Wskaźnik (Kropka pod ikoną)
        if (!w->minimized) {
            uint32_t dot_col = (w == active_window) ? COL_NORD14 : COL_NORD4; // Zielony vs Biały
            graphics_draw_rect(task_x + 15, task_y + 32, 10, 2, dot_col);
        }

        task_x += 50; // Odstęp
    }
}

void Desktop::DrawLauncher() {
    if (!start_menu_open) return;
    int menu_w = 400; int menu_h = 320;
    int x = (fb_width - menu_w) / 2; int y = (fb_height - menu_h) / 2;

    // Cień i Tło Menu (Zaokrąglone)
    DrawRoundedRect(x + 8, y + 8, menu_w, menu_h, 12, 0x101010); // Fake shadow
    DrawRoundedRect(x, y, menu_w, menu_h, 12, COL_NORD1);
    
    // Nagłówek
    graphics_print(x + 20, y + 20, "APPS", COL_NORD8);
    graphics_draw_rect(x + 20, y + 40, menu_w - 40, 2, COL_NORD2);

    const char* apps[] = {"Terminal", "XEyes", "Files", "Video", "Settings", "Shutdown"};
    int ty = y + 60;
    for (int i = 0; i < 6; i++) {
        // Hover effect symulowany (można dodać logikę w Update)
        // Rysujemy "Button" tło
        DrawRoundedRect(x + 20, ty, menu_w - 40, 30, 6, COL_NORD2);
        
        graphics_print(x + 40, ty + 7, apps[i], COL_NORD6);
        ty += 40;
    }
}

void Desktop::Draw() {
    // 1. Full Clear (Fix na duchy przy zamykaniu)
    DrawBackgroundGrid(); 
    
    // 2. Okna
    for(int i=0; i<window_count; i++) windows[i]->Draw();
    
    // 3. UI
    DrawTaskbar();
    DrawLauncher();
}

void Desktop::Update(int mx, int my, bool left_click) {
    static bool prev_click = false;
    bool clicked = left_click && !prev_click;

    // START (Kliknięcie w kółko)
    if (clicked && my >= bar_y && my <= bar_y + bar_h && mx >= bar_x && mx <= bar_x + 50) {
        start_menu_open = !start_menu_open;
        prev_click = left_click; return;
    }

    // TASK SWITCHING (Kliknięcie w ikony na pasku)
    if (clicked && my >= bar_y && my <= bar_y + bar_h && mx > bar_x + 60) {
        int icon_idx = (mx - (bar_x + 60)) / 50; // 50px odstępu
        if (icon_idx >= 0 && icon_idx < window_count) {
            Window* target = windows[icon_idx];
            if (target == active_window && !target->minimized) {
                target->minimized = true;
                active_window = nullptr;
            } else {
                BringToFront(target);
            }
            prev_click = left_click; return;
        }
    }

    // LAUNCHER
    if (clicked && start_menu_open) {
        int menu_w = 400; int menu_h = 320;
        int lx = (fb_width - menu_w) / 2; int ly = (fb_height - menu_h) / 2;
        
        if (mx >= lx && mx <= lx + menu_w && my >= ly && my <= ly + menu_h) {
            int rel_y = my - ly;
            // 60 offset, 40 height per item
            if (rel_y >= 60) {
                int item = (rel_y - 60) / 40;
                switch(item) {
                    case 0: AddWindow(new TerminalWindow(150, 150)); break;
                    case 1: AddWindow(new EyesWindow(300, 200)); break;
                    case 2: AddWindow(new FileManagerWindow(400, 150)); break;
                    case 3: AddWindow(new VideoPlayerWindow(100, 100, "movie.vid")); break;
                    case 4: AddWindow(new SettingsWindow(200, 200)); break;
                    case 5: start_menu_open = false; break;
                }
                start_menu_open = false;
            }
        } else {
            start_menu_open = false;
        }
        prev_click = left_click; return;
    }

    // DRAG & CLOSE
    if (active_window && active_window->is_dragging) {
        if (left_click) {
            active_window->x = mx - drag_off_x;
            active_window->y = my - drag_off_y;
        } else {
            active_window->is_dragging = false;
        }
    } else if (clicked) {
        bool hit = false;
        for (int i = window_count - 1; i >= 0; i--) {
            Window* w = windows[i];
            if (w->minimized) continue;

            if (mx >= w->x && mx <= w->x + w->width && my >= w->y && my <= w->y + w->height) {
                BringToFront(w); hit = true;
                if (my < w->y + TITLE_BAR_HEIGHT) {
                    // X
                    if (mx > w->x + w->width - 25) { w->should_close = true; RemoveWindow(w); break; }
                    // _
                    else if (mx > w->x + w->width - 45) { w->minimized = true; active_window = nullptr; break; }
                    
                    w->is_dragging = true;
                    drag_off_x = mx - w->x;
                    drag_off_y = my - w->y;
                } else {
                    w->OnMouseDown(mx - w->x, my - w->y);
                }
                break;
            }
        }
    }
    prev_click = left_click;
}

void Desktop::DrawFlatButton(int x, int y, int w, int h, const char* label, uint32_t color, bool hovered) {
    uint32_t bg_col = hovered ? COL_NORD3 : COL_NORD2;
    graphics_draw_rect(x, y, w, h, bg_col);
    graphics_print(x + 10, y + 7, label, color);
}

void Desktop::HandleKeyboard(char c) {


        // ALT + F4 do zamykania aktywnego okna
    if (key_alt_pressed && c == 0x73) { // F4
        if (active_window) {
            active_window->should_close = true;
            RemoveWindow(active_window);
        }
        return;
    }

    //ALT + TAB do przełączania okien
    if (key_alt_pressed && c == 0x09) { // Tab
        if (window_count > 1) {
            int idx = -1;
            for(int i=0; i<window_count; i++) if(windows[i] == active_window) idx = i;
            if (idx != -1) {
                int next_idx = (idx + 1) % window_count;
                BringToFront(windows[next_idx]);
            }
        }
        return;
    }
    
    if (active_window) active_window->OnKeyboard(c);


}
