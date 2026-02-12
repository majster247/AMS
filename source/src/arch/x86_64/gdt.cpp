#include "gdt.h"
#include "kernel.h"

static uint64_t gdt_real[9]; 
tss_entry system_tss;
CpuData cpu_data; 
static uint8_t double_fault_stack[8192]; // IST dla błędów krytycznych

extern "C" void gdt_init() {
    for(int i=0; i<9; i++) gdt_real[i] = 0;

    // Index 1: Kernel Code (0x08)
    gdt_real[1] = 0x00AF9A000000FFFFULL; 
    // Index 2: Kernel Data (0x10)
    gdt_real[2] = 0x00CF92000000FFFFULL; 

    // --- STANDARD LINUXA (WYMAGANY PRZEZ SYSRET) ---
    // Index 3: User Data 32 (Dummy / Compatibility)
    gdt_real[3] = 0x00CFF2000000FFFFULL;
    // Index 4: User Code 32 (Dummy / Compatibility)
    gdt_real[4] = 0x00AFFA000000FFFFULL; // L=1, ale użyjemy go jako bazy
    
    // Index 5: User Data 64 (0x2B) - Tutaj celuje SYSRET SS
    gdt_real[5] = 0x00CFF2000000FFFFULL; 
    // Index 6: User Code 64 (0x33) - Tutaj celuje SYSRET CS
    gdt_real[6] = 0x00AFFA000000FFFFULL; 
    // ----------------------------------------------

    // TSS (Index 7+8)
    memset(&system_tss, 0, sizeof(tss_entry));
    system_tss.ist[1] = (uint64_t)double_fault_stack + 8192; // Bezpieczny stos
    system_tss.iopb_offset = sizeof(tss_entry);

    uint64_t tss_base = (uint64_t)&system_tss;
    uint32_t tss_limit = sizeof(tss_entry) - 1;

    gdt_real[7] = (tss_limit & 0xFFFF) | ((tss_base & 0xFFFF) << 16) | 
                  ((tss_base & 0xFF0000) << 16) | (0x89ULL << 40) | 
                  (((tss_limit >> 16) & 0x0F) << 48) | ((tss_base & 0xFF000000) << 32);
    gdt_real[8] = (tss_base >> 32);
    
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr;
    gdtr.limit = sizeof(gdt_real) - 1;
    gdtr.base = (uint64_t)&gdt_real;

    asm volatile("lgdt %0" :: "m"(gdtr));
    asm volatile("ltr %%ax" :: "a"(0x38)); // Index 7

    // GS Base dla CpuData
    uint64_t gs_base = (uint64_t)&cpu_data;
    asm volatile("wrmsr" :: "a"(gs_base & 0xFFFFFFFF), "d"(gs_base >> 32), "c"(0xC0000102));

    write_serial_string("[GDT] Linux-style (Indices 5/6) configured.\n");
}