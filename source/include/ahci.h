#pragma once
#include <stdint.h>

#define PHYSICAL_MEM_OFFSET 0xFFFF800000000000

// Typy FIS
typedef enum {
    FIS_TYPE_REG_H2D = 0x27,
    FIS_TYPE_REG_D2H = 0x34,
    FIS_TYPE_DMA_ACT = 0x39,
    FIS_TYPE_DMA_SETUP = 0x41,
    FIS_TYPE_DATA = 0x46,
    FIS_TYPE_BIST = 0x58,
    FIS_TYPE_PIO_SETUP = 0x5F,
    FIS_TYPE_DEV_BITS = 0xA1,
} FIS_TYPE;

struct fis_reg_h2d {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  reserved0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  reserved1[4];
} __attribute__((packed));

struct ahci_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc:22;
    uint32_t reserved1:9;
    uint32_t i:1;
} __attribute__((packed));

struct ahci_command_header {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  reserved0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved1[4];
} __attribute__((packed));

// PANCERNA ZMIANA: Zwiększamy ilość wpisów PRDT
// 128 wpisów pozwala na bardzo poszatkowane transfery (bezpieczeństwo DMA)
struct ahci_command_table {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    ahci_prdt_entry prdt_entry[128]; 
} __attribute__((packed));

struct ahci_port {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed));

struct ahci_hba_mem {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_port ports[32];
} __attribute__((packed));

extern "C" {
    void ahci_init(uint32_t bar5);
    bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
    bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
    void ahci_read_sectors(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
}