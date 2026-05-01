#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE      (1ULL << 7) // Dla 2MB stron, ustawiany w PD

#define PHYS_OFFSET 0xFFFF800000000000ULL

extern "C" {
    uint64_t get_cr3();
    void set_cr3(uint64_t cr3);
    
    // Podstawowe mapowanie
    void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
    
    // Rozszerzone mapowanie (dla konkretnego PML4)
    void vmm_map_page_ex(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags);
    
    // Pobieranie adresu fizycznego (obecny CR3)
    uint64_t vmm_get_phys(uint64_t virt);
    
    // DODAJ TO: Pobieranie adresu fizycznego z konkretnego PML4 (używane w sys_exec)
    uint64_t vmm_get_phys_ex(uint64_t pml4_phys, uint64_t virt);
    
    // Tworzenie nowej tablicy stron dla jądra (z Identity Map)
    uint64_t vmm_create_kernel_pml4();
    
    
    // Tworzenie nowej przestrzeni dla procesu
    uint64_t vmm_create_user_pml4();



    void vmm_map_mmio(uint64_t virt, uint64_t phys, size_t size);

    void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags);
    void vmm_map_huge_ex(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags);

    extern bool vmm_hhdm_ready; // Globalna flaga, która mówi, czy VMM powinien używać adresów fizycznych czy lustrzanych


    inline uint64_t* fix_ptr(uint64_t phys) {
        // Jeśli HHDM jest gotowe, używamy lustra. 
        // Jeśli nie, używamy adresu fizycznego (bo mamy Identity Map 1:1)
        if (vmm_hhdm_ready) {
            return (uint64_t*)(phys + 0xFFFF800000000000ULL);
        }
        return (uint64_t*)phys;
    }

}