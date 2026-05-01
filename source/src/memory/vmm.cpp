#include "vmm.h"
#include "kernel.h"


extern "C" bool vmm_hhdm_ready = false; // Na początku nie jest gotowe, potem ustawimy na true w kernel.cpp

// Pomocnicza funkcja: zamienia adres fizyczny na wirtualny w "lustrze"
inline uint64_t phys_to_virt(uint64_t phys) {
    if (vmm_hhdm_ready) {
        return phys + 0xFFFF800000000000ULL;
    }
    // Jeśli nie gotowe, używamy adresu fizycznego (bo mamy Identity Map)
    return phys; 
}
static uint64_t* alloc_table() {
    uint64_t phys = (uint64_t)pmm_alloc_frame();
    if (!phys) return nullptr;

    // fix_ptr sam zdecyduje, czy użyć adresu fizycznego czy wirtualnego
    uint64_t* virt = fix_ptr(phys);

    // Zerujemy tablicę
    for (int i = 0; i < 512; i++) virt[i] = 0;

    return (uint64_t*)phys; // Zawsze zwracamy fizyczny do wpisania w strukturę
}

void vmm_map_page_ex(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    // PML4 jest adresem fizycznym, musimy go widzieć przez lustro
    uint64_t* pml4 = (uint64_t*)phys_to_virt(pml4_phys);

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pml4[pml4_idx] = (uint64_t)alloc_table() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        memset((void*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL), 0, 4096);
    }
    // Każdy kolejny poziom też musimy czytać przez phys_to_virt!
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pdpt[pdpt_idx] = (uint64_t)alloc_table() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        memset((void*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL), 0, 4096);
    }
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    // TUTAJ JEST PUŁAPKA: Jeśli pd[pd_idx] ma bit PAGE_HUGE, nie możemy tam wstawić PT!
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        pd[pd_idx] = (uint64_t)alloc_table() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        memset((void*)phys_to_virt(pd[pd_idx] & ~0xFFFULL), 0, 4096);
    } else if (pd[pd_idx] & PAGE_HUGE) {
        // Jeśli tu już jest Huge Page, to vmm_map_page (4KB) musi odpuścić, 
        // bo adres jest już zmapowany (często przy MMIO pod 4GB).
        return; 
    }
    
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = (phys & ~0xFFFULL) | flags;

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_map_page_ex(get_cr3(), virt, phys, flags);
}

uint64_t vmm_get_phys_ex(uint64_t pml4_phys, uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)phys_to_virt(pml4_phys);
    
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    if (pd[pd_idx] & PAGE_HUGE) {
        return (pd[pd_idx] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);
    }
    uint64_t* pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt[pt_idx] & ~0xFFFULL) | (virt & 0xFFF);
}

// vmm_get_phys po prostu używa obecnego CR3
uint64_t vmm_get_phys(uint64_t virt) {
    return vmm_get_phys_ex(get_cr3(), virt);
}


uint64_t get_cr3() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void set_cr3(uint64_t cr3) {
    asm volatile("mov %0, %%cr3" :: "r"(cr3));
}


void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_map_page(virt, phys, flags);
}

void vmm_unmap(uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)get_cr3();
    
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return;
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return;
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);

    if (!(pt[pt_idx] & PAGE_PRESENT)) return;
    
    pt[pt_idx] = 0; // Odznacz stronę jako nieobecną

    // Odśwież TLB dla tego adresu
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

extern "C" uint8_t stack_bottom;
extern "C" uint8_t stack_top;

