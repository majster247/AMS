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



extern "C" void kmain(uint64_t multiboot_info_address) {
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

    //Mapowanie Framebuffera do przestrzeni wirtualnej
    for (uint64_t i = 0; i < (fb.width * fb.height * 4); i += 4096) {
        vmm_map(fb.address + i, fb.address + i, PAGE_PRESENT | PAGE_WRITABLE);
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

    log_step("Włączam przerwania (sti)");
    asm volatile("sti");

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
    terminal_writestring("Welcome to AMS OS!\n");
    shell_init();
    log_step("System gotowy. Wchodzę w pętlę główną.");

    //Test rysowania paska stanu
    log_step("Rysuję pasek stanu na ekranie oraz wallpaper");
    graphics_clear_screen(0x1D1D1D); // Ciemnoszary "pulpit"
    graphics_draw_bmp_centered();
    draw_status_bar();
    log_step("Pasek stanu i wallpaper narysowane.");

    // najpierw koloruj pulpit, potem rysuj wallpaper, potem pasek stanu na wierzch bo to warstwami idzie nygus

    while(1) {
        shell_update();
        shell_update_remote();
        asm volatile("hlt"); // Opcjonalnie: zatrzymaj procesor do następnego przerwania (oszczędza CPU)
    }
}