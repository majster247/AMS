#include "kernel.h"
#include <stdint.h>

#define PAGE_SIZE 4096

static uint8_t* bitmap = nullptr;
static uint64_t total_frames = 0;
static uint64_t bitmap_size = 0;
static uint64_t used_frames = 0;

void pmm_memset(void* dest, int val, uint64_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    for(uint64_t i=0; i<len; i++) ptr[i] = (uint8_t)val;
}

extern "C" void pmm_init(uint64_t mem_size_mb, void* bitmap_addr) {
    total_frames = (mem_size_mb * 1024 * 1024) / PAGE_SIZE;
    bitmap = (uint8_t*)bitmap_addr;
    bitmap_size = total_frames / 8;

    // Domyślnie wszystko ZAJĘTE (bezpieczeństwo)
    pmm_memset(bitmap, 0xFF, bitmap_size);
    used_frames = total_frames;
    write_serial_string("[PMM] Zainicjalizowano. Domyslnie caly RAM zajety.\n");
}

extern "C" void pmm_mark_free(uint64_t start, uint64_t size) {
    uint64_t frame_start = start / PAGE_SIZE;
    uint64_t frame_count = size / PAGE_SIZE;

    for (uint64_t i = 0; i < frame_count; i++) {
        uint64_t f = frame_start + i;
        if (f >= total_frames) break;
        
        if (bitmap[f / 8] & (1 << (f % 8))) {
            bitmap[f / 8] &= ~(1 << (f % 8)); // 0 = wolne
            used_frames--;
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
            bitmap[f / 8] |= (1 << (f % 8)); // 1 = zajęte
            used_frames++;
        }
    }
}

extern "C" void* pmm_alloc_frame() {
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] != 0xFF) { 
            for (int j = 0; j < 8; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    uint64_t frame = i * 8 + j;
                    pmm_mark_used(frame * PAGE_SIZE, PAGE_SIZE);
                    return (void*)(frame * PAGE_SIZE);
                }
            }
        }
    }
    return nullptr; 
}

// --- NOWOŚĆ: Brakująca funkcja ---
extern "C" void pmm_free_frame(void* ptr) {
    if (!ptr) return;
    uint64_t addr = (uint64_t)ptr;
    // Po prostu oznaczamy ten adres (jedną stronę) jako wolny
    pmm_mark_free(addr, PAGE_SIZE);
}

extern "C" void pmm_mark_chunk_used(uint64_t start_addr, size_t size_bytes) {
    pmm_mark_used(start_addr, size_bytes);
}

extern "C" void* pmm_alloc_blocks(size_t block_count) {
    size_t size_bytes = block_count * PAGE_SIZE;
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] != 0xFF) { 
            for (int j = 0; j < 8; j++) {
                if (!(bitmap[i] & (1 << j))) {
                    uint64_t frame = i * 8 + j;
                    uint64_t addr = frame * PAGE_SIZE;

                    // Sprawdź, czy kolejne bloki są wolne
                    bool all_free = true;
                    for (size_t k = 0; k < block_count; k++) {
                        uint64_t next_frame = frame + k;
                        if (next_frame >= total_frames || (bitmap[next_frame / 8] & (1 << (next_frame % 8)))) {
                            all_free = false;
                            break;
                        }
                    }

                    if (all_free) {
                        // Oznacz wszystkie bloki jako zajęte
                        for (size_t k = 0; k < block_count; k++) {
                            uint64_t next_frame = frame + k;
                            bitmap[next_frame / 8] |= (1 << (next_frame % 8));
                            used_frames++;
                        }
                        return (void*)addr;
                    }
                }
            }
        }
    }
    return nullptr; 
}