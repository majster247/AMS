#include "gdt.h"
#include "kernel.h"
#include <string.h>

static constexpr uint16_t KERNEL_DS = 0x10;
static constexpr uint16_t TSS_SELECTOR = 0x38;
static constexpr uint64_t IST_STACK_SIZE = 8192;

// Linux-style segment descriptors (accessed bit set).
static constexpr uint64_t GDT_KERNEL_CODE64 = 0x00AF9B000000FFFFULL;
static constexpr uint64_t GDT_KERNEL_DATA64 = 0x00CF93000000FFFFULL;
static constexpr uint64_t GDT_USER_DATA64   = 0x00CFF3000000FFFFULL;
static constexpr uint64_t GDT_USER_CODE64   = 0x00AFFB000000FFFFULL;

uint64_t gdt_real[11] __attribute__((aligned(16))); 
tss_entry system_tss __attribute__((aligned(16)));
CpuData cpu_data __attribute__((aligned(16)));
static uint8_t ist1_stack[IST_STACK_SIZE] __attribute__((aligned(16)));

extern "C" uint8_t stack_top; 

extern "C" void gdt_init() {
    memset(gdt_real, 0, sizeof(gdt_real));

    gdt_real[0] = 0;                 // Null descriptor
    gdt_real[1] = GDT_KERNEL_CODE64; // 0x08
    gdt_real[2] = GDT_KERNEL_DATA64; // 0x10
    gdt_real[3] = 0;                 // 0x18 (reserved)
    gdt_real[4] = 0;                 // 0x20 (reserved)
    gdt_real[5] = GDT_USER_DATA64;   // 0x28 -> 0x2B
    gdt_real[6] = GDT_USER_CODE64;   // 0x30 -> 0x33

    // --- TSS (Indeks 7 i 8) ---
    uint64_t tss_base = (uint64_t)&system_tss;
    uint32_t tss_limit = sizeof(tss_entry) - 1;

    memset(&system_tss, 0, sizeof(tss_entry));
    system_tss.rsp0 = (uint64_t)&stack_top; // Stos dla przerwań po wejściu do Ring 0
    // IDT ma kilka bramek z IST=1, więc musi być ustawione IST1 (ist[0]).
    system_tss.ist[0] = (uint64_t)(ist1_stack + IST_STACK_SIZE);
    system_tss.iopb_offset = sizeof(tss_entry);

    // Deskryptor TSS jest 128-bitowy (zajmuje 2 sloty w GDT)
    gdt_real[7] = ((uint64_t)tss_limit & 0xFFFFULL) |
                  ((tss_base & 0xFFFFFFULL) << 16) |
                  (0x89ULL << 40) |  // Type: 0x89 (Available 64-bit TSS)
                  ((((uint64_t)tss_limit >> 16) & 0xFULL) << 48) |
                  (((tss_base >> 24) & 0xFFULL) << 56);
    gdt_real[8] = (tss_base >> 32) & 0xFFFFFFFFULL;

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;

    gdtr.limit = sizeof(gdt_real) - 1;
    gdtr.base = (uint64_t)gdt_real;

    asm volatile("lgdt %0" : : "m"(gdtr));

    // Po LGDT trzeba przeładować CS (far return), inaczej zostaje cache starego deskryptora.
    asm volatile(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        :
        : "rax", "memory"
    );
    
    // Ładowanie TSS (Indeks 7 * 8 = 56 = 0x38)
    asm volatile("ltr %%ax" : : "a"(TSS_SELECTOR));

    // Przeładowanie segmentów danych
    asm volatile(
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "i"(KERNEL_DS)
        : "ax"
    );
    
    write_serial_string("[GDT] Initialized and TSS loaded.\n");
}