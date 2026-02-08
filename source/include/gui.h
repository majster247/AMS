#pragma once
#include <stdint.h>
#include "graphics.h"
#include "vfs.h"
#include "ext2.h"

// Konfiguracja GUI
#define MAX_WINDOWS 32
#define TASKBAR_HEIGHT 45
#define TITLE_BAR_HEIGHT 28

// === NORD THEME (Pro Colors) ===
#define COL_NORD0  0x2E3440 // Tło
#define COL_NORD1  0x3B4252 // Belki
#define COL_NORD2  0x434C5E // Selection
#define COL_NORD3  0x4C566A // Ramki/Komentarze
#define COL_NORD4  0xD8DEE9 // Tekst Główny
#define COL_NORD5  0xE5E9F0 // Tekst Jasny
#define COL_NORD6  0xECEFF4 // Biały
#define COL_NORD7  0x8FBCBB // Turkus
#define COL_NORD8  0x88C0D0 // Cyan (Akcent)
#define COL_NORD9  0x81A1C1 // Niebieski
#define COL_NORD10 0x5E81AC // Ciemny niebieski
#define COL_NORD11 0xBF616A // Czerwony (Error)
#define COL_NORD12 0xD08770 // Pomarańczowy
#define COL_NORD13 0xEBCB8B // Żółty
#define COL_NORD14 0xA3BE8C // Zielony (Success)

// Aliasy
#define COL_BG          COL_NORD0
#define COL_WIN_BG      COL_NORD0
#define COL_WIN_BORDER  COL_NORD8
#define COL_TITLE_BAR   COL_NORD1
#define COL_TEXT        COL_NORD4

// === KLASA BAZOWA OKNA ===
class Window {
public:
    int32_t x, y, width, height;
    char title[64];
    bool is_dragging;
    bool should_close;
    bool is_focused;
    bool minimized;

    Window(int x, int y, int w, int h, const char* title);
    virtual ~Window() {}

    virtual void Draw(); 
    virtual void OnMouseDown(int rel_x, int rel_y);
    virtual void OnMouseUp();
    virtual void OnMouseMove(int rel_x, int rel_y);
    virtual void OnKeyboard(char c);
};

// === APLIKACJE (Deklaracje) ===

// 1. Terminal (Hacker Console)
class TerminalWindow : public Window {
private:
    char buffer[25][80];
    int cursor_row, cursor_col;
    char cmd_buffer[128];
    int cmd_idx;

    char cmd_history[80][128];
    int history_count;
public:
    TerminalWindow(int x, int y);
    void Draw() override;
    void OnKeyboard(char c) override;
    void WriteChar(char c);
    void WriteString(const char* str);
    void Clear();

    void ExecuteCommand();
};

// 2. AMS Note (Edytor a'la Kate)
class NotepadWindow : public Window {
private:
    char text_buffer[100][80];
    int lines_count;
    int cursor_x, cursor_y;
    int scroll_y;

    vfs_node* open_file_node; // Jeśli otwarty z pliku, trzymamy wskaźnik do zapisu przy zamknięciu
public:
    NotepadWindow(int x, int y, vfs_node* file = nullptr);
    void Draw() override;
    void OnKeyboard(char c) override;
};

// 3. File Manager (Pro Explorer)
class FileManagerWindow : public Window {
private:
    vfs_node* current_path;
    int selected_idx;
    int hover_idx;
public:
    FileManagerWindow(int x, int y);
    void Draw() override;
    void OnMouseMove(int rel_x, int rel_y) override;
    void OnMouseDown(int rel_x, int rel_y) override;
    void OpenFile(vfs_node* node);
};

// 4. Video Player (Streaming)
class VideoPlayerWindow : public Window {
private:
    uint8_t* video_ram;
    int frame_width, frame_height, total_frames, current_frame;
    uint8_t* delta_buffer; // Bufor na streaming
    uint32_t file_offset;     
    vfs_node* video_file;     
public:
    VideoPlayerWindow(int x, int y, const char* filename);
    ~VideoPlayerWindow();
    void Draw() override;
};

// 5. Settings & Eyes (Drobne apki)
class SettingsWindow : public Window {
public:
    SettingsWindow(int x, int y);
    void Draw() override;
    void OnMouseDown(int rel_x, int rel_y) override;
};

class EyesWindow : public Window {
public:
    EyesWindow(int x, int y);
    void Draw() override;
};

// === MENEDŻER PULPITU ===
class Desktop {
private:
    Window* windows[MAX_WINDOWS];
    int window_count;
    Window* active_window;
    bool start_menu_open;
    bool start_button_hover;
    int drag_off_x, drag_off_y;

    int bar_x, bar_y, bar_w, bar_h;

public:
    Desktop();
    void Init();
    void AddWindow(Window* win);
    void RemoveWindow(Window* win);
    void BringToFront(Window* win);
    void Update(int mx, int my, bool left_click);
    void Draw(); 
    void HandleKeyboard(char c);

    // Helpers
    static void DrawFlatButton(int x, int y, int w, int h, const char* text, uint32_t bg_color, bool hover);
    static void DrawRoundedRect(int x, int y, int w, int h, int r, uint32_t color);
    static void DrawRoundedRectAlpha(int x, int y, int w, int h, int radius, uint32_t color, uint8_t alpha);

private:
    void DrawTaskbar();   
    void DrawLauncher(); 
    void DrawBackgroundGrid();
};