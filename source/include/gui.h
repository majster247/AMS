/**
 * @file gui.h
 * @author Majster
 * @brief System okienkowy AMS-OS.
 */

#pragma once
#include <stdint.h>
#include "graphics.h"
#include "vfs.h"
#include "ext2.h"

#define MAX_WINDOWS 32
#define TASKBAR_HEIGHT 45
#define TITLE_BAR_HEIGHT 28

// === NORD THEME COLORS ===
#define COL_NORD0  0x2E3440
#define COL_NORD1  0x3B4252
#define COL_NORD2  0x434C5E
#define COL_NORD3  0x4C566A
#define COL_NORD4  0xD8DEE9
#define COL_NORD5  0xE5E9F0
#define COL_NORD6  0xE5E9F0
#define COL_NORD7  0xECEFF4
#define COL_NORD8  0x88C0D0
#define COL_NORD9  0x81A1C1
#define COL_NORD10 0x5E81AC
#define COL_NORD11 0xBF616A
#define COL_NORD12 0xD08770
#define COL_NORD13 0xEBCB8B
#define COL_NORD14 0xA3BE8C
#define COL_NORD15 0xB48EAD

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
    virtual void HandleInput(char c){}
};

// --- Deklaracje apek (klasy pochodne) ---
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
    void HandleInput(char c) override;
};

class NotepadWindow : public Window {
private:
    char text_buffer[100][80];
    int lines_count, cursor_x, cursor_y, scroll_y;
    vfs_node* open_file_node;
public:
    NotepadWindow(int x, int y, vfs_node* file = nullptr);
    void Draw() override;
    void OnKeyboard(char c) override;
};

class FileManagerWindow : public Window {
private:
    vfs_node* current_path;
    int selected_idx, hover_idx;
public:
    FileManagerWindow(int x, int y);
    void Draw() override;
    void OnMouseMove(int rel_x, int rel_y) override;
    void OnMouseDown(int rel_x, int rel_y) override;
    void OpenFile(vfs_node* node);
};

class VideoPlayerWindow : public Window {
public:
    uint8_t* video_ram;
    int frame_width, frame_height, total_frames, current_frame;
    uint8_t* delta_buffer; 
    uint32_t file_offset;     
    vfs_node* video_file;     
    VideoPlayerWindow(int x, int y, const char* filename);
    ~VideoPlayerWindow();
    void Draw() override;
};

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

    // Te pola są używane w gui_core.cpp w Desktop::Desktop()
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

    static void DrawFlatButton(int x, int y, int w, int h, const char* text, uint32_t bg_color, bool hover);
    static void DrawRoundedRect(int x, int y, int w, int h, int r, uint32_t color);
    
    // Ta deklaracja musi pasować do definicji w gui_core.cpp:68
    static void DrawRoundedRectAlpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
    
    void DrawClock(uint32_t x, uint32_t y);

    // Te metody muszą być widoczne dla Desktop::Draw()
    void DrawBackgroundGrid();
    void DrawTaskbar();
    void DrawLauncher();
};