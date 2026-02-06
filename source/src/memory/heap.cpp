#include "vmm.h"
#include "kernel.h"

static uint64_t heap_start = 0; 
static uint64_t heap_current = heap_start;

extern "C" void heap_init(void* addr, uint64_t size) {
    heap_start = (uint64_t)addr;
    heap_current = heap_start;
    // Opcjonalnie: możesz tu od razu zmapować kilka stron, 
    // żeby kmalloc nie musiał robić wszystkiego na raz.
    //Mapuj pierwszą stronę
    vmm_map(heap_start, heap_start, PAGE_PRESENT | PAGE_WRITABLE);

    write_serial_string("[HEAP] Zainicjalizowano na adresie: ");
    write_serial_hex(heap_current);
    write_serial_string("\n");
}


extern "C" void* kmalloc(size_t size) {
    size = (size + 7) & ~7;

    // Jeśli alokacja nie mieści się w obecnej stronie, przejdź na początek następnej
    if ((heap_current & 0xFFF) + size > 0x1000) {
        heap_current = (heap_current + 0xFFF) & ~0xFFFULL;
    }

    // Mapuj strony tak długo, aż całe żądanie 'size' będzie pokryte
    uint64_t temp_addr = heap_current & ~0xFFFULL;
    while (temp_addr < (heap_current + size)) {
        // Tu powinieneś sprawdzić, czy strona jest już zmapowana (get_page_entry)
        // Ale na szybko:
        void* phys = pmm_alloc_frame();
        vmm_map(temp_addr, (uint64_t)phys, PAGE_PRESENT | PAGE_WRITABLE);
        temp_addr += 0x1000;
    }

    void* ptr = (void*)heap_current;
    heap_current += size;
    return ptr;
}

extern "C" void kfree(void* ptr) {
    // Na razie nic nie robimy - pancerna warstwa nie potrzebuje 
    // odzyskiwać pamięci w fazie bootowania.
    (void)ptr; 
}

//mallok i free są bardzo proste, ale nie obsługują zwalniania pamięci ani ponownego używania.
extern "C" void* malloc(size_t size) {
    return kmalloc(size);
}

extern "C" void free(void* ptr) {
    kfree(ptr);
}

void* operator new(size_t size) {
    extern void* kmalloc(size_t size);
    extern void write_serial_string(const char*); // Or whatever your debug function is
    
    void* ptr = kmalloc(size);
    if (ptr == nullptr) {
        write_serial_string("PANIC: operator new returned NULL! Heap full or broken.\n");
        while(1) asm volatile("hlt");
    }
    return ptr;
}

void* operator new[](size_t size) {
    extern void* kmalloc(size_t size);
    return kmalloc(size);
}

// Zwykły delete
void operator delete(void* p) {
    // kfree(p); // Odkomentuj jak zaimplementujesz kfree w heap.cpp
}

// Zwykły delete []
void operator delete[](void* p) {
    // kfree(p);
}

// === TO JEST TO, CZEGO BRAKUJE (Sized Delete) ===
void operator delete(void* p, size_t size) {
    (void)size; // Nie używamy rozmiaru, uciszamy warning
    // kfree(p); 
}

void operator delete[](void* p, size_t size) {
    (void)size;
    // kfree(p);
}