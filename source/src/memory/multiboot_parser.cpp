#include "kernel.h"
#include "graphics.h"
#include <stdint.h>

// Globalna zmienna, w której zapiszemy adres naszego systemu plików
uint64_t initrd_addr = 0;

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
};

// --- NOWA STRUKTURA DLA MODUŁÓW (INITRD) ---
struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start; // Adres fizyczny początku modułu
    uint32_t mod_end;   // Adres fizyczny końca modułu
    char cmdline[];     // Opcjonalna linia komend
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
};

//ram size z multiboota
uint64_t ram_size_mb = 0;


extern "C" void pmm_mark_free(uint64_t start, uint64_t size);

extern "C" void parse_multiboot(uint64_t addr) {
    write_serial_string("[MEM] Parsing Multiboot2 info...\n");

    uint32_t total_size = *(uint32_t*)addr;
    uint8_t* tag_ptr = (uint8_t*)(addr + 8);

    while (tag_ptr < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;

        if (tag->type == 0) break;

        // TAG TYPU 3: Moduł (nasz Initrd)
        if (tag->type == 3) {
            multiboot_tag_module* mod = (multiboot_tag_module*)tag;
            write_serial_string("[VFS] Znaleziono modul pod adresem: ");
            write_serial_hex(mod->mod_start);
            write_serial_string("\n");
            
            initrd_addr = mod->mod_start;
        }

        // TAG TYPU 6: Mapa pamięci
        if (tag->type == 6) {
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;

            for (uint32_t i = 0; i < num_entries; i++) {
                multiboot_mmap_entry* entry = (multiboot_mmap_entry*)((uint8_t*)mmap->entries + (i * mmap->entry_size));
                
                if (entry->type == 1) { // AVAILABLE
                    uint64_t start = entry->addr;
                    uint64_t length = entry->len;

                    if (start < 0x100000) {
                        uint64_t end = start + length;
                        if (end <= 0x100000) continue;
                        start = 0x100000;
                        length = end - 0x100000;
                    }
                    pmm_mark_free(start, length);
                }
            }
        }

        //Tag Typu 8: Framebuffer
         if (tag->type == 8) {
            write_serial_string("[MEM] Znaleziono tag framebuffera.\n");
            multiboot_tag_framebuffer* fb_tag = (multiboot_tag_framebuffer*)tag_ptr;
            fb.address = fb_tag->framebuffer_addr;
            fb.width = fb_tag->framebuffer_width;
            fb.height = fb_tag->framebuffer_height;
            fb.pitch = fb_tag->framebuffer_pitch;
            

            //logowanie info o framebufferze
            write_serial_string("      -> Adres: ");
            write_serial_hex(fb.address);
            write_serial_string("\n");
            write_serial_string(" Pitch: ");
            write_serial_dec(fb.pitch);
            write_serial_string("\n");
            write_serial_string(" BPP: ");
            write_serial_dec(fb_tag->framebuffer_bpp);
            write_serial_string("\n");
            write_serial_string(" Type: ");
            write_serial_dec(fb_tag->framebuffer_type);
            write_serial_string("\n");
            write_serial_string(" Rozdzielczosc: ");
            write_serial_dec(fb.width);
            write_serial_string("x");
            write_serial_dec(fb.height);
            write_serial_string("\n");
            break;

        }

        tag_ptr += (tag->size + 7) & ~7;
    }

    write_serial_string("[MEM] Parsing finished.\n");
    // Po parsingu zapisz ile RAM mamy
    uint64_t total_bytes = 0; // Używamy bajtów dla precyzji
    tag_ptr = (uint8_t*)(addr + 8);
    while (tag_ptr < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;
        if (tag->type == 0) break;

        if (tag->type == 6) {
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;

            for (uint32_t i = 0; i < num_entries; i++) {
                multiboot_mmap_entry* entry = (multiboot_mmap_entry*)((uint8_t*)mmap->entries + (i * mmap->entry_size));
                if (entry->type == 1) { // AVAILABLE
                    total_bytes += entry->len; // SUMUJEMY długość bloku
                }
            }
        }
        tag_ptr += (tag->size + 7) & ~7;
    }
    ram_size_mb = total_bytes / (1024 * 1024);
}