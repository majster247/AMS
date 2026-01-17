#include "kernel.h"
#include <stdint.h>

#define PAGE_SIZE 4096
uint8_t* bitmap;
uint64_t total_frames;
uint64_t bitmap_size;

extern "C" void pmm_init(uint64_t mem_size, void* bitmap_address) {
    bitmap = (uint8_t*)bitmap_address;
    total_frames = mem_size / PAGE_SIZE;
    bitmap_size = total_frames / 8;

    // Domyślnie wszystko zajęte (1)
    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    write_serial_string("[PMM] Bitmapa zainicjalizowana jako ZAJETA.\n");
}

void pmm_mark_free(uint64_t start, uint64_t size) {
    uint64_t frame = start / PAGE_SIZE;
    uint64_t count = size / PAGE_SIZE;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t f = frame + i;
        bitmap[f / 8] &= ~(1 << (f % 8)); // Ustaw bit na 0 (wolne)
    }
}

void pmm_mark_used(uint64_t start, uint64_t size) {
    uint64_t frame = start / PAGE_SIZE;
    uint64_t count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t f = frame + i;
        bitmap[f / 8] |= (1 << (f % 8)); // Ustaw bit na 1 (zajęte)
    }
}
extern "C" void* pmm_alloc_frame() {
    // Przeszukujemy bitmapę
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] != 0xFF) { // Jeśli bajt nie jest w całości zajęty
            for (int j = 0; j < 8; j++) {
                uint8_t bit = 1 << j;
                if (!(bitmap[i] & bit)) { // Znaleźliśmy bit 0!
                    uint64_t frame_index = i * 8 + j;
                    pmm_mark_used(frame_index * 4096, 4096); // Zarezerwuj ją
                    return (void*)(frame_index * 4096);
                }
            }
        }
    }
    return nullptr; // Brak wolnej pamięci (Kernel Panic?)
}