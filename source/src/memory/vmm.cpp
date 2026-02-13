#include "vmm.h"
#include "kernel.h"
#include "heap.h" 
#include "task.h"

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

            // ✅ Upewnij się, że new_table to czysty adres!
            uint64_t addr = (uint64_t)new_table & ~0xFFF; // Strip any flags

            table[index] = addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        } else {
            // Upewnij się, że istniejąca ścieżka też ma bit USER
            table[index] |= PAGE_USER;
        }

        // ✅ Zwracamy czysty adres (bez flag)
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
        if(!pdpt) {
            write_serial_string("[VMM] Failed to get PDPT!\n");
            return;
        }

        uint64_t* pd = get_next_table(pdpt, pdpt_idx, dir_flags);
        if(!pd) {
            write_serial_string("[VMM] Failed to get PD!\n");
            return;
        }

        uint64_t* pt = get_next_table(pd, pd_idx, dir_flags);
        if(!pt) {
            write_serial_string("[VMM] Failed to get PT!\n");
            return;
        }

        // ✅ Upewnij się, że phys NIE MA flag!
        if (phys & 0xFFF) {
            write_serial_string("[VMM] WARNING: Physical address has flags! phys=");
            write_serial_hex(phys);
            write_serial_string("\n");
            phys &= ~0xFFF; // Clear flags
        }

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
        
        // ✅ Zmień z 4GB na 16GB (żeby pokryć mmap_base)
        uint64_t limit = 0x400000000ULL; // 16GB zamiast 4GB
        uint64_t step = 0x200000;        // 2MB (huge pages)
        
        write_serial_string("[VMM] Start mapowania Identity (0-16GB) na Huge Pages...\n");
        
        for (uint64_t phys = 0; phys < limit; phys += step) {
            // ✅ Mapuj z PAGE_USER dla Ring 3
            vmm_map_huge(phys, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            
            // Kernel map (high half) bez USER
            vmm_map_huge(phys + PHYSICAL_MEM_OFFSET, phys, PAGE_PRESENT | PAGE_WRITABLE);
        }
        
        write_serial_string("[VMM] Mapowanie zakonczone. CR3 przeladowane.\n");
    }

    // Funkcja do translacji adresu wirtualnego na fizyczny
    uint64_t vmm_get_phys(uint64_t virt) {
        uint64_t* pml4 = (uint64_t*)get_cr3();
        uint64_t pml4_idx = (virt >> 39) & 0x1FF;
        uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
        uint64_t pd_idx   = (virt >> 21) & 0x1FF;
        uint64_t pt_idx   = (virt >> 12) & 0x1FF;
        uint64_t offset   = virt & 0xFFF;

        // Dostęp do PML4 przez kernel offset
        uint64_t* pml4_kernel = (uint64_t*)((uint64_t)pml4 + 0xFFFF800000000000ULL);

        if (!(pml4_kernel[pml4_idx] & PAGE_PRESENT)) {
            return 0; // Strona nie zamapowana
        }

        uint64_t* pdpt = (uint64_t*)((pml4_kernel[pml4_idx] & ~0xFFFULL) + 0xFFFF800000000000ULL);

        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
            return 0;
        }

        // Sprawdź czy to huge page (1GB)
        if (pdpt[pdpt_idx] & (1ULL << 7)) {
            uint64_t page_base = pdpt[pdpt_idx] & ~0x3FFFFFFFULL;
            return page_base + (virt & 0x3FFFFFFFULL);
        }

        uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFFULL) + 0xFFFF800000000000ULL);

        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            return 0;
        }

        // Sprawdź czy to huge page (2MB)
        if (pd[pd_idx] & (1ULL << 7)) {
            uint64_t page_base = pd[pd_idx] & ~0x1FFFFFULL;
            return page_base + (virt & 0x1FFFFFULL);
        }

        uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFFULL) + 0xFFFF800000000000ULL);

        if (!(pt[pt_idx] & PAGE_PRESENT)) {
            return 0;
        }

        uint64_t page_base = pt[pt_idx] & ~0xFFFULL;
        return page_base + offset;
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


    // Znajduje wolny obszar wirtualny i mapuje tam fizyczne ramki
    // size - rozmiar w bajtach (musi być wyrównany do 4096)
    // flags - np. PAGE_USER | PAGE_WRITABLE
    uint64_t vmm_allocate_region(uint64_t size, uint64_t flags) {
        if (!current_task) return 0;

        // 1. Pobierz aktualny "wierzchołek" pamięci procesu
        uint64_t start_addr = current_task->virt_memory_top;

        // 2. Oblicz ile stron potrzebujemy
        uint64_t pages = (size + 4095) / 4096;

        // 3. Dla każdej strony:
        for (uint64_t i = 0; i < pages; i++) {
            uint64_t phys = (uint64_t)pmm_alloc_frame();
            if (!phys) {
                // OOM (Out Of Memory)! W prawdziwym OS tutaj robimy cleanup.
                return 0; 
            }

            // Mapujemy (virt -> phys)
            vmm_map(start_addr + (i * 4096), phys, flags | PAGE_PRESENT | PAGE_USER);

            // Zerujemy pamięć (ważne dla bezpieczeństwa!)
            memset((void*)(start_addr + (i * 4096)), 0, 4096);
        }

        // 4. Przesuwamy wierzchołek
        current_task->virt_memory_top += pages * 4096;

        return start_addr;
    }
}


// vmm_create_user_pml4() tworzy nową tablicę PML4 dla procesu użytkownika, kopiując wpisy z aktualnego PML4 (kernelowego) i ustawiając bit USER dla tych wpisów, które są obecne. Dzięki temu proces użytkownika ma dostęp do tych samych zasobów co kernel, ale z ograniczeniami wynikającymi z bitu USER.
uint64_t vmm_create_user_pml4() {
    uint64_t* new_pml4 = (uint64_t*)pmm_alloc_frame();
    if (!new_pml4) return 0;

    // Pobierz aktualny PML4 (kernelowy)
    uint64_t* current_pml4 = (uint64_t*)get_cr3();

    // Skopiuj wpisy z aktualnego PML4, ustawiając bit USER
    for (int i = 0; i < 512; i++) {
        if (current_pml4[i] & PAGE_PRESENT) {
            // Ustaw bit USER dla wszystkich obecnych wpisów
            new_pml4[i] = current_pml4[i] | PAGE_USER;
        }
    }

    return (uint64_t)new_pml4;
}