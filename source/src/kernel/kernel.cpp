#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"
#include "vfs.h"
#include "pci.h"
#include "ext2.h"
#include "idt.h"
#include "graphics.h"
#include "mouse.h"
#include "gui.h"
#include "elf.h"

// Zewnętrzne inicjalizatory
extern "C" void pci_init(); 
extern ahci_port* sata_port;
static uint8_t pmm_bitmap_buffer[132000]; // 1MB bitmapy dla PMM

extern "C" void gdt_init();
extern "C" void syscall_init();

extern "C" void switch_to_kernel_stack(void* new_stack, void (*func)());
extern void elf_load_file(const char* name);

// GUI
Desktop* main_desktop = nullptr;

extern "C" int sys_exec(const char* path, int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    write_serial_string("[SYSCALL] EXEC called with path: ");
    write_serial_string(path);
    write_serial_string("\n");
    for(int i=0; i<argc; i++) {
        write_serial_string("  argv[");
        char idx_str[10];
        // Prosta konwersja int -> string
        int idx = 0, temp = i;
        if (temp == 0) idx_str[idx++] = '0';
        else {
            char rev[10];
            int rev_idx = 0;
            while (temp > 0) {
                rev[rev_idx++] = '0' + (temp % 10);
                temp /= 10;
            }
            // Odwróć
            while (rev_idx > 0) idx_str[idx++] = rev[--rev_idx];
        }
        idx_str[idx] = 0;
        write_serial_string(idx_str);
        write_serial_string("]: ");
        write_serial_string(argv[i]);
        write_serial_string("\n");

    }
    
    return 0; // Sukces
}

extern "C" void enable_sse() {
    uint64_t cr0;
    uint64_t cr4;

    // 1. Ustawienie CR0
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2);  // Wyczyść bit EM (Emulation)
    cr0 |= (1 << 1);   // Ustaw bit MP (Monitor Coprocessor)
    asm volatile ("mov %0, %%cr0" :: "r"(cr0));

    // 2. Ustawienie CR4
    asm volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);   // Ustaw bit OSFXSR (OS Support for FXSAVE and FXRSTOR)
    cr4 |= (1 << 10);  // Ustaw bit OSXMMEXCPT (OS Support for Unmasked SIMD Floating-Point Exceptions)
    asm volatile ("mov %0, %%cr4" :: "r"(cr4));
}

// Helper do logowania
void log_step(const char* msg) {
    write_serial_string("[KERNEL] ");
    write_serial_string(msg);
    write_serial_string("\n");
}

void kmain_post_stack_switch() {
    asm volatile("sti"); // Włącz przerwania - TERAZ TIMER ZACZNIE BIĆ!
    log_step("GUI Loop starting...");

    //if (elf_load("hello.elf")) {
        //log_step("ELF zaladowany i czeka w kolejce.");
    //}

    while(true) {
        // GUI kod...
        main_desktop->Update(mouse_x, mouse_y, mouse_left_pressed);
        main_desktop->Draw();
        mouse_draw();
        graphics_flip();
        
        // Zamiast hlt, możemy wymusić oddanie czasu procesora:
        asm volatile("int $0x20"); // Załóżmy, że 0x20 to Twoje przerwanie timera/schedulera
    }
}

extern "C" void kmain(uint64_t multiboot_info_address) {
    init_serial();
    log_step("BOOT: Serial Port Inicjalizowany.");

    timer_init(100); // 100Hz
    log_step("BOOT: Timer ustawiony na 100Hz.");

    // 1. Pamięć i System Podstawowy
    pmm_init(4096, (void*)pmm_bitmap_buffer);
    parse_multiboot(multiboot_info_address);
    vmm_init_direct_map(0); 
    
    extern uint64_t initrd_end; 
    uint64_t heap_start = 0x2000000; 
    if (initrd_end > heap_start) heap_start = (initrd_end + 0x1FFFFF) & ~0x1FFFFFULL;
    heap_init((void*)heap_start, 1024*1024*1024); 
    log_step("BOOT: Pamiec i Sterta OK.");

    // 2. Sprzęt Wejściowy (Mysz/Klawiatura)
    // UWAGA: To zazwyczaj są urządzenia PS/2 (nie PCI), obsługiwane przez ISA Bridge.
    log_step("BOOT: Inicjalizacja Klawiatury...");
    keyboard_init();
    
    log_step("BOOT: Inicjalizacja Myszy...");
    mouse_init();
    log_step("BOOT: Mysz/Klawiatura Init zakonczone.");

    // 3. Inicjalizacja Tablic Systemowych
    gdt_init();     
    syscall_init();
    enable_sse();
    
    scheduler_init_kernel_task();

    // 4. Scheduler
    //task* main_task = create_task(nullptr); 
    //main_task->state = STATE_RUNNING;
    //current_task = main_task;
    //log_step("BOOT: Scheduler przygotowany.");

    // 5. Włączenie Przerwań
    idt_init(); // bez STI, bo chcemy mieć kontrolę nad momentem włączenia
    log_step("BOOT: Przerwania wlaczone (STI). Czekam na ticki...");

    // 6. System Plików i Sterowniki
    vfs_init();
    pci_init(); // Standardowe init
    //debug_pci_scan(); // Twoje logi PCI
    
    if (sata_port && ext2_init(sata_port)) {
        log_step("BOOT: System plikow EXT2 zamontowany.");
    } else {
        log_step("BOOT: OSTRZEZENIE - Brak dysku lub EXT2.");
    }
    // 7. Grafika
    graphics_init_double_buffer();
    log_step("BOOT: Backbuffer zaalokowany.");

    main_desktop = new Desktop();
    main_desktop->Init();
    main_desktop->AddWindow(new TerminalWindow(100, 100));

    log_step("BOOT: GUI Loop Start. Jeśli po tym nic nie ma - system wisi.");

    asm volatile("cli");

    write_serial_string("[KERNEL] Switching stack...\n");

    // Skok (z wyłączonymi przerwaniami)
    switch_to_kernel_stack((void*)current_task->kstack_top, kmain_post_stack_switch);

    while(1);
}