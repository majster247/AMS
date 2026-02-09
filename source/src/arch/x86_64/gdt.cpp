#include "gdt.h"
#include "kernel.h"
#include "heap.h"

// Struktura per-CPU (na razie jeden CPU)
struct CpuData {
    uint64_t self;
    uint64_t kernel_stack; 
};

tss_entry system_tss;
static uint64_t gdt_real[8]; 
static CpuData cpu_data; 
static uint8_t kernel_stack_buffer[16384]; // 16KB stosu

extern "C" void gdt_init() {
    // 1. Czyścimy GDT
    for(int i=0; i<8; i++) gdt_real[i] = 0;

    // 2. Kernel Segments
    gdt_real[1] = 0x00AF9A000000FFFFULL; // Kernel Code
    gdt_real[2] = 0x00CF92000000FFFFULL; // Kernel Data

    // 3. User Segments (Poprawione Access Bytes!)
    // Było 0x...32... -> Jest 0x...F2... (Present=1, DPL=3, Data, RW)
    gdt_real[3] = 0x00CFF2000000FFFFULL; 
    
    // Było 0x...3A... -> Jest 0x...FA... (Present=1, DPL=3, Code, Readable, 64-bit)
    gdt_real[4] = 0x00AFFA000000FFFFULL; 

    // 4. Konfiguracja TSS
    for(int i=0; i<(int)sizeof(tss_entry); i++) ((uint8_t*)&system_tss)[i] = 0;
    
    // Alokujemy stos dla jądra (ważne przy syscallu)
    system_tss.rsp0 = (uint64_t)kernel_stack_buffer + 16384;
    cpu_data.kernel_stack = system_tss.rsp0; // Muszą być identyczne, bo GS będzie do tego odwoływał się w syscallu
    system_tss.iopb_offset = sizeof(tss_entry);

    // 5. Deskryptor TSS (Bezpieczna metoda)
    uint64_t tss_base = (uint64_t)&system_tss;
    uint32_t tss_limit = sizeof(tss_entry) - 1;

    // Slot 5 (Low part)
    gdt_real[5] = 0;
    gdt_real[5] |= (tss_limit & 0xFFFF);
    gdt_real[5] |= (tss_base & 0xFFFF) << 16;
    gdt_real[5] |= ((tss_base >> 16) & 0xFF) << 32;
    gdt_real[5] |= (0x89ULL) << 40; // Type: Available 64-bit TSS (0x9), Present (0x80) -> 0x89
    gdt_real[5] |= ((uint64_t)(tss_limit >> 16) & 0x0F) << 48;
    gdt_real[5] |= ((tss_base >> 24) & 0xFF) << 56;

    // Slot 6 (High part)
    gdt_real[6] = 0;
    gdt_real[6] |= (tss_base >> 32);
    
    gdt_tss_entry* tss_desc = (gdt_tss_entry*)&gdt_real[5];

    tss_desc->limit_low = tss_limit & 0xFFFF;
    tss_desc->base_low = tss_base & 0xFFFF;
    tss_desc->base_mid = (tss_base >> 16) & 0xFF;
    tss_desc->flags1 = 0x89; // Present(1), DPL(0), Type(1001b = 64-bit TSS Available)
    tss_desc->flags2 = 0x00; // Granularity(0)
    tss_desc->base_high_mid = (tss_base >> 24) & 0xFF;
    tss_desc->base_high = (tss_base >> 32) & 0xFFFFFFFF; // Górne 32 bity adresu
    tss_desc->reserved = 0;

    // 6. Załaduj GDT
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) gdtr;
    gdtr.limit = sizeof(gdt_real) - 1;
    gdtr.base = (uint64_t)&gdt_real;

    asm volatile("lgdt %0" :: "m"(gdtr));
    asm volatile("ltr %%ax" :: "a"(0x28)); // 0x28 to 5-ty wpis w GDT (5 * 8)

    // 7. Konfiguracja GS dla SYSCALL
    cpu_data.kernel_stack = system_tss.rsp0;
    cpu_data.self = (uint64_t)&cpu_data;

    uint64_t gs_base = (uint64_t)&cpu_data;
    uint32_t lo = gs_base & 0xFFFFFFFF;
    uint32_t hi = gs_base >> 32;
    // MSR_KERNEL_GS_BASE = 0xC0000102
    asm volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0xC0000102));

    write_serial_string("[GDT] GDT (Ring3 fix), TSS i GS Base skonfigurowane.\n");
}