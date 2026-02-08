#include "vmm.h"
#include "kernel.h"
#include "heap.h" 

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE     (1ULL << 7)

#define PHYSICAL_MEM_OFFSET 0xFFFF800000000000

extern "C" {
    // --- POPRAWKA: Usunięto 'static inline', teraz funkcja jest widoczna dla linkera ---
    uint64_t get_cr3() { 
        uint64_t cr3; 
        asm volatile("mov %%cr3, %0" : "=r"(cr3)); 
        return cr3 & ~0xFFFULL; 
    }

    // Te funkcje mogą zostać static, jeśli używamy ich tylko tutaj, 
    // ale dla bezpieczeństwa też można je upublicznić (helpery VMM).
    void set_cr3(uint64_t cr3) { 
        asm volatile("mov %0, %%cr3" :: "r"(cr3) : "memory"); 
    }
    
    void invlpg(uint64_t virt) { 
        asm volatile("invlpg (%0)" :: "r"(virt) : "memory"); 
    }

    // Alokator tablic (4KB)
    static uint64_t* alloc_table() {
        void* ptr = pmm_alloc_frame();
        if(!ptr) return nullptr;
        uint64_t* table = (uint64_t*)ptr;
        // Zerowanie tablicy
        for(int i=0; i<512; i++) table[i] = 0;
        return table;
    }

    // Pobiera następną tablicę, tworzy jeśli brak.
    uint64_t* get_next_table(uint64_t* table, uint64_t index, uint64_t flags) {
        if (!(table[index] & PAGE_PRESENT)) {
            uint64_t* new_table = alloc_table();
            if (!new_table) return nullptr;
            // Wpis w katalogu dziedziczy flagi
            table[index] = (uint64_t)new_table | PAGE_PRESENT | PAGE_WRITABLE | flags;
        } else {
            if (flags & PAGE_USER) table[index] |= PAGE_USER;
            if (flags & PAGE_WRITABLE) table[index] |= PAGE_WRITABLE;
        }
        
        return (uint64_t*)(table[index] & ~0xFFFULL);
    }

    // --- MAPOWANIE 4KB ---
    void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;
        uint64_t pt_idx   = (virt >> 12) & 0x1FF;

        uint64_t dir_flags = flags & (PAGE_USER | PAGE_WRITABLE);

        uint64_t* pdpt = get_next_table(pml4, pml4_idx, dir_flags);
        if(!pdpt) return;

        uint64_t* pd = get_next_table(pdpt, pdpt_idx, dir_flags);
        if(!pd) return;

        uint64_t* pt = get_next_table(pd, pd_idx, dir_flags);
        if(!pt) return;

        pt[pt_idx] = phys | flags | PAGE_PRESENT;
        invlpg(virt);
    }

    // --- MAPOWANIE HUGE (2MB) ---
    void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;

        uint64_t dir_flags = flags & (PAGE_USER | PAGE_WRITABLE);

        uint64_t* pdpt = get_next_table(pml4, pml4_idx, dir_flags);
        uint64_t* pd   = get_next_table(pdpt, pdpt_idx, dir_flags);
        
        pd[pd_idx] = phys | flags | PAGE_HUGE | PAGE_PRESENT;
        invlpg(virt);
    }

    void vmm_map_user(uint64_t virt, uint64_t phys, bool writable) {
        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if (writable) flags |= PAGE_WRITABLE;
        
        vmm_map_page(virt, phys, flags);
    }

    void vmm_init_direct_map(uint64_t ram_size_mb) {
        (void)ram_size_mb;
        uint64_t limit = 0x100000000ULL; // 4GB
        uint64_t step = 0x200000;       // 2MB

        write_serial_string("[VMM] Start mapowania Identity (0-4GB) na Huge Pages...\n");

        for (uint64_t phys = 0; phys < limit; phys += step) {
            vmm_map_huge(phys, phys, PAGE_PRESENT | PAGE_WRITABLE);
            vmm_map_huge(phys + PHYSICAL_MEM_OFFSET, phys, PAGE_PRESENT | PAGE_WRITABLE);
        }
        
        write_serial_string("[VMM] Mapowanie zakonczone. CR3 przeladowane.\n");
    }

    uint64_t vmm_get_phys(uint64_t virt) {
        if (virt >= PHYSICAL_MEM_OFFSET) return virt - PHYSICAL_MEM_OFFSET;
        return virt;
    }
    
    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
        vmm_map_page(virt, phys, flags);
    }
    
    void vmm_set_nocache(uint64_t virt) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;
        
        uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
        uint64_t* pd   = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
        
        pd[pd_idx] |= (1<<4) | (1<<3);
        invlpg(virt);
    }
}