uint64_t vmm_create_kernel_pml4() {
    uint64_t new_pml4 = (uint64_t)alloc_table();
    
    // 1. Mapujemy pierwsze 4GB (Identity i Higher Half)
    for (uint64_t i = 0; i < 0x100000000ULL; i += 0x200000) {
        vmm_map_huge_ex(new_pml4, i, i, PAGE_PRESENT | PAGE_WRITABLE);
        vmm_map_huge_ex(new_pml4, PHYS_OFFSET + i, i, PAGE_PRESENT | PAGE_WRITABLE);
    }

    // 2. KLUCZOWE: Mapujemy stos jądra, jeśli jest poza pierwszymi 4GB (na wszelki wypadek)
    // Ale ważniejsze: upewnij się, że stos, który masz w TSS (system_tss.rsp0), 
    // jest dostępny pod adresem, który tam wpisałeś.

        for (uint64_t addr = stack_bottom & ~0xFFFULL; addr < stack_top; addr += 0x200000) {
            vmm_map_huge_ex(new_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
            vmm_map_huge_ex(new_pml4, PHYS_OFFSET + addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
        }
    
    return new_pml4;
}


uint64_t vmm_create_user_pml4() {
    uint64_t phys_pml4 = (uint64_t)pmm_alloc_frame();
    if (!phys_pml4) return 0;

    uint64_t* new_pml4_virt = (uint64_t*)phys_to_virt(phys_pml4);
    uint64_t* current_pml4_virt = (uint64_t*)phys_to_virt(get_cr3());

    for (int i = 0; i < 512; i++) new_pml4_virt[i] = 0;

    // Kopiujemy jądro bez bitu USER
    for (int i = 256; i < 512; i++) {
        new_pml4_virt[i] = current_pml4_virt[i];
    }

    // 2. NOWE: Ratujemy kod jądra! (Identity Mapping dla pierwszych 8MB)
    // Mapujemy obszar 0x000000 do 0x800000, aby jądro przeżyło set_cr3().
    // Używamy flag PRESENT i WRITABLE, ale BEZ flagi PAGE_USER!
    for (uint64_t addr = 0; addr < 0x800000; addr += 4096) {
        vmm_map_page_ex(phys_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }
    return phys_pml4;
}

void vmm_map_mmio(uint64_t virt, uint64_t phys, size_t size) {
    size_t page_count = (size + 4095) / 4096; // Zaokrąglij w górę do liczby stron
    for (size_t i = 0; i < page_count; i++) {
        vmm_map_page(virt + i * 4096, phys + i * 4096, PAGE_PRESENT | PAGE_WRITABLE);
    }
}

//VMM MAP HIGE - mapowanie dużych stron (2MB) - używane do mapowania całej pamięci fizycznej w kernelu


void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    
    // Zawsze używamy lustra do dostępu do tablic!
    uint64_t* pml4 = (uint64_t*)phys_to_virt(cr3 & ~0xFFFULL);

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pml4[pml4_idx] = (uint64_t)pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        // Zerowanie nowej tablicy przez lustro
        uint64_t* next_table = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) next_table[i] = 0;
    }
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pdpt[pdpt_idx] = (uint64_t)pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        uint64_t* next_table = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) next_table[i] = 0;
    }
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    // MASKOWANIE ADRESU: Dla Huge Page bity 0-20 MUSZĄ być zero!
    // Error 8 wyskakuje, jeśli spróbujesz tam wstawić "nieczysty" adres.
    pd[pd_idx] = (phys & ~0x1FFFFFULL) | flags | PAGE_HUGE | PAGE_PRESENT;
}

// Prosty test mapowania strony i wywołania przerwania (do sprawdzenia, czy TLB jest odświeżany)
void vmm_test() {
    // Mapujemy stronę 0x400000 na fizyczną 0x400000
    vmm_map_page(0x400000, 0x400000, PAGE_PRESENT | PAGE_WRITABLE);
    
    // Zapisujemy coś na tej stronie
    uint64_t* ptr = (uint64_t*)0x400000;
    *ptr = 0xDEADBEEF;

    // Teraz wywołajmy przerwanie, żeby sprawdzić, czy TLB jest odświeżany
    asm volatile("int $3"); // Breakpoint interrupt - zatrzyma się w debuggerze
}

// Ogólnie mapowanie powinno działać dynamicznie i nie powinno wymagać ręcznego odświeżania CR3, bo vmm_map_page i vmm_map_page_ex już robią invlpg.
// Jednak ten test jest tu, żebyś mógł ręcznie sprawdzić, czy
// 1. Strona jest poprawnie mapowana
// 2. Dane są zapisywane i odczytywane poprawnie
// 3. Po mapowaniu i zapisie, wywołanie przerwania (int $3) powinno działać bez błędów, co oznacza, że TLB jest odświeżany i strona jest widoczna dla CPU.
// Jeśli wszystko jest poprawnie, po wywołaniu vmm_test() powinieneś zobaczyć, że przerwanie int $3 działa (zatrzymuje się w debuggerze) i możesz sprawdzić, że na stronie 0x400000 jest wartość 0xDEADBEEF. Jeśli przerwanie nie działa lub strona nie jest widoczna, to znaczy, że TLB nie został odświeżony i musisz sprawdzić implementację vmm_map_page i vmm_map_page_ex, czy poprawnie wykonują invlpg.
// Dodatkowo, jeśli chcesz ręcznie sprawdzić TLB, możesz po mapowaniu strony i zapisie wartości, spróbować odczytać tę wartość bezpośrednio z adresu fizycznego (przez lustro) i zobaczyć, czy jest tam 0xDEADBEEF. Jeśli tak, to znaczy, że mapa działa, ale jeśli przerwanie int $3 nie działa, to znaczy, że TLB nie został odświeżony i CPU nadal widzi starą mapę bez tej strony.

void vmm_map_huge_ex(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    // Ta funkcja jest podobna do vmm_map_huge, ale pozwala podać własny PML4 (przydatne przy tworzeniu nowych przestrzeni adresowych)
    uint64_t* pml4 = (uint64_t*)phys_to_virt(pml4_phys);

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pml4[pml4_idx] = (uint64_t)pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        uint64_t* next_table = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) next_table[i] = 0;
    }
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pdpt[pdpt_idx] = (uint64_t)pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        uint64_t* next_table = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) next_table[i] = 0;
    }
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    pd[pd_idx] = (phys & ~0x1FFFFFULL) | flags | PAGE_HUGE | PAGE_PRESENT;
}

