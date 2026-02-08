#include "idt.h"
#include "kernel.h"
#include "task.h" 
#include "io.h" // Niezbędne dla outb/inb

// Deklaracje funkcji assemblerowych (interrupts.s)
extern "C" {
    void isr0(); void isr1(); void isr2(); void isr3(); void isr4(); void isr5();
    void isr6(); void isr7(); void isr8(); void isr9(); void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15(); void isr16(); void isr17();
    void isr18(); void isr19(); void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
    void isr30(); void isr31();

    void irq0(); void irq1(); void irq2(); void irq3(); void irq4(); void irq5();
    void irq6(); void irq7(); void irq8(); void irq9(); void irq10(); void irq11();
    void irq12(); void irq13(); void irq14(); void irq15();
}

// Tablice wskaźników
void* isr_stub_table[32] = {
    (void*)isr0, (void*)isr1, (void*)isr2, (void*)isr3, (void*)isr4, (void*)isr5,
    (void*)isr6, (void*)isr7, (void*)isr8, (void*)isr9, (void*)isr10, (void*)isr11,
    (void*)isr12, (void*)isr13, (void*)isr14, (void*)isr15, (void*)isr16, (void*)isr17,
    (void*)isr18, (void*)isr19, (void*)isr20, (void*)isr21, (void*)isr22, (void*)isr23,
    (void*)isr24, (void*)isr25, (void*)isr26, (void*)isr27, (void*)isr28, (void*)isr29,
    (void*)isr30, (void*)isr31
};

void* irq_stub_table[16] = {
    (void*)irq0, (void*)irq1, (void*)irq2, (void*)irq3, (void*)irq4, (void*)irq5,
    (void*)irq6, (void*)irq7, (void*)irq8, (void*)irq9, (void*)irq10, (void*)irq11,
    (void*)irq12, (void*)irq13, (void*)irq14, (void*)irq15
};

// Zmienne globalne IDT
__attribute__((aligned(0x10))) 
static idt_entry idt[256];
static idt_ptr idtr;

// Helper ustawiający wpis
void idt_set_gate(uint8_t num, void* base, uint16_t sel, uint8_t flags) {
    uint64_t addr = (uint64_t)base;
    idt[num].isr_low = addr & 0xFFFF;
    idt[num].kernel_cs = sel;
    idt[num].ist = 0;
    idt[num].attributes = flags;
    idt[num].isr_mid = (addr >> 16) & 0xFFFF;
    idt[num].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[num].reserved = 0;
}

// Remapowanie PIC (Programmable Interrupt Controller)
void idt_remap_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x00); outb(0xA1, 0x00);
}

// Główna inicjalizacja
extern "C" void idt_init() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    // ISRs
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    // Remap PIC
    idt_remap_pic();

    // IRQs
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, irq_stub_table[i], 0x08, 0x8E);
    }
    
    asm volatile("lidt %0" :: "m"(idtr));
    asm volatile("sti"); // Włącz przerwania
}

// Handlery C++ wywoływane z assemblera

extern "C" void isr_handler(registers* r) {
    (void)r; // Unused
    // Tutaj logika błędów (Page Fault, GPF itp.)
}

extern "C" void keyboard_handler(registers* r);
extern "C" void mouse_handler(registers* r);

extern "C" void irq_handler(registers* r, uint64_t irq_num) {
    
    if (irq_num == 33) { // IRQ 1: Klawiatura
        keyboard_handler(r);
    } 
    else if (irq_num == 44) { // IRQ 12: Mysz (PS/2 Mouse to zazwyczaj IRQ 12, czyli 32+12=44)
        mouse_handler(r);
    }

    // Wysłanie EOI (End of Interrupt) do kontrolera PIC
    if (irq_num >= 40) {
        outb(0xA0, 0x20); // Slave PIC
    }
    outb(0x20, 0x20); // Master PIC
}