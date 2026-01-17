// src/kernel.cpp
extern "C" void kmain();

#include <stdint.h>
#include <stddef.h>

static constexpr uint16_t VGA_WIDTH = 80;
static constexpr uint16_t VGA_HEIGHT = 25;
static uint16_t* const VGA_BUFFER = reinterpret_cast<uint16_t*>(0xB8000);

enum class VGAColor : uint8_t { Black, Blue, Green, Cyan, Red, Magenta, Brown, LightGrey };

inline uint8_t vga_entry_color(VGAColor fg, VGAColor bg) { return uint8_t(fg) | (uint8_t(bg)<<4); }
inline uint16_t vga_entry(char c, uint8_t color) { return uint16_t(c) | (uint16_t(color)<<8); }

size_t terminal_row = 0;
size_t terminal_column = 0;
uint8_t terminal_color;
uint16_t* terminal_buffer;

extern "C" void terminal_initialize() {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGAColor::LightGrey, VGAColor::Black);
    terminal_buffer = VGA_BUFFER;
    for(size_t y=0;y<VGA_HEIGHT;y++)
        for(size_t x=0;x<VGA_WIDTH;x++)
            terminal_buffer[y*VGA_WIDTH+x] = vga_entry(' ', terminal_color);
}

extern "C" void terminal_putchar(char c) {
    if(c=='\n'){terminal_column=0; terminal_row++; return;}
    terminal_buffer[terminal_row*VGA_WIDTH+terminal_column] = vga_entry(c,terminal_color);
    terminal_column++;
    if(terminal_column>=VGA_WIDTH){terminal_column=0; terminal_row++;}
}

extern "C" void terminal_writestring(const char* str){
    for(size_t i=0;str[i];i++) terminal_putchar(str[i]);
}

extern "C" void kmain() {
    terminal_initialize();
    terminal_writestring("Hello, 64-bit kernel is alive!\n");
    while(1){ asm volatile("hlt"); }
}
