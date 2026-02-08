#pragma once
#include <stdint.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE     (1ULL << 7)

extern "C" {
    void vmm_init_direct_map(uint64_t ram_size_mb);
    uint64_t vmm_get_phys(uint64_t virt);
    
    void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
    void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags);
    void vmm_map_user(uint64_t virt, uint64_t phys, bool writable);
    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags); // Legacy wrapper
    void vmm_set_nocache(uint64_t virt);

    //heap
    void heap_init(void* addr, uint64_t size);
}