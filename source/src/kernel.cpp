#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"
#include "tar.h"
#include "vfs.h"
#include "shell.h"
#include "pci.h"
#include "ahci.h"




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

    if (sata_port) {
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
    }

    //Test zapisu na dysk SATA (jeśli znaleziono port)
    log_step("Test zapisu na dysku SATA (jeśli dostępny)");

    const char* msg = "AMS OS TEST ZAPISU";
    uint8_t* my_buffer = (uint8_t*)kmalloc(512);
    memset(my_buffer, 0, 512);
    memcpy(my_buffer, msg, 18);

    // 1. Zapisujemy na sektorze 10 (bezpiecznie poza MBR)
    if(ahci_write(sata_port, 10, 1, (uint16_t*)my_buffer)) {
        write_serial_string("[KERNEL] Zapisano sektor 10!\n");
    }

    // 2. Czyścimy bufor, żeby mieć pewność, że czytamy z dysku, a nie z RAMu
    memset(my_buffer, 0, 512);

    // 3. Czytamy z powrotem
    if(ahci_read(sata_port, 10, 1, (uint16_t*)my_buffer)) {
        write_serial_string("[KERNEL] Odczyt po zapisie: ");
        write_serial_string((char*)my_buffer);
        write_serial_string("\n");
    }

    


    // 7. Shell
    log_step("Uruchamiam Shell użytkownika");
    terminal_writestring("Welcome to AMS OS!\n");
    shell_init();

    while(1) {
        shell_update();
    }
}