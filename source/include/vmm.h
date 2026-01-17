#pragma once
#include <stdint.h>

// Flagi dla wpisów w tablicach stron
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

extern "C" void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);