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
    void idt_init();
    void keyboard_init();
    
    int init_serial();
    void write_serial(char a);
    void write_serial_string(const char* str);
    const char* to_hex(uint64_t val);

    void parse_multiboot(uint64_t addr);

    void pmm_init(uint64_t mem_size, void* bitmap_address);
    void pmm_mark_free(uint64_t start, uint64_t size);
    void pmm_mark_used(uint64_t start, uint64_t size);
    void* pmm_alloc_frame();

    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);

    void* kmalloc(size_t size);

}