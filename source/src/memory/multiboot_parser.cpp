#include "kernel.h"
#include "graphics.h"
#include <stdint.h>

// Eksportujemy te zmienne, żeby Kernel wiedział gdzie ustawić Heap
uint64_t initrd_addr = 0;
uint64_t initrd_end = 0;
uint64_t kernel_end_symbol = 0; // Opcjonalnie, jeśli linker to podaje
uint64_t ram_size_mb = 0;

// Struktury Multiboot2 (Skrócone dla czytelności, zachowaj swoje definicje structów)
struct multiboot_tag { uint32_t type; uint32_t size; };
struct multiboot_mmap_entry { uint64_t addr; uint64_t len; uint32_t type; uint32_t zero; };
struct multiboot_tag_mmap { uint32_t type; uint32_t size; uint32_t entry_size; uint32_t entry_version; struct multiboot_mmap_entry entries[]; };
struct multiboot_tag_module { uint32_t type; uint32_t size; uint32_t mod_start; uint32_t mod_end; char cmdline[]; };
struct multiboot_tag_framebuffer { uint32_t type; uint32_t size; uint64_t addr; uint32_t pitch; uint32_t width; uint32_t height; uint8_t bpp; uint8_t type_fb; uint16_t reserved; };

extern "C" void pmm_mark_free(uint64_t start, uint64_t size);
extern "C" void pmm_mark_used(uint64_t start, uint64_t size);

extern "C" void parse_multiboot(uint64_t addr) {
    write_serial_string("[MEM] Parsing Multiboot2 info...\n");
    uint32_t total_size = *(uint32_t*)addr;
    uint8_t* tag_ptr = (uint8_t*)(addr + 8);

    // KROK 1: Skanowanie RAMu (MMAP) - Zwalniamy pamięć w PMM
    // Robimy to najpierw, żeby PMM wiedział co w ogóle istnieje.
    uint8_t* ptr_copy = tag_ptr;
    while (ptr_copy < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)ptr_copy;
        if (tag->type == 6) { // MMAP
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;
            for (uint32_t i = 0; i < num_entries; i++) {
                multiboot_mmap_entry* entry = (multiboot_mmap_entry*)((uint8_t*)mmap->entries + (i * mmap->entry_size));
                if (entry->type == 1) { // AVAILABLE
                    // Zabezpieczenie: Nie zwalniaj pierwszego 1MB (BIOS area)
                    uint64_t safe_start = entry->addr;
                    uint64_t safe_len = entry->len;
                    if (safe_start < 0x100000) {
                        uint64_t end = safe_start + safe_len;
                        if (end > 0x100000) {
                            safe_len = end - 0x100000;
                            safe_start = 0x100000;
                        } else { continue; }
                    }
                    pmm_mark_free(safe_start, safe_len);
                    // Prosta kalkulacja RAM
                    if ((safe_start + safe_len) > (ram_size_mb * 1024 * 1024)) {
                         ram_size_mb = (safe_start + safe_len) / (1024*1024);
                    }
                }
            }
        }
        if (tag->type == 0) break;
        ptr_copy += (tag->size + 7) & ~7;
    }

    // KROK 2: Oznaczanie zajętych obszarów (Moduły, Framebuffer, Kernel)
    // To jest kluczowe dla "Pancernego Kodu".
    while (tag_ptr < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;

        if (tag->type == 3) { // MODULE (Initrd)
            multiboot_tag_module* mod = (multiboot_tag_module*)tag;
            initrd_addr = mod->mod_start;
            initrd_end = mod->mod_end;
            
            // BLOKUJEMY INITRD W PMM!
            pmm_mark_used(mod->mod_start, mod->mod_end - mod->mod_start);
            
            write_serial_string("[MEM] Initrd (Film/TAR) ZABLOKOWANY: ");
            write_serial_hex(mod->mod_start);
            write_serial_string(" - ");
            write_serial_hex(mod->mod_end);
            write_serial_string("\n");
        }
        else if (tag->type == 8) { // FRAMEBUFFER
            multiboot_tag_framebuffer* fb_tag = (multiboot_tag_framebuffer*)tag;
            fb.address = fb_tag->addr;
            fb.width = fb_tag->width;
            fb.height = fb_tag->height;
            fb.pitch = fb_tag->pitch;
            // BLOKUJEMY FRAMEBUFFER (żeby Heap tam nie wlazł)
            pmm_mark_used(fb.address, fb.height * fb.pitch);
        }

        if (tag->type == 0) break;
        tag_ptr += (tag->size + 7) & ~7;
    }
    
    // KROK 3: Blokada Kernela (pierwsze 16MB na sztywno)
    pmm_mark_used(0, 0x1000000); 
    write_serial_string("[MEM] Kernel (0-16MB) ZABLOKOWANY.\n");
}