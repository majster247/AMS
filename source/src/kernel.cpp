#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"
#include "tar.h"
#include "vfs.h"

// Zapowiedź funkcji z idt.cpp
extern "C" void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

// Zapowiedź symbolu z interrupts.s (asemblera)
extern "C" void isr_keyboard_stub();

extern uint64_t initrd_addr; // Zmienna z multiboot_parser.cpp

extern "C" void second_task() {
    while(1) {
        // Piszemy bezpośrednio do portu szeregowego bez zbędnych funkcji
        outb(0x3F8, 'B'); 
        // Bardzo długa pętla, żebyś zauważył literki
        for(volatile uint64_t i = 0; i < 10000000; i++); 
    }
}

void vga_matrix_effect() {
    uint16_t* vga = (uint16_t*)0xB8000;
    for(int i = 0; i < 80 * 25; i++) {
        uint8_t character = 33 + (i % 94); // Losowe znaki ASCII
        uint8_t color = (i % 15) + 1;
        vga[i] = (uint16_t)character | (uint16_t)(color << 8);
    }
}

void run_shell() {
    extern char cmd_buffer[];
    extern bool line_ready;
    extern int cmd_index;

    terminal_writestring("\nAMS-1 Shell> ");

    while (true) {
        if (line_ready) {
            if (strcmp(cmd_buffer, "ls") == 0) {
                vfs_node* curr = vfs_root;
                while (curr) {
                    terminal_writestring(curr->name);
                    terminal_writestring("  ");
                    curr = curr->next;
                }
                terminal_writestring("\n");
            } 
            else if (strncmp(cmd_buffer, "cat ", 4) == 0) {
                const char* filename = cmd_buffer + 4;
                vfs_node* file = vfs_find(filename);
                if (file) {
                    uint8_t buf[1024];
                    uint32_t n = file->read(file, 0, file->size, buf);
                    for(uint32_t i=0; i<n; i++) terminal_writestring((char[]){(char)buf[i], '\0'});
                    terminal_writestring("\n");
                } else {
                    terminal_writestring("File not found.\n");
                }
            }
            else if (strcmp(cmd_buffer, "help") == 0) {
                terminal_writestring("Dostepne komendy: ls, cat <file>, help, clear\n");
            }

            // Reset bufora
            cmd_index = 0;
            line_ready = false;
            terminal_writestring("AMS-1 Shell> ");
        }
        
        // Tutaj procesor może odpocząć, żeby nie palić 100% CPU w QEMU
        //asm volatile("hlt"); 
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
    write_serial_string("AMSx64 v04.5 booting...\n");
    // 1. Pamięć najpierw (żeby stos i IDT miały gdzie żyć)
    pmm_init(128 * 1024 * 1024, (void*)0x200000);
    parse_multiboot(multiboot_info_address);

    //Oznaczenie pamięci jądra i bitmapy jako zajętej (pierwsze 4MB)
    pmm_mark_used(0x0, 0x400000);
    
    // 2. IDT (teraz bezpiecznie)
    idt_init();

    /*write_serial_string("Inicjalizacja zadań...\n");
    create_task(nullptr);      // Zadanie 1 (jądro)
    create_task(second_task);  // Zadanie 2
    pit_init(100); // 100 Hz timer
    */
    write_serial_string("Włączam przerwania (sti)...\n");
    asm volatile("sti");

    write_serial_string("Inicjalizacja VFS...\n");
    vfs_init(); // Buduje strukturę plików
    write_serial_string("Sprawdzam plik hello.txt w VFS...\n");

    vfs_node* file = vfs_find("hello.txt");
    if (file) {
        uint8_t buffer[64];
        file->read(file, 0, 63, buffer);
    
        // Piszemy bezpośrednio do pamięci VGA (drugi wiersz, żeby nie zniknęło)
        uint16_t* vga = (uint16_t*)0xB8000 + 80; 
        for(int i = 0; buffer[i] != '\0' && i < 64; i++) {
            vga[i] = (uint16_t)buffer[i] | (uint16_t)(0x0F << 8);
        }
    }
    
    terminal_writestring("System started and multitasking active!\n");
    //vga_matrix_effect();
    keyboard_init();
    run_shell();


    while(1) {
        //write_serial_string("A");
        for(volatile int i = 0; i < 1000000; i++);
    }
}