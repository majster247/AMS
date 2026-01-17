#include "vmm.h"
#include "kernel.h"

// Pomocnicza funkcja: pobiera wpis z tablicy, a jeśli nie istnieje - tworzy nową tablicę
uint64_t* get_next_table(uint64_t* table, uint64_t index) {
    if (table[index] & PAGE_PRESENT) {
        return (uint64_t*)(table[index] & ~0xFFFULL);
    } else {
        // Nie ma tablicy? Alokujemy ją przez PMM!
        uint64_t* new_table = (uint64_t*)pmm_alloc_frame();
        
        // Czyścimy nową tablicę (zero fill)
        for (int i = 0; i < 512; i++) new_table[i] = 0;

        // Wpisujemy nową tablicę do tablicy nadrzędnej
        table[index] = (uintptr_t)new_table | PAGE_PRESENT | PAGE_WRITABLE;
        return new_table;
    }
}

extern "C" void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);

    // Przechodzimy przez 4 poziomy
    uint64_t* pdpt = get_next_table(pml4, pml4_idx);
    uint64_t* pd   = get_next_table(pdpt, pdpt_idx);
    uint64_t* pt   = get_next_table(pd, pd_idx);

    // Na samym końcu wpisujemy fizyczny adres ramki
    pt[pt_idx] = (phys & ~0xFFFULL) | flags;

    // Odświeżamy TLB (Translation Lookaside Buffer), żeby procesor zauważył zmianę
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}