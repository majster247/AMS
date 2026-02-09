#include "ahci.h"
#include "kernel.h"
#include "vmm.h"
#include "heap.h"

ahci_port* sata_port = nullptr;
extern "C" void pmm_free_frame(void* ptr); // Zadeklarowane w pmm.cpp
extern "C" void vmm_set_nocache(uint64_t virt);

#define VIRT(addr) ((uint64_t)(addr) + PHYSICAL_MEM_OFFSET)
#define ATA_CMD_READ_DMA_EXT  0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY      0xEC

// PCI Helpers
static inline void outl(uint16_t port, uint32_t val) { asm volatile("outl %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint32_t inl(uint16_t port) { uint32_t ret; asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }

uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
    outl(0xCF8, address);
    uint32_t current = inl(0xCFC);
    if ((offset & 2) == 0) current = (current & 0xFFFF0000) | value;
    else current = (current & 0x0000FFFF) | (value << 16);
    outl(0xCFC, current);
}

void force_pci_bus_master() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t vendor = pci_read_config_dword(bus, slot, func, 0);
                if ((vendor & 0xFFFF) == 0xFFFF) continue;
                uint32_t class_rev = pci_read_config_dword(bus, slot, func, 0x08);
                if (((class_rev >> 24) & 0xFF) == 0x01 && ((class_rev >> 16) & 0xFF) == 0x06) { // SATA
                    uint32_t cmd = pci_read_config_dword(bus, slot, func, 0x04) & 0xFFFF;
                    if (!(cmd & 0x04)) {
                        write_serial_string("[AHCI] Wlaczam Bus Master...\n");
                        pci_write_config_word(bus, slot, func, 0x04, cmd | 0x07);
                    }
                    return;
                }
            }
        }
    }
}

void ahci_start_port(ahci_port* port) {
    while (port->cmd & (1 << 15));
    port->cmd |= 0x0010;
    port->cmd |= 0x0001;
}

void ahci_stop_port(ahci_port* port) {
    port->cmd &= ~0x0001;
    port->cmd &= ~0x0010;
    int timeout = 1000000;
    while (timeout--) {
        if (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14))) return;
    }
}

void ahci_rebase_port(ahci_port* port, int port_no) {
    ahci_stop_port(port);
    
    // Alokujemy nowe struktury w pamięci fizycznej (PMM)
    uint64_t clb_phys = (uint64_t)pmm_alloc_frame();
    uint64_t fb_phys = (uint64_t)pmm_alloc_frame();
    uint64_t ctba_phys = (uint64_t)pmm_alloc_frame();

    port->clb = (uint32_t)clb_phys;
    port->clbu = (uint32_t)(clb_phys >> 32);
    port->fb = (uint32_t)fb_phys;
    port->fbu = (uint32_t)(fb_phys >> 32);

    memset((void*)VIRT(clb_phys), 0, 1024);
    memset((void*)VIRT(fb_phys), 0, 256);
    
    ahci_command_header* cmd_header = (ahci_command_header*)VIRT(clb_phys);
    cmd_header[0].ctba = (uint32_t)ctba_phys;
    cmd_header[0].ctbau = (uint32_t)(ctba_phys >> 32);
    memset((void*)VIRT(ctba_phys), 0, 256);

    ahci_start_port(port);
    port->serr = 0xFFFFFFFF; // Clear errors
    port->is = 0xFFFFFFFF;   // Clear int status
    port->ie = 0;            // Disable IRQ
}

// Funkcja wykonawcza z obsługą Bounce Buffer i Forgiving Error Check
bool ahci_send_command(ahci_port* port, uint8_t cmd, uint64_t lba, uint32_t count, uint16_t* buffer) {
    port->is = 0xFFFFFFFF; 
    int slot = 0;

    void* dma_phys = pmm_alloc_frame();
    uint8_t* dma_virt = (uint8_t*)VIRT(dma_phys);
    memset(dma_virt, 0, 4096);

    // [FIX 1] JEŚLI ZAPISUJEMY (0x35), KOPIUJEMY DANE DO BUFORA PRZED WYSŁANIEM!
    if (cmd == 0x35) {
        memcpy(dma_virt, buffer, count * 512);
    }

    ahci_command_header* cmd_header = (ahci_command_header*)VIRT(port->clb);
    cmd_header[slot].prdtl = 1;
    // [FIX 2] Ustaw flagę zapisu (bit 6 w bajcie 0: w)
    cmd_header[slot].w = (cmd == 0x35) ? 1 : 0; 
    cmd_header[slot].cfl = sizeof(fis_reg_h2d) / 4;
    cmd_header[slot].w = 0; // Ups, wyżej ustawiłem, a tu zerowałem. Poprawka poniżej:
    // PRAWIDŁOWE USTAWIANIE FLAG:
    if (cmd == 0x35) cmd_header[slot].w = 1; 
    else cmd_header[slot].w = 0;

    ahci_command_table* cmd_table = (ahci_command_table*)VIRT(cmd_header[slot].ctba);
    memset(cmd_table, 0, sizeof(ahci_command_table));

    cmd_table->prdt_entry[0].dba = (uint32_t)(uint64_t)dma_phys;
    cmd_table->prdt_entry[0].dbau = (uint32_t)((uint64_t)dma_phys >> 32);
    cmd_table->prdt_entry[0].dbc = (count * 512) - 1;
    cmd_table->prdt_entry[0].i = 1;

    fis_reg_h2d* cmdfis = (fis_reg_h2d*)(&cmd_table->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = cmd;
    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6; 
    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);
    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    while (port->tfd & (0x80 | 0x08));
    port->ci = (1 << slot);

    while (true) {
        if (!(port->ci & (1 << slot))) break;
        if (port->is & (1 << 30)) {
             pmm_free_frame(dma_phys);
             return false;
        }
    }

    // JEŚLI ODCZYT, KOPIUJEMY Z POWROTEM
    if (cmd != 0x35) {
        memcpy(buffer, dma_virt, count * 512);
    }

    pmm_free_frame(dma_phys);
    return true;
}

