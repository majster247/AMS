#include "kernel.h"
#include <stdint.h>

#define PAGE_SIZE 4096

static uint8_t* bitmap = nullptr;
static uint64_t total_frames = 0;
static uint64_t bitmap_size = 0;
static uint64_t used_frames = 0;
static uint64_t last_free_frame = 0; // Optymalizacja: szukaj od ostatniego wolnego miejsca

extern "C" void write_serial_string(const char* str);
extern "C" void write_serial_hex(uint64_t value);

extern "C" void pmm_init(uint64_t mem_size_mb, void* bitmap_addr) {
    total_frames = (mem_size_mb * 1024 * 1024) / PAGE_SIZE;
    bitmap = (uint8_t*)bitmap_addr;
    bitmap_size = total_frames / 8;

    // Szybki memset (blokujemy wszystko na start)
    for(uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    
    used_frames = total_frames;
    last_free_frame = 0;

    write_serial_string("[PMM] Init: Bitmap at ");
    write_serial_hex((uint64_t)bitmap);
    write_serial_string(" for ");
    write_serial_hex(mem_size_mb);
    write_serial_string(" MB RAM\n");
}

extern "C" void pmm_mark_free(uint64_t start, uint64_t size) {
    uint64_t frame_start = start / PAGE_SIZE;
    uint64_t frame_count = size / PAGE_SIZE;

    for (uint64_t i = 0; i < frame_count; i++) {
        uint64_t f = frame_start + i;
        if (f >= total_frames) break;
        
        if (bitmap[f / 8] & (1 << (f % 8))) {
            bitmap[f / 8] &= ~(1 << (f % 8));
            used_frames--;
            if (f < last_free_frame) last_free_frame = f; // Cofnij kursor szukania
        }
    }
}

extern "C" void pmm_mark_used(uint64_t start, uint64_t size) {
    uint64_t frame_start = start / PAGE_SIZE;
    uint64_t frame_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < frame_count; i++) {
        uint64_t f = frame_start + i;
        if (f >= total_frames) break;

        if (!(bitmap[f / 8] & (1 << (f % 8)))) {
            bitmap[f / 8] |= (1 << (f % 8));
            used_frames++;
        }
    }
}

extern "C" void* pmm_alloc_frame() {
    // Szukamy od last_free_frame (drastyczne przyspieszenie)
    for (uint64_t f = last_free_frame; f < total_frames; f++) {
        if (!(bitmap[f / 8] & (1 << (f % 8)))) {
            bitmap[f / 8] |= (1 << (f % 8));
            used_frames++;
            last_free_frame = f + 1;
            return (void*)(f * PAGE_SIZE);
        }
    }
    return nullptr; 
}

extern "C" void pmm_free_frame(void* ptr) {
    if (!ptr) return;
    pmm_mark_free((uint64_t)ptr, PAGE_SIZE);
}

extern "C" bool pmm_is_free(uint64_t addr) {
    uint64_t frame = addr / PAGE_SIZE;
    if (frame >= total_frames) return false; // Poza zakresem
    return !(bitmap[frame / 8] & (1 << (frame % 8)));
}

extern "C" uint32_t pmm_get_free_memory_kb() {
    return (total_frames - used_frames) * (PAGE_SIZE / 1024);
}


extern "C" void pmm_dump_memory_map() {
    write_serial_string("\n--- AMS PHYSICAL MEMORY VISUALIZER ---\n");
    write_serial_string("Legend: [#] Occupied  [-] Free\n\n");

    uint64_t total_frames = total_ram_bytes / 4096;
    
    // Optymalizacja: jeden znak reprezentuje 4MB pamięci (1024 ramki)
    // Dzięki temu pasek dla 9GB będzie miał ~2300 znaków, co jest czytelne.
    const uint64_t frames_per_char = 1024; 
    uint64_t chars_in_line = 0;

    for (uint64_t i = 0; i < total_frames; i += frames_per_char) {
        bool cluster_occupied = false;

        // Sprawdzamy czy w danym bloku 4MB jest choć jedna zajęta ramka
        for (uint64_t j = 0; j < frames_per_char && (i + j) < total_frames; j++) {
            if (!pmm_is_free((i + j) * 4096)) {
                cluster_occupied = true;
                break;
            }
        }

        if (cluster_occupied) {
            write_serial_string("#");
        } else {
            write_serial_string("-");
        }

        chars_in_line++;
        if (chars_in_line >= 64) { // Nowa linia po 64 znakach (256MB na linię)
            write_serial_string("\n");
            chars_in_line = 0;
        }
    }

    write_serial_string("\n\n--- END OF MEMORY MAP ---\n");
    
    // Dodatkowe info liczbowe
    write_serial_string("Free RAM: ");
    write_serial_hex(pmm_get_free_memory_kb());
    write_serial_string(" KB\n");
}