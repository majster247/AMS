#include "../include/kernel.h" // Upewnij się, że ścieżki include są poprawne
#include <stdint.h>

// Struktury Multiboot2 (uproszczone dla czytelności)
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


extern "C" void parse_multiboot(uint64_t addr) {
    write_serial_string("[MEM] Parsing Multiboot2 info...\n");

    // Pierwsze 8 bajtów to całkowity rozmiar struktury
    uint32_t total_size = *(uint32_t*)addr;
    
    // Wskaźnik na pierwszy tag (pomijamy 8 bajtów nagłówka)
    uint8_t* tag_ptr = (uint8_t*)(addr + 8);

    while (tag_ptr < (uint8_t*)(addr + total_size)) {
        multiboot_tag* tag = (multiboot_tag*)tag_ptr;

        if (tag->type == 0) { // Tag kończący
            break;
        }

        if (tag->type == 6) { // Tag Mapy Pamięci (Memory Map)
            write_serial_string("[MEM] Found Memory Map:\n");
            
            multiboot_tag_mmap* mmap = (multiboot_tag_mmap*)tag;
            uint32_t num_entries = (mmap->size - 16) / mmap->entry_size;

            for (uint32_t i = 0; i < num_entries; i++) {
                multiboot_mmap_entry* entry = (multiboot_mmap_entry*)((uint64_t)mmap->entries + (i * mmap->entry_size));
                
                // Wypisz szczegóły każdego regionu
                write_serial_string("   Region: Start=");
                write_serial_string(to_hex(entry->addr));
                write_serial_string(" Len=");
                write_serial_string(to_hex(entry->len));
                write_serial_string(" Type=");
                
                if (entry->type == 1) { // AVAILABLE
                    write_serial_string("AVAILABLE\n");
                    pmm_mark_free(entry->addr, entry->len);
                } else {
                    write_serial_string("RESERVED\n");
                }
            }
        }

        // Przejdź do następnego taga (wyrównanie do 8 bajtów)
        tag_ptr += ((tag->size + 7) & ~7);
    }
    write_serial_string("[MEM] Parsing finished.\n");
}