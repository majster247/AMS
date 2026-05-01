/**
 * @file gdt.h
 * @author Majster
 * @brief Definicje Global Descriptor Table (GDT) oraz Task State Segment (TSS).
 */

#pragma once
#include <stdint.h>

/** @brief Standardowy wpis w tablicy GDT */
struct gdt_entry_bits {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/** @brief Task State Segment - kluczowy dla przełączania stosu przy przerwaniach Ring 3 */
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;      /**< Stos jądra, na który procesor przełączy się po przerwaniu z Ring 3 */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    /**< Interrupt Stack Table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

/** @brief Specjalny, 16-bajtowy wpis GDT dla TSS w trybie 64-bitowym */
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

struct CpuData {
    uint64_t self;               // Offset 0x00
    uint64_t kernel_stack;       // Offset 0x08
    uint64_t user_stack_scratch; // Offset 0x10
};

extern "C" {
    /** @brief Inicjalizuje tablicę GDT, wczytuje selektory i ładuje TSS */
    void gdt_init();
    /** @brief Inicjalizuje rejestr MSR_LSTAR dla szybkich wywołań systemowych (SYSCALL/SYSRET) */
    void syscall_init();

    extern tss_entry system_tss;
    extern CpuData cpu_data;
    //gdt_real
    extern uint64_t gdt_real[11]; // 9 standardowych + 2 dla TSS

    //main_kernel_stack i double_fault_stack są w gdt.cpp, ale deklarujemy je tutaj, bo są potrzebne do ustawienia TSS
    
    // DEFINICJA STOSU JĄDRA
    // Musi być wyrównany do 16 bajtów dla ABI x86_64

    
    //task jest w task.h i tam jest deklarowany, ale potrzebujemy tego symbolu do ustawienia TSS

    static uint8_t main_kernel_stack[16384] __attribute__((aligned(16)));
    static uint8_t double_fault_stack[8192];
    
    uint64_t get_kernel_stack_top();
}