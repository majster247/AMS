#include "idt.h"
#include "kernel.h"
#include "io.h"

// Musi pasować do tego, co asembler wrzuca na stos!
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    //uint64_t ds;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

extern "C" {
    // ISRs
    void isr0(); void isr1(); void isr2(); void isr3(); void isr4(); void isr5();
    void isr6(); void isr7(); void isr8(); void isr9(); void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15(); void isr16(); void isr17();
    void isr18(); void isr19(); void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
    void isr30(); void isr31();

    void isr128(); // Syscall (INT 0x80)

    // IRQs
    void irq0(); void irq1(); void irq2(); void irq3(); void irq4(); void irq5();
    void irq6(); void irq7(); void irq8(); void irq9(); void irq10(); void irq11();
    void irq12(); void irq13(); void irq14(); void irq15();

}

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

__attribute__((aligned(0x10))) 
static idt_entry idt[256];
static idt_ptr idtr;

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

void idt_remap_pic() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x00); outb(0xA1, 0x00); // Odmaskuj wszystko
}

extern "C" void idt_init() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    // 1. Wyjątki (0-31)
    for (int i = 0; i < 32; i++) idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    idt_remap_pic();

    // 2. Sprzęt (32-47)
    for (int i = 0; i < 16; i++) idt_set_gate(32 + i, irq_stub_table[i], 0x08, 0x8E);

    // 0xEE = 11101110b -> Present, DPL=3 (Ring 3), 64-bit Interrupt Gate
    idt_set_gate(128, (void*)isr128, 0x08, 0xEE); 

    asm volatile("lidt %0" :: "m"(idtr));
}

extern "C" void keyboard_handler(registers* r);
extern "C" void mouse_handler(registers* r);
// Syscall handler jest w syscall.cpp, ale deklarujemy go tutaj, bo to też przerwanie!
extern "C" void syscall_handler(registers* regs);

extern "C" void isr_handler(registers* r) {
    // 1. Jeśli to syscall (Int 128 / 0x80), skocz do handlera syscalli
    if (r->int_no == 128) {
        syscall_handler(r);
        return;
    }

    // 2. Jeśli to Timer (zakładamy IRQ0 = 32), ignoruj (lub scheduler to obsłużył w ASM)
    if (r->int_no == 32) return;

    // 3. Reszta to błędy CPU - wypisz diagnostykę i zatrzymaj system
    write_serial_string("\n!!! CPU EXCEPTION !!!\n");
    
    char buf[64];
    sprintf(buf, "Int: %d (Error: %x)\n", r->int_no, r->err_code);
    write_serial_string(buf);

    sprintf(buf, "RIP: %x  CS: %x\n", r->rip, r->cs);
    write_serial_string(buf);
    
    sprintf(buf, "RAX: %x  RSP: %x\n", r->rax, r->rsp);
    write_serial_string(buf);

    write_serial_string("\n[CPU ERROR DUMP]\n");
    write_serial_string(buf);

    asm volatile("cli; hlt");
}

extern "C" void irq_handler(registers* r, uint64_t irq_num) {
    if (irq_num == 32) {
        // Timer - milczymy
        outb(0x20, 0x20);
        return;
    }

    // DIAGNOSTYKA: Wypisz co dostaliśmy!
    //write_serial_string("IRQ GOT: ");
    //write_serial_dec(irq_num);
    //write_serial_string("\n");

    if (irq_num == 33) {
        keyboard_handler(r);
    }
    else if (irq_num == 44) {
        mouse_handler(r);
        outb(0xA0, 0x20);
    }
    
    outb(0x20, 0x20);
}

