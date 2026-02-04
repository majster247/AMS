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
#include "window.h"




// Zewnętrzne inicjalizatory
extern "C" void pci_init(); 
extern uint64_t initrd_addr;
extern ahci_port* sata_port; // Globalny wskaźnik na pierwszy znaleziony port SATA (do testów)
extern uint64_t bitmap_size; // Informujemy linker, że to jest w pmm.cpp


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
    heap_init((void*)0x40000000, 0x1000000); // 16MB na stertę zamiast 1MB
    log_step("Sterta (Heap) Zainicjalizowana");


    //Mapowanie Framebuffera do przestrzeni wirtualnej
    uint64_t fb_virtual_base = 0xFFFF900000000000; // Wybierz jakiś wolny adres w High Memory
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

    write_serial_string("Test zapisu do bufora...");
    backbuffer[0] = 0x12345678;
    write_serial_string(" OK\n");
    
    write_serial_string("Test flipa...");
    graphics_flip();
    write_serial_string(" OK\n");

    graphics_init_double_buffer();
    uint64_t bb_addr = (uint64_t)backbuffer;
    for (uint64_t i = 0; i < (1280 * 720 * 4); i += 4096) {
        vmm_map(bb_addr + i, vmm_get_phys(bb_addr + i), PAGE_PRESENT | PAGE_WRITABLE);
    }
    write_serial_string("[KERNEL] Backbuffer zmapowany pomyślnie.\n");

    //Test rysowania paska stanu
    log_step("Rysuję pasek stanu na ekranie oraz wallpaper");
    graphics_clear_screen(0x1D1D1D); // Ciemnoszary "pulpit"
    graphics_draw_bmp_centered();
    draw_status_bar();
    log_step("Pasek stanu i wallpaper narysowane.");
    // najpierw koloruj pulpit, potem rysuj wallpaper, potem pasek stanu na wierzch bo to warstwami idzie nygus

    mouse_init(); 
    log_step("[KERNEL] Myszka zainicjalizowana.\n");
    update_mouse_on_screen();

     // Włączamy przerwania
    log_step("Włączam przerwania (sti)");
    asm volatile("sti");


    
    // --------- Boot time measurement ---------
    boot_end_cycles = rdtsc();   // System gotowy, zapisujemy koniec
    uint64_t boot_cycles = boot_end_cycles - boot_start_cycles;
    write_serial_string("[KERNEL] Czas rozruchu (w cyklach): ");
    write_serial_hex(boot_cycles);
    //konwersja na sekundy (przy 3GHz):
    double boot_seconds = (double)boot_cycles / 3000000000.0;
    write_serial_string(" (~");
    char boot_time_str[32];
    // Prosta konwersja float -> string (bez sprintf)
    int len = 0;
    int int_part = (int)boot_seconds;
    double frac_part = boot_seconds - int_part;
    // Integer part
    if (int_part == 0) {
        boot_time_str[len++] = '0';
    } else {
        char int_buf[16];
        int int_len = 0;
        while (int_part > 0) {
            int_buf[int_len++] = '0' + (int_part % 10);
            int_part /= 10;
        }
        for (int i = int_len - 1; i >= 0; i--) {
            boot_time_str[len++] = int_buf[i];
        }
    }
    boot_time_str[len++] = '.';
    // Fractional part (2 decimal places)
    frac_part *= 100;
    int frac_int = (int)frac_part;
    if (frac_int < 10) {
        boot_time_str[len++] = '0';
    }
    char frac_buf[16];
    int frac_len = 0;
    while (frac_int > 0) {
        frac_buf[frac_len++] = '0' + (frac_int % 10);
        frac_int /= 10;
    }
    for (int i = frac_len - 1; i >= 0; i--) {
        boot_time_str[len++] = frac_buf[i];
    }
    boot_time_str[len] = '\0';
    write_serial_string(boot_time_str);
    write_serial_string(" seconds)\n");

    // ---------------------------------------

    list_tasks(); // Wyświetl listę zadań na starcie
    
    // Test okna GUI
    Window test_win;
    test_win.x = 100;
    test_win.y = 100;
    test_win.width = 300;
    test_win.height = 200;
    test_win.color = 0x007ACC; // Niebieski
    const char* title = "AMS-OS Test Window";
    test_win.title = title;
    test_win.is_dragging = false;
    draw_window(&test_win);

    int32_t old_win_x = test_win.x;
    int32_t old_win_y = test_win.y;
    int32_t last_mouse_x = 0, last_mouse_y = 0;




// Rysujemy tło RAZ przed pętlą
graphics_draw_bmp_centered();

while(1) {
    // 1. NIE RYSUJEMY TAPETY CO KLATKĘ! 
    // Zamiast tego czyścimy tylko obszar pod starym kursorem i zegarem
    // Na razie dla testu: zakomentuj tapetę w pętli.
    
    // 2. Rysuj okna i pasek (lekkie operacje)
    draw_window(&test_win);
    draw_status_bar();


    // 3. Myszka    
    update_mouse_on_screen(); 

    // 4. Kopiowanie na ekran
    graphics_flip();

    // 5. WAŻNE: Pozwól schedulerowi działać
    // Jeśli nie używasz wielozadaniowości wywłaszczającej, musisz oddać czas:
    // yield(); 
}

}