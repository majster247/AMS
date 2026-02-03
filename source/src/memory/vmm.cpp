#include "vmm.h"
#include "kernel.h"


static inline uint64_t get_cr3() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & ~0xFFFULL;
}

static inline uint64_t table_entry_phys(uint64_t entry) {
    return entry & ~0xFFF; // Zerujemy flagi (dolne 12 bitów), zostaje adres
}

uint64_t* get_next_table(uint64_t* table, uint64_t index) {
    if (table[index] & PAGE_PRESENT) {
        return (uint64_t*)(table[index] & ~0xFFFULL);
    } else {
        void* frame = pmm_alloc_frame();
        uint64_t* new_table = (uint64_t*)frame;
        
        // Czyścimy nową tablicę (zero fill)
        for (int i = 0; i < 512; i++) new_table[i] = 0;

        table[index] = (uint64_t)frame | PAGE_PRESENT | PAGE_WRITABLE;
        return new_table;
    }
}

extern "C" void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pml4 = (uint64_t*)get_cr3();
    
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_table(pml4, pml4_idx);
    uint64_t* pd   = get_next_table(pdpt, pdpt_idx);
    uint64_t* pt   = get_next_table(pd, pd_idx);

    pt[pt_idx] = phys | flags;
    
    // Inwalidacja wpisu w TLB (wymagane po mapowaniu)
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t vmm_get_phys(uint64_t virtual_addr) {
    uint64_t pml4_phys = get_cr3();
    uint64_t* pml4 = (uint64_t*)pml4_phys; 

    uint64_t pml4_idx = (virtual_addr >> 39) & 0x1FF;
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;

    // Wyciągamy adres fizyczny PDPT
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
    uint64_t pdpt_idx = (virtual_addr >> 30) & 0x1FF;
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;

    // Wyciągamy adres fizyczny PD
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    uint64_t pd_idx = (virtual_addr >> 21) & 0x1FF;
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;

    // Obsługa Huge Pages (2MB) - ważne, jeśli bootloader tak mapuje kernel
    if (pd[pd_idx] & (1 << 7)) {
        return (pd[pd_idx] & ~0x1FFFFFULL) + (virtual_addr & 0x1FFFFFULL);
    }

    // Wyciągamy adres fizyczny PT
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);
    uint64_t pt_idx = (virtual_addr >> 12) & 0x1FF;
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    return (pt[pt_idx] & ~0xFFFULL) + (virtual_addr & 0xFFFULL);
}

