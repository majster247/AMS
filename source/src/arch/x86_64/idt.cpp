#include "idt.h"
#include "io.h"
#include "task.h"
#include "kernel.h"

// Tablica wskaźników do stubów w ASM
extern "C" {
    void isr0(); void isr1(); void isr2(); void isr3(); void isr4(); void isr5();
    void isr6(); void isr7(); void isr8(); void isr9(); void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15(); void isr16(); void isr17();
    void isr18(); void isr19(); void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
    void isr30(); void isr31();
    void isr128(); // INT 0x80

    void irq0(); void irq1(); void irq2(); void irq3(); void irq4(); void irq5();
    void irq6(); void irq7(); void irq8(); void irq9(); void irq10(); void irq11();
    void irq12(); void irq13(); void irq14(); void irq15();
    extern uint64_t dbg_ring3_rip;
    extern uint64_t dbg_ring3_rsp;
    extern uint64_t dbg_ring3_ksp;
    void keyboard_handler(registers* r);
    void mouse_handler(registers* r);
}
//deklaracja idt tablicy struktur idt_entry
idt_entry idt[256];


// Główny dyspozytor wywoływany z interrupts.s
extern "C" void interrupt_handler(registers* regs) {
    uint64_t int_no = regs->int_no;

    if (int_no < 32) {
        // Obsługa wyjątków CPU
        write_serial_string("CPU EXCEPTION: ");
        write_serial_dec(int_no);
        write_serial_string(" at RIP: ");
        write_serial_hex(regs->rip);
        write_serial_string("\n");
        if (int_no == 13) {
            write_serial_string("[DBG][IRET] planned RIP: ");
            write_serial_hex(dbg_ring3_rip);
            write_serial_string(" planned RSP: ");
            write_serial_hex(dbg_ring3_rsp);
            write_serial_string(" kernel RSP before frame: ");
            write_serial_hex(dbg_ring3_ksp);
            write_serial_string("\n");
        }
        
        // Jeśli to Page Fault (14)
        if (int_no == 14) {
            uint64_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            write_serial_string("Fault address: ");
            write_serial_hex(cr2);
            write_serial_string("\n");
        }
        
        asm volatile("cli; hlt");
    } 
    else if (int_no >= 32 && int_no <= 47) {
        // Obsługa IRQ (Hardware)
        if (int_no == 33) {
            keyboard_handler(regs);
        } else if (int_no == 44) {
            mouse_handler(regs);
        }
        
        // Wyślij EOI do PIC
        if (int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
    else if (int_no == 128) {
        // Obsługa syscalli
        // To jest wywoływane z syscall_entry.s, który ustawia odpowiednie dane w rejestrach
        // i wywołuje tę funkcję. Tutaj powinieneś przekazać te dane do syscall_handlera.
        extern uint64_t syscall_handler(registers* regs);
        uint64_t result = syscall_handler(regs);
        regs->rax = result; // Zwróć wynik do RAX, który trafi z powrotem do procesu użytkownika
    }
     else {
        write_serial_string("UNKNOWN INTERRUPT: ");
        write_serial_dec(int_no);
        write_serial_string("\n");
    }
}

//idt_set_gate definicja:
/*

@brief Struktura wpisu IDT (16 bajtów w trybie 64-bitowym) 
struct idt_entry {
    uint16_t isr_low;  
    uint16_t kernel_cs;  
    uint8_t  ist;        
    uint8_t  attributes; 
    uint16_t isr_mid;    
    uint32_t isr_high;   
    uint32_t reserved;
} __attribute__((packed));

Struktura tablicy IDT to tablica 256 wpisów, gdzie każdy wpis jest strukturą idt_entry. Każdy wpis zawiera adres handlera przerwania (podzielony na trzy części: low, mid, high), selektor segmentu kodu jądra, informacje o stosie przerwania (IST) oraz atrybuty określające typ bramki i uprawnienia.

*/


void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    // Na podstawie linux ABI i standardów x86_64, ustawiamy odpowiednie pola w strukturze idt_entry
    idt[num].isr_low = base & 0xFFFF; // Dolne
    idt[num].kernel_cs = sel;
    idt[num].ist = ist & 0x7; // IST to tylko 3 bity
    idt[num].attributes = flags;
    idt[num].isr_mid = (base >> 16) & 0xFFFF; // Środkowe
    idt[num].isr_high = (base >> 32) & 0xFFFFFFFF; // Górne
    idt[num].reserved = 0;
}



extern "C" void idt_init() {
    // Tutaj powinieneś wypełnić tablicę IDT odpowiednimi adresami stubów (isr0, isr1, ..., irq0, irq1, ..., isr128)
    // i ustawić odpowiednie flagi (np. obecność, DPL, typ bramki)
    // Następnie załaduj IDTR podobnie jak GDTR (limit + base)

     // Załaduj IDTR
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr;

    idtr.limit = (sizeof(idt_entry) * 256) - 1;
    idtr.base = (uint64_t)&idt;
    //Samodzielna konfiguracja na podstawie Linuxa ABI oraz POSIX:
    //Wyjątki CPU (ISRs 0-31)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0x8E, 1); // DPL 0, Present, Interrupt Gate
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0x8E, 0);
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0x8E, 0);
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0x8E, 0);
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0x8E, 0);
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0x8E, 0);
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0x8E, 0);
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0x8E, 0);
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0x8E, 1); // DPL 0, Present, Interrupt Gate, IST 1 (dla wyjątków krytycznych)
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0x8E, 0);
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0x8E, 0);
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0x8E, 0);
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0x8E, 0);
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0x8E, 1); // #GP na IST1, by przeżyć błędny RSP
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0x8E, 1); // DPL 0, Present, Interrupt Gate, IST 1 (Page Fault)
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0x8E, 0);
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0x8E, 0);
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0x8E, 0);
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0x8E, 0);
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0x8E, 0);
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0x8E, 0);
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0x8E, 0);
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0x8E, 0);
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0x8E, 0);
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0x8E, 0);
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0x8E, 0);
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0x8E, 0);
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0x8E, 0);
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0x8E, 0);
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0x8E, 0);
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0x8E, 0);
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0x8E, 0);

    //IRQ (Hardware) - offset 32
    idt_set_gate(32, (uint64_t)irq0, 0x08, 0x8E, 0);
    idt_set_gate(33, (uint64_t)irq1, 0x08, 0x8E, 0);
    idt_set_gate(34, (uint64_t)irq2, 0x08, 0x8E, 0);
    idt_set_gate(35, (uint64_t)irq3, 0x08, 0x8E, 0);
    idt_set_gate(36, (uint64_t)irq4, 0x08, 0x8E, 0);
    idt_set_gate(37, (uint64_t)irq5, 0x08, 0x8E, 0);
    idt_set_gate(38, (uint64_t)irq6, 0x08, 0x8E, 0);
    idt_set_gate(39, (uint64_t)irq7, 0x08, 0x8E, 0);
    idt_set_gate(40, (uint64_t)irq8, 0x08, 0x8E, 0);
    idt_set_gate(41, (uint64_t)irq9, 0x08, 0x8E, 0);
    idt_set_gate(42, (uint64_t)irq10, 0x08, 0x8E, 0);
    idt_set_gate(43, (uint64_t)irq11, 0x08, 0x8E, 0);
    idt_set_gate(44, (uint64_t)irq12, 0x08, 0x8E, 0);
    idt_set_gate(45, (uint64_t)irq13, 0x08, 0x8E, 0);
    idt_set_gate(46, (uint64_t)irq14, 0x08, 0x8E, 0);
    idt_set_gate(47, (uint64_t)irq15, 0x08, 0x8E, 0);

    //Syscall - INT 0x80 (128)
    idt_set_gate(128, (uint64_t)isr128, 0x08, 0xEE, 0); // DPL 3, Present, Interrupt Gate

    write_serial_string("[IDT] Setting up IDT entries for ISRs, IRQs, and Syscall.\n");
    asm volatile("lidt %0" : : "m"(idtr));
    write_serial_string("[IDT] IDTR loaded with base: ");
    write_serial_hex(idtr.base);
    write_serial_string(" and limit: ");
    write_serial_hex(idtr.limit);
    write_serial_string("\n");
    write_serial_string("[IDT] IDT entries set and IDTR loaded.\n");
    //asm volatile("sti"); // Włączamy przerwania
    //write_serial_string("[IDT] Interrupts enabled.\n");

    // 0x00 oznacza "przepuszczaj wszystko"
    //outb(0x21, 0x00); // Odmaskuj Master PIC
    //outb(0xA1, 0x00); // Odmaskuj Slave PIC

    write_serial_string("[IDT] IDT initialized with ISRs and IRQs.\n");

}