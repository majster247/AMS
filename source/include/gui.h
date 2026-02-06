#pragma once
#include <stdint.h>
#include "graphics.h"

// Konfiguracja
#define MAX_WINDOWS 32
#define TASKBAR_HEIGHT 30
#define TITLE_BAR_HEIGHT 25

// Kolory (Styl Windows 2000/98)
#define COL_BG       0xC0C0C0 // Szary
#define COL_TITLE    0x000080 // Granatowy
#define COL_TITLE_IA 0x808080 // Szary (nieaktywny)
#define COL_TEXT     0x000000
#define COL_WHITE    0xFFFFFF
#define COL_DARK     0x404040
#define COL_LIGHT    0xE0E0E0

// === Klasa Bazowa Okna ===
class Window {
public:
    int32_t x, y, width, height;
    char title[64];
    bool is_dragging;
    bool should_close;
    bool is_focused;

    Window(int x, int y, int w, int h, const char* title);
    virtual ~Window() {} // Wirtualny destruktor

    // Metody wirtualne (Interakcja)
    virtual void Draw(); 
    virtual void OnMouseDown(int rel_x, int rel_y);
    virtual void OnMouseUp();
    virtual void OnMouseMove(int rel_x, int rel_y);
    virtual void OnKeyboard(char c);
};

// === Terminal w Oknie ===
class TerminalWindow : public Window {
private:
    char buffer[25][80]; // Bufor tekstowy 80x25
    int cursor_row, cursor_col;

public:
    TerminalWindow(int x, int y);
    void Draw() override;
    void OnKeyboard(char c) override;
    
    // API Terminala
    void WriteChar(char c);
    void WriteString(const char* str);
    void Clear();
};

class EyesWindow : public Window {
public:
    EyesWindow(int x, int y);
    void Draw() override;
    // Nie potrzebujemy obsługi klawiatury w oczach
};

// === Menedżer Pulpitu (Desktop) ===
class Desktop {
private:
    Window* windows[MAX_WINDOWS];
    int window_count;
    Window* active_window;
    
    // Stan Menu Start
    bool start_menu_open;
    bool start_button_hover;
    
    // Przeciąganie
    int drag_off_x, drag_off_y;

public:
    Desktop();
    void Init();
    
    void AddWindow(Window* win);
    void RemoveWindow(Window* win);
    void BringToFront(Window* win);
    
    // Główna pętla logiki (Kliknięcia, Ruch myszy)
    void Update(int mx, int my, bool left_click);
    
    // Główna pętla rysowania
    void Draw(); 
    
    // Przekazywanie klawiszy
    void HandleKeyboard(char c);

private:
    void DrawTaskbar();   
    void DrawStartMenu();
    // Rysowanie przycisku 3D
    void Draw3DButton(int x, int y, int w, int h, const char* text, bool pressed);
};