#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"
#include "tar.h"
#include "vfs.h"
#include "shell.h"
#include "pci.h"
#include "ahci.h"
#include "ext2.h"
#include "idt.h"
#include "graphics.h"
#include "mouse.h"
#include "gui.h"
#include "gdt.h"

// Zewnętrzne inicjalizatory
extern "C" void pci_init(); 
extern uint64_t initrd_addr;
extern ahci_port* sata_port;
extern uint64_t bitmap_size;
static uint8_t pmm_bitmap_buffer[132000];
extern "C" void vmm_set_nocache(uint64_t virt);
// UWAGA: jump_to_ring3 już nie używamy bezpośrednio w kmain!

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} __attribute__((packed));

extern "C" void gdt_init();
extern "C" void syscall_init();
extern "C" void vmm_map_user(uint64_t virt, uint64_t phys, bool writable);

Desktop* desktop = nullptr;

// Helper do logowania
void log_step(const char* msg, uint64_t addr = 0) {
    write_serial_string("[KERNEL] ");
    write_serial_string(msg);
    if (addr != 0) {
        write_serial_string(" -> Adres: ");
        write_serial_hex(addr);
    }
    write_serial_string("\n");
}

// Funkcja ładująca proces do Schedulera (nie blokuje kernela!)
void exec(const char* filename) {
    log_step("Proba uruchomienia procesu (Exec): ");
    write_serial_string(filename); write_serial_string("\n");

    vfs_node* elf_file = vfs_find(filename);
    if (!elf_file) {
        log_step("BLAD: Nie znaleziono pliku.");
        return;
    }

    Elf64_Ehdr ehdr;
    elf_file->read(elf_file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdr);

    if (ehdr.e_ident[0] == 0x7F && ehdr.e_ident[1] == 'E' && ehdr.e_ident[2] == 'L' && ehdr.e_ident[3] == 'F') {
        // 1. Mapowanie segmentów
        for (int i = 0; i < ehdr.e_phnum; i++) {
            Elf64_Phdr phdr;
            elf_file->read(elf_file, ehdr.e_phoff + (i * ehdr.e_phentsize), sizeof(Elf64_Phdr), (uint8_t*)&phdr);
            
            if (phdr.p_type == 1) { // PT_LOAD
                uint64_t pages = (phdr.p_memsz + 4095) / 4096;
                for (uint64_t j = 0; j < pages; j++) {
                    uint64_t phys = (uint64_t)pmm_alloc_frame();
                    // Mapuj jako USER (Ring 3 access)
                    vmm_map_user(phdr.p_vaddr + (j * 4096), phys, true);
                }
                // Wczytaj dane
                elf_file->read(elf_file, phdr.p_offset, phdr.p_filesz, (uint8_t*)phdr.p_vaddr);
                
                // Zeruj BSS
                if (phdr.p_memsz > phdr.p_filesz) {
                    uint64_t bss_size = phdr.p_memsz - phdr.p_filesz;
                    uint64_t bss_start = phdr.p_vaddr + phdr.p_filesz;
                    memset((void*)bss_start, 0, bss_size);
                }
            }
        }

        // 2. Alokacja stosu użytkownika
        uint64_t u_stack_phys = (uint64_t)pmm_alloc_frame();
        uint64_t u_stack_virt = 0x7FFFFFFFF000; // Wysoki adres wirtualny
        vmm_map_user(u_stack_virt, u_stack_phys, true);

        // 3. Dodanie do Schedulera zamiast skoku!
        // Stos rośnie w dół, więc podajemy koniec strony
        scheduler_add_user_task((void*)ehdr.e_entry, (void*)(u_stack_virt + 4096));
        
        log_step("Proces dodany do kolejki Schedulera.");
    } else {
        log_step("BLAD: To nie jest plik ELF.");
    }
}

// Pomiar czasu bootowania
uint64_t boot_start_cycles = 0;
static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

// Zadanie "Idle" / Zegarowe (Opcjonalne, jeśli GUI jest w kmain)
void task_zegar() {
    // To zadanie może np. aktualizować zegar na pasku co sekundę
    while(1) {
        update_clock_display();
        sleep(100); 
    }
}

