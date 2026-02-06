#include "gui.h"
#include "kernel.h" // kmalloc, sprintf
#include "mouse.h"
#include "graphics.h" // graphics_draw_rect, graphics_print
#include "heap.h" // kmalloc

extern uint64_t ram_size_mb; // Zmienna globalna RAM

// Bezpieczne kopiowanie stringów (FIX CRASH!)
void str_copy(char* d, const char* s) {
    int i=0; while(s[i] && i<63) { d[i] = s[i]; i++; } d[i]=0;
}

// =======================
// KLASA WINDOW
// =======================

Window::Window(int x, int y, int w, int h, const char* t) {
    this->x = x; this->y = y; this->width = w; this->height = h;
    this->is_dragging = false; this->is_focused = false; this->should_close = false;
    
    // Zerowanie bufora tytułu (FIX CRASH!)
    for(int i=0; i<64; i++) this->title[i] = 0;
    str_copy(this->title, t);
}

void Window::Draw() {
    // Cień pod oknem
    graphics_draw_rect(x+4, y+4, width, height, 0x000000); // Cień

    // Tło i ramka 3D
    graphics_draw_rect(x, y, width, height, COL_BG);
    
    // Ramka zewnętrzna (3D Raised)
    graphics_draw_rect(x, y, width, 1, COL_WHITE);
    graphics_draw_rect(x, y, 1, height, COL_WHITE);
    graphics_draw_rect(x+width-1, y, 1, height, COL_DARK);
    graphics_draw_rect(x, y+height-1, width, 1, COL_DARK);

    // Pasek tytułu
    uint32_t tc = is_focused ? COL_TITLE : COL_TITLE_IA;
    graphics_draw_rect(x+3, y+3, width-6, TITLE_BAR_HEIGHT, tc);
    graphics_print(x+6, y+8, title, COL_WHITE);

    // Przycisk [X]
    int bx = x + width - 22, by = y + 5;
    // Rysujemy "Button" dla X
    graphics_draw_rect(bx, by, 18, 18, COL_BG);
    graphics_draw_rect(bx, by, 18, 1, COL_WHITE);      // Top
    graphics_draw_rect(bx, by, 1, 18, COL_WHITE);      // Left
    graphics_draw_rect(bx+17, by, 1, 18, COL_DARK);    // Right
    graphics_draw_rect(bx, by+17, 18, 1, COL_DARK);    // Bottom
    
    // Krzyżyk
    graphics_print(bx+5, by+3, "X", 0x000000);
}

void Window::OnMouseDown(int rel_x, int rel_y) {
    // Sprawdź przycisk X
    if (rel_y < TITLE_BAR_HEIGHT && rel_x > width - 25) should_close = true;
}

void Window::OnMouseUp() {}
void Window::OnMouseMove(int rel_x, int rel_y) { (void)rel_x; (void)rel_y; }
void Window::OnKeyboard(char c) { (void)c; }

// =======================
// KLASA TERMINAL
// =======================

TerminalWindow::TerminalWindow(int x, int y) : Window(x, y, 600, 400, "Terminal") {
    Clear();
    WriteString("AMS-OS v1.0 [Interactive Mode]\nType commands...\n\nroot@ams:~$ ");
}

void TerminalWindow::Clear() {
    for(int i=0; i<25; i++) for(int j=0; j<80; j++) buffer[i][j] = ' ';
    cursor_row = 0; cursor_col = 0;
}

void TerminalWindow::WriteChar(char c) {
    if (c == '\n') { cursor_row++; cursor_col = 0; }
    else if (c == '\b') { if(cursor_col > 0) { cursor_col--; buffer[cursor_row][cursor_col] = ' '; } }
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
    // Echo znaku
    WriteChar(c);
    if (c == '\n') WriteString("root@ams:~$ ");
}

void TerminalWindow::Draw() {
    Window::Draw(); // Ramka
    
    // Czarny ekran w środku
    int cx = x + 4, cy = y + TITLE_BAR_HEIGHT + 4;
    int cw = width - 8, ch = height - TITLE_BAR_HEIGHT - 8;
    
    // Wklęsła ramka terminala
    graphics_draw_rect(cx, cy, cw, ch, 0x000000);
    graphics_draw_rect(cx, cy, cw, 1, COL_DARK);
    graphics_draw_rect(cx, cy, 1, ch, COL_DARK);
    graphics_draw_rect(cx+cw-1, cy, 1, ch, COL_WHITE);
    graphics_draw_rect(cx, cy+ch-1, cw, 1, COL_WHITE);
    
    // Tekst
    for(int i=0; i<25; i++) {
        for(int j=0; j<80; j++) {
            if (buffer[i][j] != ' ') 
                graphics_draw_char(cx + 4 + j*8, cy + 4 + i*16, buffer[i][j], 0x00FF00);
        }
    }
    
    // Kursor (Mrugający)
    static int blink = 0; blink++;
    if ((blink % 60) < 30 && is_focused) {
        graphics_draw_rect(cx + 4 + cursor_col*8, cy + 4 + cursor_row*16 + 14, 8, 2, 0x00FF00);
    }
}

