#pragma once
#include <stdint.h>

// Typy urządzeń SATA
#define SATA_SIG_ATA   0x00000101  // Dysk SATA
#define SATA_SIG_ATAPI 0xEB140101  // Napęd CD/DVD
#define SATA_SIG_SEMB  0xC33C0101  // Enclosure management bridge
#define SATA_SIG_PM    0x96690101  // Port multiplier

// Status portu
#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3


// Typy FIS (Framework Information Structure)
typedef enum {
    FIS_TYPE_REG_H2D = 0x27,   // Host to Device
    FIS_TYPE_REG_D2H = 0x34,   // Device to Host
    FIS_TYPE_DMA_ACT = 0x39,   // DMA activate
    FIS_TYPE_DMA_SETUP = 0x41, // DMA setup
    FIS_TYPE_DATA = 0x46,      // Data
    FIS_TYPE_BIST = 0x58,      // BIST activate
    FIS_TYPE_PIO_SETUP = 0x5F, // PIO setup
    FIS_TYPE_DEV_BITS = 0xA1,  // Device bits
} FIS_TYPE;

// Struktura wpisu w tablicy PRDT (Physical Region Descriptor Table)
struct ahci_hba_prdt_entry {
    uint32_t dba;       // Data base address (low)
    uint32_t dbau;      // Data base address (high)
    uint32_t reserved0;
    uint32_t dbc:22;    // Byte count (max 4MB)
    uint32_t reserved1:9;
    uint32_t i:1;       // Interrupt on completion
} __attribute__((packed));

// Nagłówek komendy
struct ahci_hba_cmd_header {
    uint8_t  cfl:5;     // Command FIS length in dwords
    uint8_t  a:1;       // ATAPI
    uint8_t  w:1;       // Write
    uint8_t  p:1;       // Prefetchable
    uint8_t  r:1;       // Reset
    uint8_t  b:1;       // BIST
    uint8_t  c:1;       // Clear busy upon R_OK
    uint8_t  reserved0:1;
    uint8_t  pmp:4;     // Port multiplier port
    uint16_t prdtl;     // PRDT length in entries
    volatile uint32_t prdbc; // PRD byte count transferred
    uint32_t ctba;      // Command table descriptor base address
    uint32_t ctbau;     // Command table descriptor base address upper
    uint32_t reserved1[4];
} __attribute__((packed));

// Struktura FIS Host to Device (do wysyłania komend ATA)
struct fis_reg_h2d {
    uint8_t  fis_type;  // FIS_TYPE_REG_H2D
    uint8_t  pmport:4;  // Port multiplier
    uint8_t  reserved0:3;
    uint8_t  c:1;       // 1: Command, 0: Control
    uint8_t  command;   // Command register
    uint8_t  featurel;  // Feature register low
    uint8_t  lba0;      // LBA low, 7:0
    uint8_t  lba1;      // LBA mid, 15:8
    uint8_t  lba2;      // LBA high, 23:16
    uint8_t  device;    // Device register
    uint8_t  lba3;      // LBA, 31:24
    uint8_t  lba4;      // LBA, 39:32
    uint8_t  lba5;      // LBA, 47:40
    uint8_t  featureh;  // Feature register high
    uint8_t  countl;    // Count low, 7:0
    uint8_t  counth;    // Count high, 15:8
    uint8_t  icc;       // Isochronous command completion
    uint8_t  control;   // Control register
    uint8_t  reserved1[4];
} __attribute__((packed));

// Command Table (musi być wyrównana do 128 bajtów)
struct ahci_hba_cmd_table {
    uint8_t  cfis[64];  // Command FIS
    uint8_t  acmd[16];  // ATAPI command
    uint8_t  reserved[48];
    ahci_hba_prdt_entry prdt_entry[1]; // Można zwiększyć dla większych transferów
} __attribute__((packed));



// Rejestry pojedynczego portu (SATA Port)
struct ahci_port {
    uint32_t clb;       // Command list base address (low)
    uint32_t clbu;      // Command list base address (high)
    uint32_t fb;        // FIS base address (low)
    uint32_t fbu;       // FIS base address (high)
    uint32_t is;        // Interrupt status
    uint32_t ie;        // Interrupt enable
    uint32_t cmd;       // Command and status
    uint32_t reserved0;
    uint32_t tfd;       // Task file data
    uint32_t sig;       // Signature
    uint32_t ssts;      // SATA status
    uint32_t sctl;      // SATA control
    uint32_t serr;      // SATA error
    uint32_t sact;      // SATA active
    uint32_t ci;        // Command issue
    uint32_t sntf;      // SATA notification
    uint32_t fbs;       // FIS-based switching control
    uint32_t reserved1[11];
    uint32_t vendor[4];
} __attribute__((packed));

// Rejestry pamięci kontrolera (HBA Memory Space)
struct ahci_hba_mem {
    uint32_t cap;       // Host capability
    uint32_t ghc;       // Global host control
    uint32_t is;        // Interrupt status
    uint32_t pi;        // Ports implemented
    uint32_t vs;        // Version
    uint32_t ccc_ctl;   // Command completion coalescing control
    uint32_t ccc_pts;   // Command completion coalescing ports
    uint32_t em_loc;    // Enclosure management location
    uint32_t em_ctl;    // Enclosure management control
    uint32_t cap2;      // Host capabilities extended
    uint32_t bohc;      // BIOS/OS handoff control and status
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_port ports[32]; // Lista portów
} __attribute__((packed));

void ahci_init(uint32_t abar_phys);
bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
void ahci_rebase_port(ahci_port* port, int port_no);
bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);

