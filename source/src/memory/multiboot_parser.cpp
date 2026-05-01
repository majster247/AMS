#include "kernel.h"
#include "graphics.h"
#include <stdint.h>

// Zmienne systemowe
uint64_t total_ram_bytes = 0;
uint64_t initrd_addr = 0;
uint64_t initrd_end = 0;
uint64_t ram_size_mb = 0;



struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct {
        uint64_t addr;
        uint64_t len;
        uint32_t type;
        uint32_t zero;
    } entries[];
};

struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[];
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type_fb;
    uint16_t reserved;
};

extern "C" void pmm_mark_free(uint64_t start, uint64_t size);
extern "C" void pmm_mark_used(uint64_t start, uint64_t size);

// Funkcja pomocnicza do wyrównywania wskaźników (Linux-style alignment)
static inline uint8_t* align_to_8(uint8_t* ptr) {
    return (uint8_t*)(((uintptr_t)ptr + 7) & ~7);
}

extern "C" void parse_multiboot(uint64_t addr) {
    write_serial_string("[MEM] Linux-style Multiboot2 Parser starting...\n");

    if (addr & 7) {
        write_serial_string("[WARN] Multiboot address not 8-byte aligned! Fix bootloader.\n");
    }

    uint32_t total_size = *(uint32_t*)addr;
    uint8_t* end_ptr = (uint8_t*)(addr + total_size);
    uint8_t* tag_ptr = (uint8_t*)(addr + 8);

    // Faza 1: Liczenie pamięci (Discovery)
    // Linux najpierw musi wiedzieć "ile", zanim zacznie "rezerwować"
    while (tag_ptr < end_ptr) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;
        if (tag->type == 0) break; // Tag kończący

        if (tag->type == 6) { // Memory Map
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t entries = (mmap->size - 16) / mmap->entry_size;
            
            for (uint32_t i = 0; i < entries; i++) {
                uint64_t start = mmap->entries[i].addr;
                uint64_t len = mmap->entries[i].len;
                uint32_t type = mmap->entries[i].type;

                if (type == 1) { // USABLE
                    if (start + len > total_ram_bytes) {
                        total_ram_bytes = start + len;
                    }
                    // Dodaj do PMM tylko jeśli powyżej 1MB (Linux rezerwuje dół dla BIOSu)
                    if (start >= 0x100000) {
                        pmm_mark_free(start, len);
                    } else if (start + len > 0x100000) {
                        pmm_mark_free(0x100000, (start + len) - 0x100000);
                    }
                } else {
                    // Wszystko inne to Reserved/Hardware
                    pmm_mark_used(start, len);
                }
            }
        } 
        else if (tag->type == 4 && total_ram_bytes == 0) { // Fallback: Basic Mem Info
            multiboot_tag_basic_meminfo* mem = (multiboot_tag_basic_meminfo*)tag;
            total_ram_bytes = (uint64_t)(mem->mem_lower + mem->mem_upper) * 1024;
            pmm_mark_free(0x100000, total_ram_bytes - 0x100000);
        }

        tag_ptr = align_to_8(tag_ptr + tag->size);
    }

    ram_size_mb = total_ram_bytes / (1024 * 1024);
    write_serial_string("[MEM] System RAM: ");
    write_serial_dec(ram_size_mb);
    write_serial_string(" MB\n");

    // Faza 2: Krytyczne komponenty (Modules & Video)
    tag_ptr = (uint8_t*)(addr + 8);
    while (tag_ptr < end_ptr) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;
        if (tag->type == 0) break;

        switch (tag->type) {
            case 3: { // Initrd
                multiboot_tag_module* mod = (multiboot_tag_module*)tag;
                initrd_addr = mod->mod_start;
                initrd_end = mod->mod_end;
                pmm_mark_used(initrd_addr, initrd_end - initrd_addr);
                write_serial_string("[MEM] Ramdisk at: ");
                write_serial_hex(initrd_addr);
                write_serial_string("\n");
                break;
            }
            case 8: { // Framebuffer
                multiboot_tag_framebuffer* fb_tag = (multiboot_tag_framebuffer*)tag;
                fb.address = fb_tag->addr;
                fb.width = fb_tag->width;
                fb.height = fb_tag->height;
                fb.pitch = fb_tag->pitch;
                // VRAM jest "Used" - nie chcemy tam kłaść stosu procesów!
                pmm_mark_used(fb.address, (uint64_t)fb.height * fb.pitch);
                break;
            }
        }
        tag_ptr = align_to_8(tag_ptr + tag->size);
    }

    // Faza 3: "Nuclear Protection" (Kluczowe dla POSIX/Linux ABI)
    // Musimy chronić jądro przed samym sobą.
    
    // 1. Dół pamięci (0-1MB) - BIOS, tablice przerwań, Real Mode
    pmm_mark_used(0, 0x100000);

    // 2. Sam obraz jądra (zakładamy 1MB - 16MB dla bezpieczeństwa)
    pmm_mark_used(0x100000, 0xF00000);

    // 3. Struktura Multiboot - jeśli ją nadpiszemy, kernel spanikuje przy próbie odczytu cmdline
    pmm_mark_used(addr, total_size);

    write_serial_string("[MEM] Linux-compliant memory map finalized.\n");
}