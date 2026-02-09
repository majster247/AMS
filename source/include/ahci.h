/**
 * @file ahci.h
 * @author Majster
 * @brief Obsługa kontrolera SATA AHCI (Advanced Host Controller Interface).
 */

#pragma once
#include <stdint.h>

/** @brief Przesunięcie pamięci fizycznej dla mapowania HBA */
#define PHYSICAL_MEM_OFFSET 0xFFFF800000000000

/** @brief Typy struktur FIS (Frame Information Structure) */
typedef enum {
    FIS_TYPE_REG_H2D = 0x27,   /**< Rejestr Host do Device */
    FIS_TYPE_REG_D2H = 0x34,   /**< Rejestr Device do Host */
    FIS_TYPE_DMA_ACT = 0x39,   /**< Aktywacja DMA */
    FIS_TYPE_DMA_SETUP = 0x41, /**< Setup DMA */
    FIS_TYPE_DATA = 0x46,      /**< FIS z danymi */
    FIS_TYPE_BIST = 0x58,      /**< Test wbudowany */
    FIS_TYPE_PIO_SETUP = 0x5F, /**< Setup PIO */
    FIS_TYPE_DEV_BITS = 0xA1,  /**< Bity urządzenia */
} FIS_TYPE;

/** @brief Struktura rejestru FIS Host-to-Device */
struct fis_reg_h2d {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  reserved0:3;
    uint8_t  c:1;        /**< Command/Control bit */
    uint8_t  command;    /**< Rejestr komend ATA */
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

/** @brief Wpis w tablicy PRDT (Physical Region Descriptor Table) dla DMA */
struct ahci_prdt_entry {
    uint32_t dba;        /**< Adres bazy danych (32-bit low) */
    uint32_t dbau;       /**< Adres bazy danych (32-bit high) */
    uint32_t reserved0;
    uint32_t dbc:22;     /**< Liczba bajtów (max 4MB per entry) */
    uint32_t reserved1:9;
    uint32_t i:1;        /**< Interrupt on completion */
} __attribute__((packed));

/** @brief Nagłówek komendy w liście komend portu */
struct ahci_command_header {
    uint8_t  cfl:5;      /**< Command FIS length w dwordach */
    uint8_t  a:1;        /**< ATAPI */
    uint8_t  w:1;        /**< Write (1=H2D, 0=D2H) */
    uint8_t  p:1;        /**< Prefetchable */
    uint8_t  r:1;        /**< Reset */
    uint8_t  b:1;        /**< BIST */
    uint8_t  c:1;        /**< Clear busy upon R_OK */
    uint8_t  reserved0:1;
    uint8_t  pmp:4;      /**< Port multiplier port */
    uint16_t prdtl;      /**< PRDT length w liczbie wpisów */
    volatile uint32_t prdbc; /**< PRD byte count (ile bajtów przesłano) */
    uint32_t ctba;       /**< Adres bazy tabeli komend (low) */
    uint32_t ctbau;      /**< Adres bazy tabeli komend (high) */
    uint32_t reserved1[4];
} __attribute__((packed));

/** @brief Tabela komend zawierająca FIS i PRDT */
struct ahci_command_table {
    uint8_t  cfis[64];   /**< Command FIS */
    uint8_t  acmd[16];   /**< ATAPI command */
    uint8_t  reserved[48];
    ahci_prdt_entry prdt_entry[128]; /**< Wpisy PRDT dla transferów DMA */
} __attribute__((packed));

/** @brief Struktura rejestrów pojedynczego portu AHCI */
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

/** @brief HBA Memory Space - Główna struktura rejestrów kontrolera */
struct ahci_hba_mem {
    volatile uint32_t cap;    /**< Host capabilities */
    volatile uint32_t ghc;    /**< Global host control */
    volatile uint32_t is;     /**< Interrupt status */
    volatile uint32_t pi;     /**< Ports implemented */
    volatile uint32_t vs;     /**< Version */
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;   /**< BIOS/OS handoff control */
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_port ports[32];      /**< Rejestry dla maksymalnie 32 portów */
} __attribute__((packed));

extern "C" {
    /** @brief Inicjalizuje kontroler AHCI pod wskazanym adresem BAR5 */
    void ahci_init(uint32_t bar5);
    /** @brief Odczytuje sektory z dysku (LBA) do bufora pamięci */
    bool ahci_read(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
    /** @brief Zapisuje dane z bufora na dysk pod wskazany adres LBA */
    bool ahci_write(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
    /** @brief Funkcja pomocnicza do czytania wielu sektorów */
    void ahci_read_sectors(ahci_port* port, uint64_t lba, uint32_t count, uint16_t* buffer);
}