// =======================
// KLASA DESKTOP
// =======================

Desktop::Desktop() { 
    window_count = 0; active_window = nullptr; 
    start_menu_open = false; start_button_hover = false;
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
    for(int i=0; i<window_count; i++) if (windows[i] == win) idx = i;
    if (idx != -1) {
        for(int i=idx; i<window_count-1; i++) windows[i] = windows[i+1];
        window_count--;
        if (active_window == win) active_window = nullptr;
        // delete win; // TODO: Pamiętaj o delete w przyszłości
    }
}

void Desktop::BringToFront(Window* win) {
    if (window_count < 2) { active_window = win; win->is_focused = true; return; }
    if (active_window) active_window->is_focused = false;
    RemoveWindow(win);
    windows[window_count++] = win;
    active_window = win;
    win->is_focused = true;
}

void Desktop::Update(int mx, int my, bool left_click) {
    static bool prev_click = false;
    bool clicked = left_click && !prev_click;

    // Stałe wymiary (muszą być takie same jak w Draw!)
    int bar_y = 720 - TASKBAR_HEIGHT;
    int menu_w = 150;
    int menu_h = 120;
    int menu_y = bar_y - menu_h; // Menu rysuje się NAD paskiem

    // Logika Podświetlania Przycisku Start
    start_button_hover = (mx >= 2 && mx <= 72 && my >= bar_y + 2);

    if (clicked) {
        // 1. KLIKNIĘCIE W PRZYCISK START
        if (start_button_hover) {
            start_menu_open = !start_menu_open;
            prev_click = left_click; return;
        }
        
        // 2. KLIKNIĘCIE W MENU START (Jeśli otwarte)
        if (start_menu_open) {
            // Sprawdź czy kursor jest wewnątrz prostokąta menu
            if (mx >= 0 && mx <= menu_w && my >= menu_y && my < bar_y) {
                
                // Oblicz relatywną pozycję Y wewnątrz menu (0 to góra menu)
                int rel_y = my - menu_y;
                
                // Opcja 1: "Terminal" (Y: 15 do 35 - zgodnie z DrawStartMenu)
                // Dajemy mały margines błędu (np. 10-40)
                if (rel_y >= 10 && rel_y <= 40) {
                    AddWindow(new TerminalWindow(150, 150));
                    start_menu_open = false; // Zamknij menu po wyborze
                }
                
                // Opcja 2: XEyes
                else if (rel_y >= 35 && rel_y <= 60) {
                    AddWindow(new EyesWindow(300, 200));
                    start_menu_open = false;
                }
                
                // Opcja 3: "Shutdown" (Y: > 80)
                else if (rel_y >= 80) {
                     // Wyłącz QEMU (Hack dla starszych wersji)
                     // outw(0x604, 0x2000); 
                     start_menu_open = false;
                }
                
                prev_click = left_click; return;
            } else {
                // Kliknięcie poza menu -> zamknij je
                start_menu_open = false;
            }
        }
    }

    // 3. LOGIKA OKIEN (Drag & Drop, Focus)
    if (active_window && active_window->is_dragging) {
        if (left_click) {
            // Przesuwanie
            active_window->x = mx - drag_off_x;
            active_window->y = my - drag_off_y;
        } else {
            // Puszczenie myszy
            active_window->is_dragging = false;
        }
    } else if (clicked) {
        // Sprawdzamy kliknięcia w okna (od wierzchu do spodu)
        bool hit_window = false;
        for (int i = window_count - 1; i >= 0; i--) {
            Window* w = windows[i];
            
            // Kolizja Mysz <-> Okno
            if (mx >= w->x && mx <= w->x + w->width &&
                my >= w->y && my <= w->y + w->height) {
                
                BringToFront(w); // Wyciągnij na wierzch
                hit_window = true;
                
                // Sprawdź czy kliknięto Pasek Tytułu (górne 25px)
                if (my < w->y + TITLE_BAR_HEIGHT) {
                    // Sprawdź przycisk [X] (po prawej stronie)
                    if (mx > w->x + w->width - 25) {
                        w->should_close = true;
                        RemoveWindow(w);
                        break; // Okno usunięte, przerywamy
                    }
                    
                    // Rozpocznij przeciąganie
                    w->is_dragging = true;
                    drag_off_x = mx - w->x;
                    drag_off_y = my - w->y;
                } else {
                    // Kliknięcie w treść okna (przekazujemy do okna np. żeby postawiło kursor)
                    // w->OnMouseDown(mx - w->x, my - w->y);
                }
                break; // Trafiliśmy w okno, nie klikamy w te pod spodem
            }
        }
        
        // Jeśli kliknięto w tło (nie w okno, nie w pasek), odznacz aktywne okno
        if (!hit_window && my < bar_y) {
            if (active_window) active_window->is_focused = false;
            active_window = nullptr;
        }
    }
    
    prev_click = left_click;
}

