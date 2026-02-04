#include "pci.h"
#include "io.h"
#include "kernel.h"
#include "ahci.h"

extern "C" uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    return inl(0xCFC);
}

extern "C" void pci_init() {
    write_serial_string("[PCI] Rozpoczynam skanowanie magistrali...\n");

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            // Czytamy Vendor ID (offset 0)
            uint32_t reg0 = pci_config_read(bus, slot, 0, 0);
            uint16_t vendor = reg0 & 0xFFFF;
            
            if (vendor == 0xFFFF) continue; // Urządzenie nie istnieje

            // Czytamy Class Code (offset 0x08)
            uint32_t class_reg = pci_config_read(bus, slot, 0, 0x08);
            uint8_t base_class = (class_reg >> 24) & 0xFF;
            uint8_t sub_class  = (class_reg >> 16) & 0xFF;

            write_serial_string("[PCI] ");
            write_serial_hex(bus);
            write_serial_string(":");
            write_serial_hex(slot);
            write_serial_string(" Vendor:");
            write_serial_hex(vendor);
            write_serial_string(" Class:");
            write_serial_hex(base_class);
            write_serial_string("\n");

            // Szukamy kontrolera SATA (AHCI)
            if (base_class == 0x01 && sub_class == 0x06) {
                // BAR5 (Offset 0x24) to adres rejestrów AHCI (ABAR)
                uint32_t bar5 = pci_config_read(bus, slot, 0, 0x24);
                write_serial_string("      -> ZNALEZIONO KONTROLER SATA AHCI! BAR5: ");
                write_serial_hex(bar5);
                write_serial_string("\n");

                // Włącz Bus Mastering (bit 2) i Memory Space (bit 1) w rejestrze Command (offset 0x04)
                uint32_t pci_cmd = pci_config_read(bus, slot, 0, 0x04);
                pci_cmd |= (1 << 2) | (1 << 1);
                pci_config_write(bus, slot, 0, 0x04, pci_cmd);

                // Inicjalizujemy AHCI z tym adresem
                ahci_init(bar5);
            }
        }
    }
    write_serial_string("[PCI] Skanowanie zakonczone.\n");
}

void pci_config_write(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(0xCF8, address);
    outl(0xCFC, value);
}