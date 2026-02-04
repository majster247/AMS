#include "ahci.h"
#include "kernel.h"
#include "vmm.h"
#include "heap.h"

ahci_port* sata_port = nullptr;

// Pomocnicze makro do zamiany adresu fizycznego na wirtualny w Higher Half
#define VIRT(addr) ((uint64_t)(addr) + PHYSICAL_MEM_OFFSET)

bool ahci_stop_port(ahci_port* port) {
    port->cmd &= ~0x0001; // ST = 0
    port->cmd &= ~0x0010; // FRE = 0

    int timeout = 1000000;
    while (timeout--) {
        if (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14))) return true;
    }
    return false;
}

void ahci_start_port(ahci_port* port) {
    while (port->cmd & (1 << 15)); // Czekaj na CR = 0
    port->cmd |= 0x0010;           // FRE = 1
    port->cmd |= 0x0001;           // ST = 1
}

void ahci_rebase_port(ahci_port* port, int port_no) {
    ahci_stop_port(port);

    // Alokujemy struktury (muszą być wyrównane!)
    // Command List: 1KB wyrównania (32 wpisy po 32 bajty)
    // Received FIS: 256 bajtów
    // Command Table: zależnie od ilości PRDT
    
    uint64_t clb_phys = (uint64_t)pmm_alloc_frame(); // Używamy PMM dla czystych ramek
    uint64_t fb_phys = (uint64_t)pmm_alloc_frame();

    port->clb = (uint32_t)clb_phys;
    port->clbu = (uint32_t)(clb_phys >> 32);
    port->fb = (uint32_t)fb_phys;
    port->fbu = (uint32_t)(fb_phys >> 32);

    // Zerujemy struktury w pamięci wirtualnej
    memset((void*)VIRT(clb_phys), 0, 1024);
    memset((void*)VIRT(fb_phys), 0, 256);

    // Alokacja Command Table dla slotu 0
    uint64_t ctba_phys = (uint64_t)pmm_alloc_frame();
    ahci_command_header* cmd_header = (ahci_command_header*)VIRT(clb_phys);
    cmd_header[0].ctba = (uint32_t)ctba_phys;
    cmd_header[0].ctbau = (uint32_t)(ctba_phys >> 32);
    memset((void*)VIRT(ctba_phys), 0, 256);

    port->serr = 0xFFFFFFFF; // Czyścimy błędy
    port->is = 0xFFFFFFFF;   // Czyścimy przerwania

    ahci_start_port(port);
    write_serial_string("[AHCI] Port rebased i wystartowany.\n");
}

bool ahci_command(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer, bool is_write) {
    int slot = 0; // Używamy tylko pierwszego slotu dla uproszczenia

    // 1. Czekaj na wolne urządzenie (BSY/DRQ w Task File)
    int timeout = 1000000;
    while ((port->tfd & (0x80 | 0x08)) && timeout--) {
        for(volatile int i=0; i<100; i++);
    }
    if (timeout <= 0) {
        write_serial_string("[AHCI] Port BSY/DRQ timeout!\n");
        return false;
    }

    // 2. Pobierz wskaźniki na struktury
    ahci_command_header* cmd_header = (ahci_command_header*)VIRT(port->clb);
    cmd_header += slot;
    cmd_header->cfl = sizeof(fis_reg_h2d) / sizeof(uint32_t);
    cmd_header->w = is_write ? 1 : 0;
    cmd_header->prdtl = 1; // Jeden wpis w PRDT

    ahci_command_table* cmd_table = (ahci_command_table*)VIRT(cmd_header->ctba);
    memset(cmd_table, 0, sizeof(ahci_command_table));

    // 3. Ustaw PRDT (gdzie DMA ma pisać/czytać)
    uint64_t phys_buf = vmm_get_phys((uint64_t)buffer);
    cmd_table->prdt_entry[0].dba = (uint32_t)phys_buf;
    cmd_table->prdt_entry[0].dbau = (uint32_t)(phys_buf >> 32);
    cmd_table->prdt_entry[0].dbc = (count << 9) - 1; // 512B na sektor
    cmd_table->prdt_entry[0].i = 1;

    // 4. Buduj FIS (Host to Device)
    fis_reg_h2d* cmdfis = (fis_reg_h2d*)(&cmd_table->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = is_write ? 0x35 : 0x25; // WRITE/READ DMA EXT

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6; // LBA Mode
    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);
    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    // 5. Wydaj komendę i czekaj
    port->is = 0xFFFFFFFF;
    asm volatile("mfence" ::: "memory"); // Upewnij się, że zapisy do RAM się zakończyły
    port->ci = (1 << slot);

    while (true) {
        if (!(port->ci & (1 << slot))) break;
        if (port->is & (1 << 30)) { // TFES bit
            write_serial_string("[AHCI] Blad krytyczny HBA! PxSERR: ");
            write_serial_hex(port->serr);
            return false;
        }
        asm volatile("pause"); // Daj procesorowi odpocząć, AHCI i tak jest wolne
    }

    return (port->tfd & 0x01) ? false : true;
}

bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    return ahci_command(port, lba, count, buffer, false);
}

bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    return ahci_command(port, lba, count, buffer, true);
}

extern "C" void ahci_init(uint32_t bar5) {
    write_serial_string("[AHCI] Start inicjalizacji MMIO...\n");

    uint64_t virt_bar = (uint64_t)bar5 + PHYSICAL_MEM_OFFSET;
    vmm_map(virt_bar, bar5, 0x03 | (1 << 4)); 

    ahci_hba_mem* hba_mem = (ahci_hba_mem*)virt_bar;
    hba_mem->ghc |= (1U << 31); // AE bit (AHCI Enable)

    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            ahci_port* port = &hba_mem->ports[i];
            uint32_t sig = port->sig;
            if (sig == 0x00000101) {
                write_serial_string("[AHCI] Znaleziono dysk SATA na porcie ");
                write_serial_hex(i);
                write_serial_string("\n");
                sata_port = port;
                ahci_rebase_port(port, i);
                return;
            }
        }
    }
}