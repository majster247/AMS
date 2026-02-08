#include "kernel.h"

// Zmienne statyczne
static uint64_t heap_start = 0;
static uint64_t heap_current = 0;
static uint64_t heap_max = 0;

extern "C" void pmm_mark_used(uint64_t start, uint64_t size);

// Inicjalizacja: Kernel podaje bezpieczny adres (za initrd)
extern "C" void heap_init(void* addr, uint64_t size) {
    heap_start = (uint64_t)addr;
    // Wyrównanie do 2MB (dla porządku z VMM)
    if (heap_start & 0x1FFFFF) {
        heap_start = (heap_start + 0x1FFFFF) & ~0x1FFFFFULL;
    }
    
    heap_current = heap_start;
    heap_max = heap_start + size;

    write_serial_string("[HEAP] Start (Identity): ");
    write_serial_hex(heap_start);
    write_serial_string("\n");
}

extern "C" void* kmalloc(size_t size) {
    if (size == 0) return nullptr;

    // Wyrównanie do 16 bajtów (dla SSE/AVX - ważne dla Minecrafta/Wideo!)
    size = (size + 15) & ~15;

    // Sprawdzenie limitu
    if (heap_current + size > heap_max) {
        write_serial_string("[HEAP] OOM! Brak pamieci na stercie!\n");
        return nullptr;
    }

    void* ptr = (void*)heap_current;

    // Zgłaszamy zajętość do PMM
    // To zapobiega sytuacji, gdzie pmm_alloc_frame() zwróciłby ten sam RAM.
    pmm_mark_used(heap_current, size);

    heap_current += size;
    return ptr;
}

extern "C" void kfree(void* ptr) {
    (void)ptr;
    // W Bump Allocatorze nie zwalniamy. 
    // Do gier/Shell to wystarczy. Prawdziwe free wymagałoby listy wolnych bloków.
}

// Wrappery C++
extern "C" void* malloc(size_t size) { return kmalloc(size); }
extern "C" void free(void* ptr) { kfree(ptr); }

void* operator new(size_t size) { return kmalloc(size); }
void* operator new[](size_t size) { return kmalloc(size); }
void operator delete(void* p) { kfree(p); }
void operator delete[](void* p) { kfree(p); }
void operator delete(void* p, size_t s) { (void)s; kfree(p); }
void operator delete[](void* p, size_t s) { (void)s; kfree(p); }