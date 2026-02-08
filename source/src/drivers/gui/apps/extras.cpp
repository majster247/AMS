#include "gui.h"
#include "kernel.h"
#include "heap.h"

// Funkcje pomocnicze graficzne (dla Eyes)
int isqrt(int n) {
    if (n < 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}
void fill_circle(int x0, int y0, int r, uint32_t c) {
    for(int y=-r; y<=r; y++) for(int x=-r; x<=r; x++) if(x*x+y*y <= r*r) graphics_put_pixel_alpha(x0+x, y0+y, c, 255);
}

// === VIDEO PLAYER (Streaming) ===
VideoPlayerWindow::VideoPlayerWindow(int x, int y, const char* filename) 
    : Window(x, y, 640, 480, "AMS Media Stream") {
    
    extern vfs_node* ext2_root; 
    extern vfs_node* vfs_root; 
    
    // Szukaj wszędzie
    video_file = vfs_find_node(vfs_root, (char*)filename);
    if (!video_file) video_file = vfs_find_node(ext2_root, (char*)filename);

    if (video_file) {
        uint32_t head[4];
        video_file->read(video_file, 0, 16, (uint8_t*)head);
        
        frame_width = head[1]; frame_height = head[2]; total_frames = head[3];
        
        // Resize okna do wideo
        this->width = frame_width + 20;
        this->height = frame_height + TITLE_BAR_HEIGHT + 20;

        video_ram = (uint8_t*)kmalloc(frame_width * frame_height * 3);
        delta_buffer = (uint8_t*)kmalloc(1024 * 1024); // 1MB Streaming buffer
        
        memset(video_ram, 0, frame_width * frame_height * 3);
        file_offset = 16; current_frame = 0;
    }
}

VideoPlayerWindow::~VideoPlayerWindow() {
    if (video_ram) kfree(video_ram);
    if (delta_buffer) kfree(delta_buffer);
}

void VideoPlayerWindow::Draw() {
    if (minimized) return;
    Window::Draw();
    if (!video_file || !video_ram) {
        graphics_print(x+10, y+40, "No video file loaded or file not found.", COL_NORD11);
        return;
    }

    uint64_t current_time = get_system_ticks();
    static uint64_t last_frame_time = 0;
    
    if (current_time - last_frame_time >= 33) {
        uint32_t p_size;
        // 1. Czytaj rozmiar paczki
        video_file->read(video_file, file_offset, 4, (uint8_t*)&p_size);
        
        if (p_size > 1024*1024) { file_offset = 16; return; } // Bezpiecznik

        // 2. Czytaj dane (Streaming)
        video_file->read(video_file, file_offset + 4, p_size, delta_buffer);
        file_offset += (p_size + 4);

        uint32_t type = *(uint32_t*)delta_buffer;

        if (type == 0xFFFFFFFF) {
            memcpy(video_ram, delta_buffer + 4, frame_width * frame_height * 3);
        } else {
            uint32_t num = type;
            uint8_t* ptr = delta_buffer + 4;
            for (uint32_t j = 0; j < num; j++) {
                uint32_t idx = *(uint32_t*)ptr;
                if (idx < (uint32_t)(frame_width*frame_height)) {
                    video_ram[idx*3] = ptr[4];
                    video_ram[idx*3+1] = ptr[5];
                    video_ram[idx*3+2] = ptr[6];
                }
                ptr += 7;
            }
        }

        // Rysowanie do Backbuffera
        extern uint32_t* backbuffer; extern uint32_t fb_width;
        int cx = x + 10;
        int cy = y + TITLE_BAR_HEIGHT + 10;

        for (int ly = 0; ly < frame_height; ly++) {
            if (cy + ly >= 720) break;
            uint32_t* dst = &backbuffer[(cy + ly) * fb_width + cx];
            uint8_t* src = &video_ram[ly * frame_width * 3];
            for (int lx = 0; lx < frame_width; lx++) {
                dst[lx] = (0xFF << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
                src += 3;
            }
        }

        current_frame++;
        last_frame_time = current_time;
        if (current_frame >= total_frames) { file_offset = 16; current_frame = 0; }
    }
}

// === SETTINGS ===
SettingsWindow::SettingsWindow(int x, int y) : Window(x, y, 320, 240, "System Settings") {}
void SettingsWindow::Draw() {
    if (minimized) return;
    Window::Draw();
    int ty = y + TITLE_BAR_HEIGHT + 20;
    graphics_print(x + 20, ty, "Theme:", COL_NORD4);
    graphics_print(x + 100, ty, "Nord Dark (Active)", COL_NORD8);
    ty += 30;
    graphics_print(x + 20, ty, "Resolution:", COL_NORD4);
    graphics_print(x + 100, ty, "1280x720", COL_NORD8);
    
    // Przycisk "Apply"
    Desktop::DrawFlatButton(x + width - 100, y + height - 40, 80, 25, "Apply", COL_NORD10, false);
}
void SettingsWindow::OnMouseDown(int rel_x, int rel_y) {
    Window::OnMouseDown(rel_x, rel_y);
}

// === EYES ===
EyesWindow::EyesWindow(int x, int y) : Window(x, y, 200, 120, "xeyes") {}
void EyesWindow::Draw() {
    if (minimized) return;
    Window::Draw();
    extern int32_t mouse_x, mouse_y;
    int lx = x + 50, ly = y + 60;
    int rx = x + 150, ry = y + 60;
    
    auto DrawOne = [&](int ex, int ey) {
        fill_circle(ex, ey, 35, 0xFFFFFF); // Białko
        // Źrenica
        int dx = mouse_x - ex, dy = mouse_y - ey;
        int dist = isqrt(dx*dx + dy*dy);
        int max_d = 20;
        int px = ex, py = ey;
        if (dist > 0) {
            if (dist > max_d) { px = ex + dx*max_d/dist; py = ey + dy*max_d/dist; }
            else { px = ex + dx; py = ey + dy; }
        }
        fill_circle(px, py, 10, 0x000000);
    };
    DrawOne(lx, ly); DrawOne(rx, ry);
}