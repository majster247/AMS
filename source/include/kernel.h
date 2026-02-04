#pragma once
#include <stddef.h>
#include <stdint.h>

extern "C" {

    // Flagi VMM
    #define PAGE_PRESENT  (1ULL << 0)
    #define PAGE_WRITABLE (1ULL << 1)
    #define PAGE_USER     (1ULL << 2)

    void terminal_initialize();
    void terminal_writestring(const char* data);
    void terminal_putchar(char c);
    void terminal_set_color(uint8_t fg, uint8_t bg);
    void terminal_set_cursor(int x, int y);
    void terminal_put_at(int x, int y, char c, uint8_t color);
    void terminal_clear();
    void terminal_writestring_at(const char* data, int x, int y);
    void terminal_write_dec(uint32_t val);
    void idt_init();
    void keyboard_init();
    uint8_t keyboard_get_char(); // Zwraca znak z klawiatury (blokujące)
    uint8_t keyboard_get_char_nonblocking(); // Zwraca znak z klawiatury (nieblokujące)
    
    int init_serial();
    void write_serial(char a);
    void write_serial_string(const char* str);
    void write_serial_hex(uint64_t val);
    void write_serial_char(char c);
    void write_serial_string(const char* str);
    void write_serial_dec(uint64_t val);
    bool serial_received();
    char serial_read();

    const char* to_hex(uint64_t val);

    void parse_multiboot(uint64_t addr);

    void pmm_init(uint64_t mem_size, void* bitmap_address);
    void pmm_mark_free(uint64_t start, uint64_t size);
    void pmm_mark_used(uint64_t start, uint64_t size);
    void* pmm_alloc_frame();

    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);

    void* kmalloc(size_t size);

    int strcmp(const char* s1, const char* s2);
    int strncmp(const char* s1, const char* s2, size_t n);
    
    void* memset(void* dest, int ch, size_t count);
    void* memcpy(void* dest, const void* src, size_t count);


    //time and random
    int rand();
    void srand(unsigned int seed);

    void get_time(int &h, int &m, int &s);

    //VGA COLORS

    #define VGA_COLOR_BLACK         0
    #define VGA_COLOR_BLUE          1
    #define VGA_COLOR_GREEN         2
    #define VGA_COLOR_CYAN          3
    #define VGA_COLOR_RED           4
    #define VGA_COLOR_MAGENTA       5
    #define VGA_COLOR_BROWN         6
    #define VGA_COLOR_LIGHT_GREY    7
    #define VGA_COLOR_DARK_GREY     8
    #define VGA_COLOR_LIGHT_BLUE    9
    #define VGA_COLOR_LIGHT_GREEN   10
    #define VGA_COLOR_LIGHT_CYAN    11
    #define VGA_COLOR_LIGHT_RED     12
    #define VGA_COLOR_LIGHT_MAGENTA 13
    #define VGA_COLOR_YELLOW        14
    #define VGA_COLOR_WHITE         15


   
}

