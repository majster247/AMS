#include "ahci.h"
#include "kernel.h"
#include "vmm.h"
#include "heap.h"

ahci_port* sata_port = nullptr;
extern "C" void pmm_free_frame(void* ptr); 
extern "C" void vmm_map_mmio(uint64_t virt, uint64_t phys, size_t page_count);

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

void ahci_start_port(ahci_port* port) {
    while (port->cmd & (1 << 15));
    port->cmd |= 0x0010; // FRE
    port->cmd |= 0x0001; // ST
}

void ahci_stop_port(ahci_port* port) {
    port->cmd &= ~0x0001; // ST
    port->cmd &= ~0x0010; // FRE
    int timeout = 1000000;
    while (timeout--) {
        if (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14))) return;
    }
}

void ahci_rebase_port(ahci_port* port, int port_no) {
    (void)port_no;
    ahci_stop_port(port);
    
    // Alokujemy struktury sterujące w pamięci fizycznej
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
    // Mapujemy tylko pierwszy slot dla prostoty
    cmd_header[0].ctba = (uint32_t)ctba_phys;
    cmd_header[0].ctbau = (uint32_t)(ctba_phys >> 32);
    memset((void*)VIRT(ctba_phys), 0, 256);

    ahci_start_port(port);
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    port->ie = 0;
}

bool ahci_send_command(ahci_port* port, uint8_t cmd, uint64_t lba, uint32_t count, uint16_t* buffer) {
    port->is = 0xFFFFFFFF; 
    int slot = 0;

    // DMA Bounce Buffer
    void* dma_phys = pmm_alloc_frame();
    uint8_t* dma_virt = (uint8_t*)VIRT(dma_phys);
    memset(dma_virt, 0, 4096);

    // Jeśli zapisujemy, kopiujemy dane z bufora użytkownika do bufora DMA
    if (cmd == ATA_CMD_WRITE_DMA_EXT) {
        memcpy(dma_virt, buffer, count * 512);
    }

    ahci_command_header* cmd_header = (ahci_command_header*)VIRT(port->clb);
    cmd_header[slot].cfl = sizeof(fis_reg_h2d) / 4;
    cmd_header[slot].w = (cmd == ATA_CMD_WRITE_DMA_EXT) ? 1 : 0; // POPRAWIONE: Brak nadpisywania zerem
    cmd_header[slot].prdtl = 1;

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
    cmdfis->device = 1 << 6; // LBA mode
    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);
    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    // Czekaj aż port będzie gotowy
    int spin = 0;
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) { spin++; }
    if (spin == 1000000) {
        pmm_free_frame(dma_phys);
        return false;
    }

    port->ci = (1 << slot); // Issue command

    while (true) {
        if (!(port->ci & (1 << slot))) break;
        if (port->is & (1 << 30)) { // Task file error
             pmm_free_frame(dma_phys);
             return false;
        }
    }

    // Jeśli odczyt, kopiujemy dane z bufora DMA z powrotem
    if (cmd != ATA_CMD_WRITE_DMA_EXT) {
        memcpy(buffer, dma_virt, count * 512);
    }

    pmm_free_frame(dma_phys);
    return true;
}

extern "C" void ahci_init(uint32_t bar5) {
    // 1. DYNAMICZNE MAPOWANIE MMIO (Naprawia Page Fault)
    // Mapujemy 32KB (8 stron) od fizycznego BAR5, by objąć rejestry sterownika i portów
    uint64_t virt_bar = (uint64_t)bar5 + 0xFFFF800000000000ULL;
    write_serial_string("[AHCI] Using pre-mapped BAR5 at: "); 
    write_serial_hex(virt_bar); 
    write_serial_string("\n");

    write_serial_string("[AHCI] BAR5 mapped to: "); write_serial_hex(virt_bar); write_serial_string("\n");

    ahci_hba_mem* hba_mem = (ahci_hba_mem*)virt_bar;

    write_serial_string("[AHCI] GHC Address: ");
    write_serial_hex((uint64_t)&hba_mem->ghc);
    write_serial_string("\n");

    hba_mem->ghc |= (1U << 31); // Global AHCI Enable
    
    uint32_t pi = hba_mem->pi; // Port Implemented bitmask
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            ahci_port* port = &hba_mem->ports[i];
            
            // Sprawdź sygnaturę portu (0x101 = SATA)
            if (port->sig == 0x00000101) {
                write_serial_string("[AHCI] SATA Drive found on port "); write_serial_hex(i); write_serial_string("\n");
                sata_port = port;
                ahci_rebase_port(port, i);
                
                // TEST IDENTIFY
                uint16_t id_buf[256];
                if (ahci_send_command(port, ATA_CMD_IDENTIFY, 0, 1, id_buf)) {
                    write_serial_string("[AHCI] IDENTIFY SUCCESS!\n");
                }
                return; // Znaleziono pierwszy dysk, kończymy
            }
        }
    }
    write_serial_string("[AHCI] No SATA drives found.\n");
}

bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    uint8_t* ptr = (uint8_t*)buffer;
    uint32_t left = count;
    uint64_t curr = lba;
    
    while (left > 0) {
        uint32_t chunk = (left > 8) ? 8 : left; // Max 4KB na raz (jeden frame)
        if (!ahci_send_command(port, ATA_CMD_READ_DMA_EXT, curr, chunk, (uint16_t*)ptr)) return false;
        left -= chunk;
        curr += chunk;
        ptr += chunk * 512;
    }
    return true;
}

bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    uint8_t* ptr = (uint8_t*)buffer;
    uint32_t left = count;
    uint64_t curr = lba;

    while (left > 0) {
        uint32_t chunk = (left > 8) ? 8 : left;
        if (!ahci_send_command(port, ATA_CMD_WRITE_DMA_EXT, curr, chunk, (uint16_t*)ptr)) return false;
        left -= chunk;
        curr += chunk;
        ptr += chunk * 512;
    }
    return true;
}

void ahci_read_sectors(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer) {
    ahci_read(port, lba, count, buffer);
}