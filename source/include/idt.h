#pragma once
#include <stdint.h>

// W pliku idt.cpp lub nagłówku idt.h
extern "C" {
    void isr_ignore_stub();
    void isr_keyboard_stub();
}

struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t  ist;
    uint8_t  attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern "C" void isr0_handler(); // Stub dla klawiatury w asm