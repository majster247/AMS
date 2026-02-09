#include <stdint.h>
#include "kernel.h"

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t common_addr;
    uint32_t common_pitch;
    uint32_t common_width;
    uint32_t common_height;
    uint8_t common_bpp;
    uint8_t common_type;
    uint16_t reserved;
};

uint32_t* fb_address;
uint32_t fb_width;
uint32_t fb_height;

void init_graphics(uint64_t addr) {
    // Przeszukujemy tagi Multiboot2 (pętla uproszczona)
    uint32_t total_size = *(uint32_t*)addr;
    uint8_t* tag_ptr = (uint8_t*)(addr + 8);

    while (tag_ptr < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;
        if (tag->type == 0) break; // End tag

        if (tag->type == 8) { // Tag Framebuffera
            multiboot_tag_framebuffer* fb = (multiboot_tag_framebuffer*)tag;
            fb_address = (uint32_t*)fb->common_addr;
            fb_width = fb->common_width;
            fb_height = fb->common_height;
        }
        tag_ptr += ((tag->size + 7) & ~7); // Wyrównanie do 8 bajtów
    }
}

// Funkcja rysująca piksel
void put_pixel(int x, int y, uint32_t color) {
    fb_address[y * fb_width + x] = color;
}