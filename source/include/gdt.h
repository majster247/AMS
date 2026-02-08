#pragma once
#include <stdint.h>

struct gdt_entry_bits {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;      // Stos jądra dla Ring 3 -> Ring 0
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

struct gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

extern "C" void gdt_init();
extern "C" void syscall_init();