void Desktop::Draw3DButton(int x, int y, int w, int h, const char* text, bool pressed) {
    graphics_draw_rect(x, y, w, h, COL_BG);
    if (pressed) { // Wklęsły
        graphics_draw_rect(x, y, w, 1, COL_DARK);
        graphics_draw_rect(x, y, 1, h, COL_DARK);
        graphics_draw_rect(x+w-1, y, 1, h, COL_WHITE);
        graphics_draw_rect(x, y+h-1, w, 1, COL_WHITE);
        graphics_print(x + 12, y + 6, text, COL_TEXT);
    } else { // Wypukły
        graphics_draw_rect(x, y, w, 1, COL_WHITE);
        graphics_draw_rect(x, y, 1, h, COL_WHITE);
        graphics_draw_rect(x+w-1, y, 1, h, COL_DARK);
        graphics_draw_rect(x, y+h-1, w, 1, COL_DARK);
        graphics_print(x + 10, y + 4, text, COL_TEXT);
    }
}

void Desktop::DrawTaskbar() {
    uint32_t bar_y = 720 - TASKBAR_HEIGHT;
    
    // Tło
    graphics_draw_rect(0, bar_y, 1280, TASKBAR_HEIGHT, COL_BG);
    graphics_draw_rect(0, bar_y, 1280, 2, COL_WHITE);

    // Przycisk Start
    Draw3DButton(2, bar_y + 2, 70, TASKBAR_HEIGHT - 4, "START", start_menu_open);

    // Info Systemowe (Z Twojego poprzedniego kodu)
    char buf[64];
    graphics_print(90, bar_y + 8, "AMS-OS x64", COL_TEXT);
    
    sprintf(buf, "RAM: %lu MB", ram_size_mb);
    graphics_print(200, bar_y + 8, buf, COL_TEXT);
    
    // Zegar (Wklęsły panel)
    int clock_x = 1280 - 90;
    graphics_draw_rect(clock_x, bar_y+4, 80, 22, COL_BG);
    graphics_draw_rect(clock_x, bar_y+4, 80, 1, COL_DARK); // Wklęsłość góra
    graphics_draw_rect(clock_x, bar_y+4, 1, 22, COL_DARK); // Wklęsłość lewo
    graphics_draw_rect(clock_x+79, bar_y+4, 1, 22, COL_WHITE);
    graphics_draw_rect(clock_x, bar_y+25, 80, 1, COL_WHITE);

    int h, m, s; get_time(h, m, s);
    char tstr[16];
    tstr[0]='0'+h/10; tstr[1]='0'+h%10; tstr[2]=':';
    tstr[3]='0'+m/10; tstr[4]='0'+m%10; tstr[5]=':';
    tstr[6]='0'+s/10; tstr[7]='0'+s%10; tstr[8]=0;
    graphics_print(clock_x + 10, bar_y + 8, tstr, COL_TEXT);
}

void Desktop::DrawStartMenu() {
    if (!start_menu_open) return;
    int h = 120, w = 150;
    int x = 0, y = 720 - TASKBAR_HEIGHT - h;

    // Tło i ramka
    graphics_draw_rect(x, y, w, h, COL_BG);
    graphics_draw_rect(x, y, w, 1, COL_WHITE);
    graphics_draw_rect(x, y, 1, h, COL_WHITE);
    graphics_draw_rect(x+w-1, y, 1, h, COL_DARK);
    graphics_draw_rect(x, y+h-1, w, 1, COL_DARK);
    
    // Pasek boczny
    graphics_draw_rect(2, y+2, 25, h-4, COL_TITLE); 
    
    // Elementy Menu (Simple Hover Simulation can be added in Update)
    graphics_print(35, y+15, "Terminal", COL_TEXT);
    graphics_print(35, y+40, "XEyes", COL_TEXT);
    
    // Separator
    graphics_draw_rect(30, y+80, w-35, 1, COL_DARK);
    graphics_draw_rect(30, y+81, w-35, 1, COL_WHITE);
    
    graphics_print(35, y+90, "Shutdown", COL_TEXT);
}

