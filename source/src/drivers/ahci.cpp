#include "ahci.h"
#include "kernel.h"
#include "vmm.h"
#include "heap.h"

// DEFINICJA: Linker teraz znajdzie to miejsce w pamięci
ahci_port* sata_port = nullptr;


void ahci_rebase_port(ahci_port* port, int /* port_no */) {
    // Alokujemy ramki dla list komend i FIS-ów
    // Muszą być wyrównane do 4KB, co pmm_alloc_frame zapewnia
    void* clb_phys = pmm_alloc_frame();
    void* fb_phys = pmm_alloc_frame();
    void* ctba_phys = pmm_alloc_frame();

    // AHCI używa adresów fizycznych. W 64-bitach musimy rozbić je na Low i High
    port->clb = (uint32_t)(uintptr_t)clb_phys;
    port->clbu = (uint32_t)((uintptr_t)clb_phys >> 32);
    
    port->fb = (uint32_t)(uintptr_t)fb_phys;
    port->fbu = (uint32_t)((uintptr_t)fb_phys >> 32);

    // Nagłówek komendy 0 wskazuje na tablicę komend (Command Table)
    // Mapujemy clb_phys, aby móc po nim pisać w jądrze
    ahci_hba_cmd_header* cmd_header = (ahci_hba_cmd_header*)clb_phys;
    
    // Czyścimy listę komend (32 sloty)
    for (int i = 0; i < 32; i++) {
        cmd_header[i].prdtl = 8; // Ilość wpisów PRDT
        cmd_header[i].ctba = (uint32_t)(uintptr_t)ctba_phys;
        cmd_header[i].ctbau = (uint32_t)((uintptr_t)ctba_phys >> 32);
    }

    // Włączamy port (ST i FRE bits)
    while (port->cmd & (1 << 15)); // Czekaj na CR (Command list Running)
    
    port->cmd |= (1 << 4); // FRE (FIS Receive Enable)
    port->cmd |= (1 << 0); // ST (Start)

    write_serial_string("[AHCI] Port zrebazowany.\n");
    memset(clb_phys, 0, 4096);
    memset(fb_phys, 0, 4096);
    memset(ctba_phys, 0, 4096);
}

void ahci_init(uint32_t abar_phys) {
    // Mapujemy rejestry HBA
    vmm_map(abar_phys, abar_phys, PAGE_PRESENT | PAGE_WRITABLE);
    ahci_hba_mem* hba = (ahci_hba_mem*)(uintptr_t)abar_phys;

    hba->ghc |= (1 << 31); // Włącz AE (AHCI Enable)

    uint32_t pi = hba->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            uint32_t ssts = hba->ports[i].ssts;
            uint8_t det = ssts & 0x0F;
            
            if (det == 3) { // 3 = Device present and PHY established
                write_serial_string("[AHCI] Znaleziono dysk na porcie: ");
                write_serial_hex(i);
                write_serial_string("\n");
                
                ahci_rebase_port(&hba->ports[i], i);

                // Rejestrujemy pierwszy napotkany dysk jako główny
                if (sata_port == nullptr) {
                    sata_port = &hba->ports[i];
                }
            }
        }
    }
}

bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    port->is = 0xFFFF; // Wyczyść flagi przerwań na porcie
    
    // Adres listy komend z rejestrów portu (pamiętaj o 64-bitach)
    uint64_t clb = port->clb | ((uint64_t)port->clbu << 32);
    ahci_hba_cmd_header* cmd_header = (ahci_hba_cmd_header*)clb;

    int slot = 0; // Używamy pierwszego slotu
    cmd_header[slot].cfl = sizeof(fis_reg_h2d) / sizeof(uint32_t);
    cmd_header[slot].w = 0;      // Kierunek: Read (0)
    cmd_header[slot].prdtl = 1;  // Jeden wpis w PRDT (jeden ciągły bufor)

    // Pobieramy adres tabeli komend
    uint64_t ctba = cmd_header[slot].ctba | ((uint64_t)cmd_header[slot].ctbau << 32);
    ahci_hba_cmd_table* cmd_table = (ahci_hba_cmd_table*)ctba;

    // Czyścimy tabelę komend (bezpieczne rzutowanie dla uniknięcia warningów)
    uint32_t* table_ptr = (uint32_t*)cmd_table;
    for (int i = 0; i < (int)(sizeof(ahci_hba_cmd_table) / sizeof(uint32_t)); i++) {
        table_ptr[i] = 0;
    }

    // PRDT - wskazujemy na bufor w pamięci RAM
    // UWAGA: DBA musi być adresem FIZYCZNYM. Jeśli używasz VMM, 
    // upewnij się, że buffer jest zmapowany 1:1 lub pobierz jego adres fizyczny.
    uint64_t phys_buffer = vmm_get_phys((uint64_t)buffer); 

    if (phys_buffer == 0) {
        write_serial_string("[AHCI] ERROR: Buffer not mapped in VMM!\n");
        return false;
    }

    cmd_table->prdt_entry[0].dba = (uint32_t)(uintptr_t)phys_buffer;
    cmd_table->prdt_entry[0].dbau = (uint32_t)((uintptr_t)phys_buffer >> 32);
    cmd_table->prdt_entry[0].dbc = (count << 9) - 1; // Rozmiar w bajtach - 1
    cmd_table->prdt_entry[0].i = 1;                 // Interrupt on completion

    // Budowa FIS-u (ATA Command)
    fis_reg_h2d* cmdfis = (fis_reg_h2d*)(&cmd_table->cfis);
    cmdfis->fis_type = 0x27; // Register FIS - Host to Device
    cmdfis->c = 1;           // Command
    cmdfis->command = 0x25;  // READ DMA EXT (LBA48)

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6; // LBA mode
    
    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);

    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    // Czekaj aż dysk nie będzie zajęty
    int spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) return false; // Timeout

    port->ci = (1 << slot); // WYDANIE ROZKAZU


    write_serial_string("[AHCI] Status rejestru PxSSTS: ");
    write_serial_hex(port->ssts);
    write_serial_string("\n[AHCI] Status rejestru PxTFD: ");
    write_serial_hex(port->tfd);
    write_serial_string("\n");

    uint64_t timeout = 1000000; // Dowolna duża liczba
    while (timeout--) {
        if ((port->ci & (1 << slot)) == 0) {
            write_serial_string("[AHCI] Komenda zakonczona sukcesem!\n");
            return true;
        }

        // Jeśli status rejestru IS (Interrupt Status) pokazuje błąd
        if (port->is & (1 << 30)) {
            write_serial_string("[AHCI] FATAL ERROR w rejestrze PxIS!\n");
            return false;
        }
    }

    write_serial_string("[AHCI] TIMEOUT! Dysk nie odpowiedzial na komende.\n");
    return false;
}


bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    port->is = 0xFFFF; // Czyścimy rejestr przerwań
    int slot = 0;      // Używamy slotu 0

    // Pobieramy adres fizyczny bufora (wymagane dla DMA)
    uint64_t phys_buffer = vmm_get_phys((uint64_t)buffer);
    if (phys_buffer == 0) {
        write_serial_string("[AHCI] WRITE ERROR: Buffer not mapped!\n");
        return false;
    }

    // Pobieramy nagłówek komendy - TU POPRAWKA NA ahci_hba_cmd_header
    uint64_t clb_addr = ((uint64_t)port->clbu << 32) | port->clb;
    ahci_hba_cmd_header* cmd_header = (ahci_hba_cmd_header*)clb_addr;

    cmd_header[slot].cfl = sizeof(fis_reg_h2d) / sizeof(uint32_t); 
    cmd_header[slot].w = 1;      // Operacja ZAPISU
    cmd_header[slot].prdtl = 1;  

    // Pobieramy tablicę komend - TU POPRAWKA NA ahci_hba_cmd_table
    uint64_t ctba_addr = ((uint64_t)cmd_header[slot].ctba | ((uint64_t)cmd_header[slot].ctbau << 32));
    ahci_hba_cmd_table* cmd_table = (ahci_hba_cmd_table*)ctba_addr;
    
    // Używamy Twojego memset, żeby wyczyścić tablicę komend
    memset(&cmd_table[slot], 0, sizeof(ahci_hba_cmd_table));

    // Ustawiamy PRDT
    cmd_table->prdt_entry[0].dba = (uint32_t)phys_buffer;
    cmd_table->prdt_entry[0].dbau = (uint32_t)(phys_buffer >> 32);
    cmd_table->prdt_entry[0].dbc = (count << 9) - 1; 
    cmd_table->prdt_entry[0].i = 1;

    // Budujemy FIS
    fis_reg_h2d* cmdfis = (fis_reg_h2d*)(&cmd_table->cfis);
    cmdfis->fis_type = 0x27; 
    cmdfis->c = 1;           
    cmdfis->command = 0x35;  // WRITE DMA EXT

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6; 

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);

    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    // Czekamy na gotowość portu
    uint32_t spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) return false;

    port->ci = (1 << slot); 

    // Czekamy na koniec
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) {
            write_serial_string("[AHCI] Write Disk Error!\n");
            return false;
        }
    }

    return true;
}