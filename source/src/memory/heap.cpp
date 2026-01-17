#include "vmm.h"
#include "kernel.h"

static uint64_t heap_start = 0x10000000; // 256MB wirtualnie
static uint64_t heap_current = heap_start;

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