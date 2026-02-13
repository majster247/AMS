#include "gdt.h"
#include "kernel.h"

uint64_t gdt_real[9]; // Globalna GDT (zgodnie z deklaracją w gdt.h)
tss_entry system_tss;
CpuData cpu_data; 
static uint8_t double_fault_stack[8192]; 

extern "C" void gdt_init() {
    for(int i=0; i<9; i++) gdt_real[i] = 0;

    // Index 1: Kernel Code (0x08)
    gdt_real[1] = 0x00AF9A000000FFFFULL; 
    // Index 2: Kernel Data (0x10)
    gdt_real[2] = 0x00CF92000000FFFFULL; 

    // UKŁAD POD SYSRET/SYSRETQ
    // Index 3: User Data 32
    gdt_real[3] = 0x00CFF2000000FFFFULL;
    // Index 4: User Code 32
    gdt_real[4] = 0x00AFFA000000FFFFULL;
    // Index 5: User Data 64 (0x2B)
    gdt_real[5] = 0x00CFF2000000FFFFULL;
    // Index 6: User Code 64 (0x33)
    gdt_real[6] = 0x00AFFA000000FFFFULL;

    // Konfiguracja TSS (Index 7+8)
    memset(&system_tss, 0, sizeof(tss_entry));
    system_tss.ist[1] = (uint64_t)double_fault_stack + 8192; 
    system_tss.iopb_offset = sizeof(tss_entry);

    uint64_t tss_base = (uint64_t)&system_tss;
    uint32_t tss_limit = sizeof(tss_entry) - 1;

    gdt_real[7] = (tss_limit & 0xFFFF) | ((tss_base & 0xFFFF) << 16) | 
                  ((tss_base & 0xFF0000) << 16) | (0x89ULL << 40) | 
                  (((tss_limit >> 16) & 0x0F) << 48) | ((tss_base & 0xFF000000) << 32);
    gdt_real[8] = (tss_base >> 32);
    
    // Pancerne ładowanie GDTR
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr_frame;

    gdtr_frame.limit = sizeof(gdt_real) - 1;
    gdtr_frame.base = (uint64_t)&gdt_real;

    asm volatile("lgdt %0" :: "m"(gdtr_frame));

    // PRZEŁADOWANIE SEGMENTÓW (Klucz do stabilności)
    asm volatile(
        "push $0x10\n"              // Kernel Data Selector
        "push %%rsp\n"
        "pushf\n"
        "push $0x08\n"              // Kernel Code Selector
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "iretq\n"                   // Czyści rury procesora i ładuje nowe segmenty
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        :: : "rax", "memory"
    );

    asm volatile("ltr %%ax" :: "a"(0x38)); // Index 7

    // GS Base dla CpuData
    uint64_t gs_base = (uint64_t)&cpu_data;
    asm volatile("wrmsr" :: "a"(gs_base & 0xFFFFFFFF), "d"(gs_base >> 32), "c"(0xC0000102));

    write_serial_string("[GDT] Linux-style GDT and Segments reloaded.\n");
}