// Identify
void ahci_identify(ahci_port* port) {
    uint16_t* buf = (uint16_t*)kmalloc(512);
    write_serial_string("[AHCI] IDENTIFY...\n");
    if (ahci_send_command(port, ATA_CMD_IDENTIFY, 0, 1, buf)) {
        write_serial_string("[AHCI] IDENTIFY OK! Model: ");
        char model[41];
        for (int i = 0; i < 20; i++) {
            uint16_t data = buf[27 + i];
            model[i * 2] = (data >> 8) & 0xFF;
            model[i * 2 + 1] = data & 0xFF;
        }
        model[40] = 0;
        write_serial_string(model); write_serial_string("\n");
    } else {
        write_serial_string("[AHCI] IDENTIFY Failed.\n");
    }
    kfree(buf);
}

bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    uint8_t* ptr = (uint8_t*)buffer;
    uint32_t left = count;
    uint64_t curr = lba;
    
    // Chunking 4KB (8 sektorów)
    while (left > 0) {
        uint32_t chunk = (left > 8) ? 8 : left;
        if (!ahci_send_command(port, ATA_CMD_READ_DMA_EXT, curr, chunk, (uint16_t*)ptr)) return false;
        left -= chunk;
        curr += chunk;
        ptr += chunk * 512;
    }
    return true;
}

bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    // Musimy faktycznie wysłać komendę zapisu (DMA WRITE EXT = 0x35)
    uint8_t* ptr = (uint8_t*)buffer;
    uint32_t left = count;
    uint64_t curr = lba;

    while (left > 0) {
        uint32_t chunk = (left > 8) ? 8 : left;
        // Używamy Twojej funkcji ahci_send_command z kodem 0x35
        if (!ahci_send_command(port, 0x35, curr, chunk, (uint16_t*)ptr)) return false;
        left -= chunk;
        curr += chunk;
        ptr += chunk * 512;
    }
    return true;
}


extern "C" void ahci_init(uint32_t bar5) {
    // 1. Mapowanie i WYŁĄCZENIE CACHE dla BAR5 (Kluczowe!)
    uint64_t virt_bar = (uint64_t)bar5 + PHYSICAL_MEM_OFFSET;
    vmm_set_nocache(virt_bar);

    ahci_hba_mem* hba_mem = (ahci_hba_mem*)virt_bar;
    hba_mem->ghc |= (1U << 31); // AE
    
    // 2. Szukaj dysku
    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            ahci_port* port = &hba_mem->ports[i];
            if (port->sig == 0x00000101) {
                write_serial_string("[AHCI] SATA Drive found. Initializing...\n");
                sata_port = port;
                ahci_rebase_port(port, i);
                
                // TEST IDENTIFY
                uint16_t id_buf[256];
                if (ahci_send_command(port, 0xEC, 0, 1, id_buf)) {
                    write_serial_string("[AHCI] IDENTIFY SUCCESS!\n");
                }
                return;
            }
        }
    }
}

void ahci_read_sectors(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    ahci_read(port, lba, count, buffer);
}

void ahci_reset_port(ahci_port* port) {
    ahci_stop_port(port);
    
    // Wymuś reset HBA dla tego portu
    port->sctl = (port->sctl & ~0x0F) | 1; // Detach device
    for(volatile int i=0; i<10000; i++);   // Czekaj
    port->sctl = (port->sctl & ~0x0F) | 0; // Re-attach
    
    // Czekaj na komunikację (SSTS)
    while ((port->ssts & 0x0F) != 3);
    
    port->serr = 0xFFFFFFFF; // Czyść błędy po resecie
    ahci_start_port(port);
}