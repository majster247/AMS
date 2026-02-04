#include "vmm.h"
#include "kernel.h"

#define PHYSICAL_MEM_OFFSET 0xFFFF800000000000
#define PAGE_HUGE (1 << 7)

// Wszystko co ma być widoczne dla reszty jądra (C++) dajemy w extern "C"
extern "C" {

    static inline uint64_t get_cr3() {
        uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        return cr3 & ~0xFFFULL;
    }

    // Twoja pomocnicza funkcja
    static inline uint64_t* get_table_ptr(uint64_t phys) {
        if (phys < 0x4000000) return (uint64_t*)phys; 
        return (uint64_t*)(phys + PHYSICAL_MEM_OFFSET);
    }

    uint64_t* get_next_table(uint64_t* table, uint64_t index) {
        if (table[index] & PAGE_PRESENT) {
            return get_table_ptr(table[index] & ~0xFFFULL);
        } else {
            void* frame = pmm_alloc_frame();
            uint64_t* new_table = get_table_ptr((uint64_t)frame);
            for (int i = 0; i < 512; i++) new_table[i] = 0;
            table[index] = (uint64_t)frame | PAGE_PRESENT | PAGE_WRITABLE;
            return new_table;
        }
    }

    // TO JEST TO CZEGO SZUKA KERNEL I AHCI
    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;
        uint64_t pt_idx   = (virt >> 12) & 0x1FF;

        uint64_t* pdpt = get_next_table(pml4, pml4_idx);
        uint64_t* pd   = get_next_table(pdpt, pdpt_idx);
        uint64_t* pt   = get_next_table(pd, pd_idx);

        pt[pt_idx] = phys | flags;
        asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    }

    void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;

        uint64_t* pdpt = get_next_table(pml4, pml4_idx);
        uint64_t* pd   = get_next_table(pdpt, pdpt_idx);

        pd[pd_idx] = phys | flags | PAGE_HUGE;
        asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    }

    // TEGO SZUKA AHCI
    uint64_t vmm_get_phys(uint64_t virtual_addr) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        // Podczas initu (niskie adresy) nie dodawaj offsetu do pml4
        if ((uint64_t)pml4 < 0x4000000) { /* ok */ } 
        else { pml4 = (uint64_t*)((uint64_t)pml4 + PHYSICAL_MEM_OFFSET); }

        uint64_t pml4_idx = (virtual_addr >> 39) & 0x1FF;
        if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;

        uint64_t* pdpt = get_table_ptr(pml4[pml4_idx] & ~0xFFFULL);
        uint64_t pdpt_idx = (virtual_addr >> 30) & 0x1FF;
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;

        uint64_t* pd = get_table_ptr(pdpt[pdpt_idx] & ~0xFFFULL);
        uint64_t pd_idx = (virtual_addr >> 21) & 0x1FF;
        if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;

        // Jeśli to Huge Page (2MB)
        if (pd[pd_idx] & PAGE_HUGE) {
            return (pd[pd_idx] & ~0x1FFFFFULL) + (virtual_addr & 0x1FFFFFULL);
        }

        uint64_t* pt = get_table_ptr(pd[pd_idx] & ~0xFFFULL);
        uint64_t pt_idx = (virtual_addr >> 12) & 0x1FF;
        if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

        return (pt[pt_idx] & ~0xFFFULL) + (virtual_addr & 0xFFFULL);
    }

    void vmm_init_direct_map(uint64_t mem_size_mb) {
        uint64_t step = 0x200000; // 2MB Huge Page
        uint64_t mem_size_bytes = mem_size_mb * 1024 * 1024;

        for (uint64_t phys = 0; phys < mem_size_bytes; phys += step) {
            // Mapowanie Higher Half
            vmm_map_huge(phys + PHYSICAL_MEM_OFFSET, phys, PAGE_PRESENT | PAGE_WRITABLE);
            
            // Mapowanie Identity (pierwsze 32MB), żeby kernel nie zgasł
            if (phys < 0x2000000) {
                vmm_map_huge(phys, phys, PAGE_PRESENT | PAGE_WRITABLE);
            }
        }
    }

}