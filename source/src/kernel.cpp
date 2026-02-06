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




// Zewnętrzne inicjalizatory
extern "C" void pci_init(); 
extern uint64_t initrd_addr;
extern ahci_port* sata_port; // Globalny wskaźnik na pierwszy znaleziony port SATA (do testów)
extern uint64_t bitmap_size; // Informujemy linker, że to jest w pmm.cpp

Desktop* desktop = nullptr;

void log_step(const char* msg, uint64_t addr = 0) {
    write_serial_string("[KERNEL] ");
    write_serial_string(msg);
    if (addr != 0) {
        write_serial_string(" -> Adres: ");
        write_serial_hex(addr);
    }
    write_serial_string("\n");
}


//boot time measurement
uint64_t boot_start_cycles = 0;
uint64_t boot_end_cycles = 0;

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    // Instrukcja rdtsc wczytuje licznik do rejestrów EDX:EAX
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

void task_zegar() {
    while(1) {
        //write_serial_string("TICK! "); // Jeśli to zobaczysz w terminalu, scheduler żyje
        
        update_clock_display();
        sleep(100); // Śpij na 100 ticków (1 sekunda przy 100Hz)
    }
}

extern "C" void kmain(uint64_t multiboot_info_address) {
    boot_start_cycles = rdtsc(); // Zapisujemy start
    init_serial();
    timer_init(100); // 100 Hz
    log_step("=== AMS OS x64 Startuje ===");
    log_step("Multiboot Info", multiboot_info_address);

    terminal_initialize();
    terminal_writestring("AMS OS x64 Booting...\n");

    // 1. Pamięć Fizyczna
    pmm_init(128 * 1024 * 1024, (void*)0x200000); // 128MB RAM, bitmapa pod 2MB
    log_step("PMM Zainicjalizowany");

    // 2. Parsowanie Multiboota (pobieranie mapy RAM i modułów)
    parse_multiboot(multiboot_info_address);
    log_step("Multiboot sparsowany, Initrd pod", initrd_addr);

    // 3. Rezerwacja pamięci jądra
    pmm_mark_used(0x0, 0x1000000); // Rezerwujemy pierwsze 16MB dla jądra i urządzeń
    log_step("Pamięć jądra zarezerwowana (0-16MB)");

    
    // 4. REZERWACJA BITMAPY I MODUŁÓW
    log_step("Rezerwacja bitmapy PMM i modułów (initrd)");
    // Musisz też oznaczyć adres bitmapy (0x200000) jako zajęty, żeby PMM sam siebie nie nadpisał!
    pmm_mark_used(0x200000, bitmap_size);
    pmm_mark_used(initrd_addr, 0x100000); // Przykładowe 1MB na initrd


    //VMM - mapowanie całej pamięci fizycznej do wyższego połowy
    log_step("Inicjalizacja VMM i mapowanie całej pamięci do Higher Half");
    vmm_init_direct_map(128); // Mapujemy 128MB RAM (lub mniej, jeśli masz mniej)
    log_step("VMM Zainicjalizowany, cała pamięć zmapowana do Higher Half (128MB)");

    // Sterta (Heap)
    heap_init((void*)0x40000000, 128 * 1024 * 1024);
    log_step("Sterta (Heap) Zainicjalizowana");


    //Mapowanie Framebuffera do przestrzeni wirtualnej
    //uint64_t fb_virtual_base = 0xFFFF900000000000; // Wybierz jakiś wolny adres w High Memory
    for (uint64_t i = 0; i < (1280 * 720 * 4); i += 4096) {
        vmm_map(fb.address + i, fb.address + i, PAGE_PRESENT | PAGE_WRITABLE | (1 << 3) | (1 << 4));
    }
    log_step("Framebuffer zmapowany do przestrzeni wirtualnej");

    if (fb.address != 0) {
        write_serial_string("Framebuffer found at: ");
        write_serial_hex(fb.address);
        write_serial_string("\n");
    } else {
        log_step("Framebuffer NOT found in Multiboot tags!\n");
    }


    // 4. Przerwania
    idt_init();
    keyboard_init();
    log_step("IDT i Klawiatura gotowe");

    // taski i scheduler
    task* main_t = create_task(nullptr);
    main_t->state = STATE_RUNNING;
    current_task = main_t; // Ustawiamy jako aktywne

    create_task(task_zegar);

    write_serial_string("[KERNEL] Wielozadaniowosc gotowa.\n");
    list_tasks();

    // 5. System Plików VFS (TarFS na start)
    vfs_init();
    log_step("VFS Zainicjalizowany (TarFS)");

    // 6. Magistrala PCI i Sterowniki (Tu znajdziemy SATA/AHCI)
    log_step("Rozpoczynam skanowanie PCI...");
    pci_init(); 

    log_step("Skanowanie PCI zakończone");
    // Test odczytu z dysku SATA (jeśli znaleziono port)
    log_step("Test odczytu z dysku SATA (jeśli dostępny)");
    
    void* test_buffer_phys = pmm_alloc_frame(); // Dostajesz adres fizyczny, np. 0x500000
    ahci_read(sata_port, 0, 1, (uint16_t*)test_buffer_phys);
    // Teraz możesz zmapować ten adres fizyczny do przestrzeni wirtualnej, jeśli chcesz go odczytać w kernelu
    vmm_map(0xFFFF800000000000, (uint64_t)test_buffer_phys, PAGE_PRESENT | PAGE_WRITABLE);
    write_serial_string("[KERNEL] Odczytano sektor 0 z SATA! Zawartość (pierwsze 16 bajtów): ");
    uint16_t* disk_data = (uint16_t*)0xFFFF800000000000;
    char* text_data = (char*)disk_data;
    for(int i=0; i<32; i++) {
        write_serial_char(text_data[i]);
    }
    write_serial_string("\n");

    if (sata_port != nullptr) {
    uint8_t* disk_buffer = (uint8_t*)kmalloc(512); // rzutujemy na bajty
    if (ahci_read(sata_port, 0, 1, (uint16_t*)disk_buffer)) {
        write_serial_string("[KERNEL] Odczytano tekst z SATA: ");
        for(int i = 0; i < 32; i++) {
            // Wypisujemy znak po znaku (zakładając, że masz funkcję serial_putc)
            // Jeśli nie masz, użyj tymczasowo write_serial_string z pojedynczym znakiem
            char c = (char)disk_buffer[i];
            if(c >= 32 && c <= 126) { // tylko znaki drukowalne
                write_serial_char(c);
            }
        }
        write_serial_string("\n");
    }
    }else {
        write_serial_string("[KERNEL] Brak dostępnego portu SATA do testu odczytu.\n");
    }

    //Test zapisu na dysk SATA (jeśli znaleziono port)
    log_step("Test zapisu na dysku SATA");

    if (sata_port != nullptr) {
        const char* test_data = "AMS OS DISK WRITE TEST OK";
        uint8_t* write_buf = (uint8_t*)kmalloc(512);
        memset(write_buf, 0, 512);
        memcpy(write_buf, test_data, 26);

        // Próbujemy zapisać na sektorze 10 (bezpieczny dystans od MBR)
        if (ahci_write(sata_port, 10, 1, (uint16_t*)write_buf)) {
            write_serial_string("[KERNEL] Sukces: Zapisano sektor 10!\n");

            // Teraz odczytujemy to samo dla weryfikacji
            uint8_t* read_buf = (uint8_t*)kmalloc(512);
            if (ahci_read(sata_port, 10, 1, (uint16_t*)read_buf)) {
                write_serial_string("[KERNEL] Weryfikacja zapisu: ");
                write_serial_string((char*)read_buf);
                write_serial_string("\n");
            }
        } else {
            write_serial_string("[KERNEL] BLAD: ahci_write zwrocilo false!\n");
        }
    } else {
        write_serial_string("[KERNEL] Pominieto: sata_port jest NULL\n");
    }

    // 6. EXT2 FS na SATA
    log_step("Inicjalizacja EXT2 na dysku SATA");
    ext2_init(sata_port);
    log_step("EXT2 Zainicjalizowany i podpięty do VFS");

    // 7. Shell
    log_step("Uruchamiam Shell użytkownika");
    //terminal_writestring("Welcome to AMS OS!\n");
    //shell_init();
    log_step("System gotowy. Wchodzę w pętlę główną.");
    
    graphics_init_double_buffer();
    uint64_t bbc_addr = (uint64_t)backbuffer;
    for (uint64_t i = 0; i < (1280 * 720 * 4); i += 4096) {
        vmm_map(bbc_addr + i, vmm_get_phys(bbc_addr + i), PAGE_PRESENT | PAGE_WRITABLE);
    }
    write_serial_string("[KERNEL] Backbuffer zmapowany pomyślnie.\n");
    
   // === INIT GUI ===
   write_serial_string("[KERNEL] Inicjalizacja GUI...\n");
    desktop = new Desktop();
    desktop->Init();
    write_serial_string("[KERNEL] GUI Zainicjalizowane.\n");
    write_serial_string("[KERNEL] Dodaję okno terminala powitalnego...\n");
    // Dodajemy okno powitalne
    TerminalWindow* term = new TerminalWindow(100, 100);
    desktop->AddWindow(term);
    write_serial_string("[KERNEL] Okno terminala dodane.\n");
    
    // === DIAGNOSTYKA PAMIĘCI ===
    extern uint32_t* backbuffer;
    uint64_t bb_addr = (uint64_t)backbuffer;
    uint64_t term_addr = (uint64_t)term;
    uint64_t bb_end = bb_addr + (1280*720*4);
    
    write_serial_string("--- DEBUG PAMIECI ---\n");
    
    // Wypisz adres Backbuffera (Start)
    write_serial_string("Backbuffer Start: ");
    write_serial_hex(bb_addr);
    write_serial_string("\n");
    
    // Wypisz adres Backbuffera (Koniec)
    write_serial_string("Backbuffer End:   ");
    write_serial_hex(bb_end);
    write_serial_string("\n");

    // Wypisz adres Okna
    write_serial_string("Window Object:    ");
    write_serial_hex(term_addr);
    write_serial_string("\n");
    
    if (term_addr >= bb_addr && term_addr < bb_end) {
        write_serial_string("FATAL ERROR: Okno zaalokowane WEWNATRZ backbuffera!\n");
        // Tu jest problem z kmalloc
    } else {
        write_serial_string("Pamiec OK. Obiekt poza buforem.\n");
    }
    

    write_serial_string("[KERNEL] Inicjalizacja myszy...\n");
    mouse_init();
    log_step("Mysz zainicjalizowana");
    log_step("Wszystkie systemy gotowe, włączam przerywania i wchodzę w pętlę główną.");
    asm volatile("sti");
    log_step("Przerwania włączone. System działa.");

     // --------- Boot time measurement ---------
    boot_end_cycles = rdtsc();
    uint64_t boot_cycles = boot_end_cycles - boot_start_cycles;
    write_serial_string("[KERNEL] Czas bootowania (w cyklach): ");
    write_serial_dec(boot_cycles);
    write_serial_string("\n");
    write_serial_string("[KERNEL] Czas bootowania (w sekundach, przy 3GHz): ");
    double boot_seconds = boot_cycles / 3000000000.0;
    char time_str[32]; // bez sprintf, więc ręcznie formatujemy
    int time_len = 0;
    int int_part = (int)boot_seconds;
    int frac_part = (int)((boot_seconds - int_part) * 1000); // 3 miejsca po przecinku
    // Część całkowita
    char int_buffer[12];
    int int_len = 0;
    do {
        int_buffer[int_len++] = '0' + (int_part % 10);
        int_part /= 10;
    } while (int_part > 0);
    for (int i = int_len - 1; i >= 0; i--) {
        time_str[time_len++] = int_buffer[i];
    }
    time_str[time_len++] = '.';
    // Część ułamkowa
    char frac_buffer[4];
    frac_buffer[3] = 0; // null terminator
    for (int i = 2; i >= 0; i--) {
        frac_buffer[i] = '0' + (frac_part % 10);
        frac_part /= 10;
    }
    for (int i = 0; i < 3; i++) {
        time_str[time_len++] = frac_buffer[i];
    }
    time_str[time_len] = 0; // null terminator
    write_serial_string(time_str);
    write_serial_string(" s\n");
    list_tasks(); // Wyświetl listę zadań na starcie

    // ---------------------------------------

    // Zmienne do myszy
    int32_t old_mx = mouse_x, old_my = mouse_y;
    save_background(mouse_x, mouse_y);

    while(1) {
        // 1. Mysz
        int mx = mouse_x;
        int my = mouse_y;
        bool lmb = mouse_left_pressed;
        
        // 2. Klawiatura - TO JEST NOWA CZĘŚĆ
        // Pobieramy wszystkie znaki, które nazbierały się w buforze
        char c;
        while ((c = keyboard_getchar()) != 0) {
            // Wysyłamy znak do okna, które jest aktualnie fokusowane
            if (desktop) {
                desktop->OnKeyboard(c);
            }
        }

        // 3. Update & Draw
        if (desktop) {
            desktop->Update(mx, my, lmb);
            
            restore_background(old_mx, old_my);
            desktop->Draw();
            save_background(mx, my);
            draw_cursor_shape(mx, my);
            
            graphics_flip();
        }
        
        old_mx = mx; old_my = my;
    }

}