#pragma once
#include <stdint.h>

// Flagi dla wpisów w tablicach stron
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

extern "C" void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
extern "C" uint64_t vmm_get_phys(uint64_t virtual_addr);

extern "C" void vmm_init_direct_map(uint64_t mem_size_gb);
extern "C" void vmm_map_huge(uint64_t virt, uint64_t phys, uint64_t flags);

//heap init
extern "C" void heap_init(void* addr, uint64_t size);