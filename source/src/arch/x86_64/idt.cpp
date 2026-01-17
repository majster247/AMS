#include "idt.h"
#include "io.h"
#include "kernel.h"

idt_entry idt[256];
idtr _idtr;

// Deklarujemy stuby z assemblera
extern "C" void isr_keyboard_stub();
extern "C" void isr_ignore_stub();

extern "C" void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uintptr_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // Mapowanie Master PIC na 0x20 (32)
    outb(0xA1, 0x28); // Mapowanie Slave PIC na 0x28 (40)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC); // Maskowanie: 11111100 (odblokowane IRQ0 - timer i IRQ1 - klawiatura)
    outb(0xA1, 0xFF); // Wszystko zablokowane na Slave
}

extern "C" void timer_handler_stub();

extern "C" void idt_init() {
    // 1. Wypełnij wszystko ignorowaniem
    for(int i = 0; i < 256; i++) {
        idt_set_descriptor(i, (void*)isr_ignore_stub, 0x8E);
    }

    // 1. Zarejestruj Timer (IRQ 0 -> wektor 32 czyli 0x20)
    idt_set_descriptor(32, (void*)timer_handler_stub, 0x8E);

    // 2. Klawiatura (IRQ 1 -> 33)
    idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);

    // 3. Popraw maskowanie w pic_remap lub tutaj:
    // 0xFC = 11111100 (odblokowane IRQ0 - timer i IRQ1 - klawiatura)
    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF);

    // 2. Ustaw konkretnie klawiaturę (IRQ 1 -> 33)
    idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);

    // 3. Remap PIC
    pic_remap();

    idt_set_descriptor(32, (void*)timer_handler_stub, 0x8E);

    // 4. Załaduj IDT
    _idtr.base = (uintptr_t)&idt[0];
    _idtr.limit = (uint16_t)sizeof(idt_entry) * 256 - 1;

    asm volatile ("lidt %0" : : "m"(_idtr));
    asm volatile ("sti");
}