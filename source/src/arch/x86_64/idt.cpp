#include "idt.h"
#include "io.h"
#include "kernel.h"
#include "graphics.h"
#include "mouse.h"

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

extern "C" void exception_handler() {
    // Wypisz coś bezpośrednio na port szeregowy, omijając przerwania
    outb(0x3F8, 'C');
    outb(0x3F8, 'R');
    outb(0x3F8, 'A');
    outb(0x3F8, 'S');
    outb(0x3F8, 'H');
    while(1); // Zatrzymaj system, żebyś widział log w konsoli
}


void pic_remap() {
    outb(0x20, 0x11); // ICW1_INIT | ICW1_ICW4
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // Master PIC offset (wektory 32-39)
    outb(0xA1, 0x28); // Slave PIC offset (wektory 40-47)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

extern "C" void timer_handler_stub();
extern "C" void mouse_int_asm_wrapper(); // Deklaracja wrappera z interrupts.s

extern "C" void timer_handler_c() {
    system_ticks++;
    outb(0x20, 0x20); // KONIECZNE: Powiedz PIC, że obsłużyłeś przerwanie
}
/*

extern "C" void mouse_handler() {
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return; // Brak danych w buforze
    if (!(status & 0x20)) return; // Dane nie pochodzą od myszy

    uint8_t data = inb(0x60);
    write_serial_hex(data); // Zobacz jakie bajty faktycznie sypie myszka
    
    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = data;
            if (!(data & 0x08)) return; // Błąd synchronizacji pakietu
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = data;
            
            // Logika przycisków
            mouse_left_pressed = (mouse_byte[0] & 0x01);
            mouse_right_pressed = (mouse_byte[0] & 0x02);

            // Ruch X (uwzględniając znak)
            int32_t rel_x = mouse_byte[1];
            if (mouse_byte[0] & 0x10) rel_x -= 256;
            
            // Ruch Y (uwzględniając znak - w PS/2 Y jest odwrócone!)
            int32_t rel_y = mouse_byte[2];
            if (mouse_byte[0] & 0x20) rel_y -= 256;

            mouse_x += rel_x;
            mouse_y -= rel_y; // Minus, bo w GUI Y rośnie w dół

            // Limity ekranu
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > 1279) mouse_x = 1279;
            if (mouse_y > 719) mouse_y = 719;

            mouse_cycle = 0;
            break;
    }
    
    // EOI dla PIC (Bardzo ważne!)
    outb(0xA0, 0x20); // Slave PIC
    outb(0x20, 0x20); // Master PIC
    while (inb(0x64) & 1) inb(0x60); // Wywal wszystko co zostało w porcie danych
}
*/
extern "C" void idt_init() {
    // 1. Wszystko na ignorowanie
    for(int i = 0; i < 256; i++) {
        idt_set_descriptor(i, (void*)isr_ignore_stub, 0x8E);
    }

    pic_remap();

    // 2. Wyjątki (Kluczowe dla debugowania!)
    idt_set_descriptor(0, (void*)exception_handler, 0x8E);  // Divide by Zero
    idt_set_descriptor(1, (void*)exception_handler, 0x8E);  // Debug
    idt_set_descriptor(2, (void*)exception_handler, 0x8E);  // Non Maskable Interrupt
    idt_set_descriptor(3, (void*)exception_handler, 0x8E);  // Breakpoint
    idt_set_descriptor(4, (void*)exception_handler, 0x8E);  // Overflow
    idt_set_descriptor(5, (void*)exception_handler, 0x8E);  // Bound Range Exceeded
    idt_set_descriptor(6, (void*)exception_handler, 0x8E);  // Invalid Opcode
    idt_set_descriptor(7, (void*)exception_handler, 0x8E);  // Device Not Available
    idt_set_descriptor(8, (void*)exception_handler, 0x8E);  // Double Fault
    idt_set_descriptor(9, (void*)exception_handler, 0x8E);  // Coprocessor Segment Overrun
    idt_set_descriptor(10, (void*)exception_handler, 0x8E); // Invalid TSS
    idt_set_descriptor(11, (void*)exception_handler, 0x8E); // Segment Not Present
    idt_set_descriptor(12, (void*)exception_handler, 0x8E); // Stack Segment Fault
    idt_set_descriptor(13, (void*)exception_handler, 0x8E); // General Protection Fault
    idt_set_descriptor(14, (void*)exception_handler, 0x8E); // Page Fault
    idt_set_descriptor(16, (void*)exception_handler, 0x8E); // x87 FPU Floating-Point Error
    idt_set_descriptor(17, (void*)exception_handler, 0x8E); // Alignment Check
    idt_set_descriptor(18, (void*)exception_handler, 0x8E); // Machine Check
    idt_set_descriptor(19, (void*)exception_handler, 0x8E); // SIMD Floating-Point Exception
    idt_set_descriptor(20, (void*)exception_handler, 0x8E); // Virtualization Exception
    idt_set_descriptor(21, (void*)exception_handler, 0x8E); // Control Protection Exception
    idt_set_descriptor(22, (void*)exception_handler, 0x8E); // Hypervisor Injection Exception
    idt_set_descriptor(23, (void*)exception_handler, 0x8E); // VMM Communication Exception
    idt_set_descriptor(24, (void*)exception_handler, 0x8E); // Security Exception
    idt_set_descriptor(25, (void*)exception_handler, 0x8E); // Triple Fault
    idt_set_descriptor(26, (void*)exception_handler, 0x8E); // FPU Virtualization Exception
    idt_set_descriptor(27, (void*)exception_handler, 0x8E); // Control Protection Exception
    idt_set_descriptor(28, (void*)exception_handler, 0x8E); // Reserved
    idt_set_descriptor(29, (void*)exception_handler, 0x8E); // Reserved
    idt_set_descriptor(30, (void*)exception_handler, 0x8E); // Reserved
    idt_set_descriptor(31, (void*)exception_handler, 0x8E); // Reserved

    // 3. IRQs
    idt_set_descriptor(32, (void*)timer_handler_stub, 0x8E); // TIMER (Scheduler)
    idt_set_descriptor(33, (void*)isr_keyboard_stub, 0x8E);  // Klawiatura
    idt_set_descriptor(44, (void*)mouse_int_asm_wrapper, 0x8E);  // Myszka (IRQ12 to wektor 44)
    idt_set_descriptor(46, (void*)exception_handler, 0x8E); // Obsługa błędów z AHCI (INT 46 to IRQ14)

    // 4. Odmaskuj przerwania na PIC
    // Master PIC (0x21):
    // Bit 0 (Timer) = 0 (ON)
    // Bit 1 (Keyboard) = 0 (ON)
    // Bit 2 (Cascade/Slave) = 0 (ON) - TO JEST KLUCZOWE DLA MYSZY!
    // Reszta zamaskowana (1) -> 1111 1000 = 0xF8
    outb(0x21, 0xF8);

    // Slave PIC (0xA1):
    // Bit 4 (Mouse IRQ 12) = 0 (ON) -> 1110 1111 = 0xEF
    outb(0xA1, 0xEF);
    
    // Odmaskuj IRQ2 na Masterze (kaskada)
    //outb(0x21, inb(0x21) & ~(1 << 2));

    // Odmaskuj IRQ12 na Slavie (mysz)
    //outb(0xA1, inb(0xA1) & ~(1 << 4));

    // 5. Load IDT
    _idtr.base = (uintptr_t)&idt[0];
    _idtr.limit = (uint16_t)sizeof(idt_entry) * 256 - 1;
    asm volatile ("lidt %0" : : "m"(_idtr));
}

void timer_init(uint32_t hz) {
    uint32_t divisor = 1193180 / hz;       
    outb(0x43, 0x36);                     
    outb(0x40, (uint8_t)(divisor & 0xFF)); 
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); 
}