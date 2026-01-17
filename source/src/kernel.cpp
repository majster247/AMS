#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"

void second_task() {
    while(1) {
        write_serial_string("[TASK 2] Działam równolegle!\n");
        // Tu można by dodać mały delay
        for(volatile int i = 0; i < 1000000; i++); 
    }
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);             // Command port: Mode 3 (Square wave)
    outb(0x40, (uint8_t)(divisor & 0xFF));        // Low byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}


extern "C" void kmain(uint64_t multiboot_info_address) {
    init_serial();
    terminal_initialize();

    // 1. Pamięć najpierw (żeby stos i IDT miały gdzie żyć)
    pmm_init(128 * 1024 * 1024, (void*)0x200000);
    parse_multiboot(multiboot_info_address);
    
    // 2. IDT (teraz bezpiecznie)
    idt_init();

    write_serial_string("Inicjalizacja zadań...\n");
    task* t1 = create_task(nullptr); 
    task* t2 = create_task(second_task);
    pit_init(100); // 100 Hz timer
    write_serial_string("Włączam przerwania (sti)...\n");
    asm volatile("sti");
    
    terminal_writestring("System started and multitasking active!\n");
    while(1) { asm("hlt"); }
}