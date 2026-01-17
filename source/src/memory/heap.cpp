#include "vmm.h"
#include "kernel.h"

static uint64_t heap_start = 0x10000000; // 256MB wirtualnie
static uint64_t heap_current = heap_start;

extern "C" void* kmalloc(size_t size) {
    // 1. Wyrównujemy do 8 bajtów dla wydajności
    size = (size + 7) & ~7;

    // 2. Sprawdzamy, czy potrzebujemy zmapować nową stronę dla tego żądania
    // (Uproszczenie: mapujemy nową stronę jeśli przekroczymy aktualną)
    if ((heap_current & 0xFFF) + size > 0x1000 || heap_current == heap_start) {
        void* phys = pmm_alloc_frame();
        vmm_map(heap_current & ~0xFFFULL, (uint64_t)phys, PAGE_PRESENT | PAGE_WRITABLE);
    }

    void* ptr = (void*)heap_current;
    heap_current += size;
    return ptr;
}