void Desktop::Draw() {
    fill_screen(COL_BG);
    graphics_draw_bmp_centered(); // Tapeta
    for(int i=0; i<window_count; i++) windows[i]->Draw();
    DrawTaskbar();
    DrawStartMenu();
}

void Desktop::HandleKeyboard(char c) {
    if (active_window) active_window->OnKeyboard(c);
}



// Prosty pierwiastek całkowity (Integer Sqrt)
int isqrt(int n) {
    if (n < 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

// Rysowanie koła (Bresenham) - dodaj to, jeśli nie masz w graphics.cpp
void graphics_draw_circle(int x0, int y0, int radius, uint32_t color) {
    int x = radius, y = 0, err = 0;
    while (x >= y) {
        graphics_put_pixel(x0 + x, y0 + y, color);
        graphics_put_pixel(x0 + y, y0 + x, color);
        graphics_put_pixel(x0 - y, y0 + x, color);
        graphics_put_pixel(x0 - x, y0 + y, color);
        graphics_put_pixel(x0 - x, y0 - y, color);
        graphics_put_pixel(x0 - y, y0 - x, color);
        graphics_put_pixel(x0 + y, y0 - x, color);
        graphics_put_pixel(x0 + x, y0 - y, color);
        y += 1;
        if (err <= 0) err += 2*y + 1;
        else { x -= 1; err += 2*(y - x) + 1; }
    }
}

// Rysowanie wypełnionego koła (na potrzeby źrenic)
void graphics_fill_circle(int x0, int y0, int radius, uint32_t color) {
    for(int y = -radius; y <= radius; y++) {
        for(int x = -radius; x <= radius; x++) {
            if(x*x + y*y <= radius*radius)
                graphics_put_pixel(x0+x, y0+y, color);
        }
    }
}



// =======================
// KLASA XEYES (OCZY)
// =======================

EyesWindow::EyesWindow(int x, int y) : Window(x, y, 200, 120, "XEyes Demo") {
    // Puste ciało, ramka rysuje się sama z klasy bazowej
}

void DrawEye(int eye_x, int eye_y, int radius, int pupil_radius) {
    // 1. Rysuj białko oka
    graphics_fill_circle(eye_x, eye_y, radius, 0xFFFFFF); // Biały
    graphics_draw_circle(eye_x, eye_y, radius, 0x000000); // Czarna obwódka

    // 2. Oblicz wektor do myszy
    int dx = mouse_x - eye_x;
    int dy = mouse_y - eye_y;
    
    // Odległość (Pitagoras)
    int dist = isqrt(dx*dx + dy*dy);
    
    // Maksymalny dystans, jaki źrenica może się odsunąć od środka
    int max_dist = radius - pupil_radius - 5;

    // Normalizacja i skalowanie (ograniczamy ruch źrenicy do wnętrza oka)
    int pupil_x = eye_x;
    int pupil_y = eye_y;

    if (dist > 0) {
        if (dist > max_dist) {
            pupil_x = eye_x + (dx * max_dist) / dist;
            pupil_y = eye_y + (dy * max_dist) / dist;
        } else {
            pupil_x = eye_x + dx;
            pupil_y = eye_y + dy;
        }
    }

    // 3. Rysuj źrenicę
    graphics_fill_circle(pupil_x, pupil_y, pupil_radius, 0x000000);
}

void EyesWindow::Draw() {
    // Rysuj ramkę okna
    Window::Draw();
    
    // Tło okna (szare) pod oczami, żeby zakryć poprzednią klatkę
    graphics_draw_rect(x+2, y+TITLE_BAR_HEIGHT+2, width-4, height-TITLE_BAR_HEIGHT-4, COL_BG);

    // Pozycje oczu (względem okna)
    int radius = 35;
    int left_eye_x = x + 50;
    int left_eye_y = y + 70;
    
    int right_eye_x = x + 150;
    int right_eye_y = y + 70;

    DrawEye(left_eye_x, left_eye_y, radius, 10);
    DrawEye(right_eye_x, right_eye_y, radius, 10);
}