extern "C" void kmain(uint64_t multiboot_info_address) {
    boot_start_cycles = rdtsc();
    init_serial();
    
    // Inicjalizacja Timera (PIT) - to on napędza Scheduler!
    timer_init(100); 

    terminal_initialize();
    terminal_writestring("AMS OS x64 Booting...\n");

    // --- Pamięć ---
    pmm_init(4096, (void*)pmm_bitmap_buffer);
    parse_multiboot(multiboot_info_address);
    vmm_init_direct_map(0);
    
    extern uint64_t initrd_end; 
    uint64_t heap_start = 0x2000000; 
    if (initrd_end > heap_start) heap_start = (initrd_end + 0x1FFFFF) & ~0x1FFFFFULL;
    heap_init((void*)heap_start, 1024*1024*1024);

    // --- Framebuffer ---
    if (fb.address != 0) {
        // Mapowanie framebuffera
        for (uint64_t i = 0; i < (1280 * 720 * 4); i += 4096) {
            vmm_map(fb.address + i, fb.address + i, PAGE_PRESENT | PAGE_WRITABLE | (1 << 3) | (1 << 4));
        }
        vmm_set_nocache(fb.address);
        vmm_set_nocache(fb.address + PHYSICAL_MEM_OFFSET);
    }

    // --- Przerwania i GDT ---
    idt_init();
    keyboard_init();
    gdt_init();     // Ring 3 ready
    syscall_init(); // Syscall ready

    // --- Start Schedulera ---
    // Tworzymy zadanie dla obecnego wątku (Kernel Main / GUI)
    // Dzięki temu, gdy timer przerwie kmain, będzie miał gdzie wrócić.
    task* main_task = create_task(nullptr); 
    main_task->state = STATE_RUNNING;
    current_task = main_task;

    // Dodatkowe zadanie w tle (np. zegar)
    // create_task(task_zegar);

    // Włączamy przerwania - od teraz timer (IRQ0) zacznie przełączać zadania!
    asm volatile("sti");
    write_serial_string("[KERNEL] Przerwania wlaczone, Scheduler aktywny.\n");

    // --- Sterowniki i Pliki ---
    vfs_init(); // TarFS
    pci_init(); // Znajdzie AHCI
    
    if (sata_port) {
        if (ext2_init(sata_port)) {
            log_step("EXT2 OK. Uruchamiam programy startowe...");
            // URUCHAMIAMY PROGRAM Z RING 3
            exec("hello.elf"); 
        }
    }

    // --- GUI Init ---
    graphics_init_double_buffer();
    // Mapowanie backbuffera
    extern uint32_t* backbuffer;
    uint64_t bbc_addr = (uint64_t)backbuffer;
    for (uint64_t i = 0; i < (1280 * 720 * 4); i += 4096) {
        vmm_map(bbc_addr + i, vmm_get_phys(bbc_addr + i), PAGE_PRESENT | PAGE_WRITABLE);
    }

    desktop = new Desktop();
    desktop->Init();
    
    TerminalWindow* term = new TerminalWindow(100, 100);
    desktop->AddWindow(term);

    mouse_init();

    log_step("AMS-OS gotowy. GUI aktywne.");

    // --- GŁÓWNA PĘTLA SYSTEMU (IDLE LOOP / GUI THREAD) ---
    // Ponieważ kmain jest zarejestrowany jako zadanie, 
    // Scheduler będzie tu wracał co kwant czasu.
    int old_mx = 0, old_my = 0;

    while(1) {
        // 1. Obsługa wejścia
        int mx = mouse_x;
        int my = mouse_y;
        bool lmb = mouse_left_pressed;
        
        char c;
        while ((c = keyboard_get_char()) != 0) {
            if (desktop) desktop->HandleKeyboard(c);
        }

        // 2. Logika GUI
        if (desktop) {
            desktop->Update(mx, my, lmb);
            
            // Rysowanie (w podwójnym buforowaniu)
            // Restore background -> Draw Desktop -> Save Background -> Draw Cursor
            restore_background(old_mx, old_my); 
            desktop->Draw();
            save_background(mx, my);
            draw_cursor_shape(mx, my);
            
            // Kopiuj na ekran
            graphics_flip();
        }
        
        old_mx = mx; old_my = my;

        // Oszczędzanie CPU (czeka na przerwanie)
        // Scheduler przerwie to instrukcją 'hlt' i przełączy na hello.elf, a potem wróci tutaj.
        // asm volatile("hlt"); // Odkomentuj jeśli migotanie jest za szybkie
        asm volatile("pause"); // TODO: naprawić handler myszki i klawiatury po wprowadzeniu poprawek związanych z ring 3
    
    }
}