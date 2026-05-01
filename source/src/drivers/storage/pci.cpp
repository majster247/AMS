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
            uint32_t reg0 = pci_config_read(bus, slot, 0, 0);
            uint16_t vendor = reg0 & 0xFFFF;
            
            if (vendor == 0xFFFF) continue; // Urządzenie nie istnieje

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

                // Włącz Bus Mastering (bit 2) i Memory Space (bit 1)
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

extern "C" uint16_t pci_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    uint32_t tmp = inl(0xCFC);
    return (uint16_t)((tmp >> ((offset & 2) * 8)) & 0xFFFF);
}

void debug_pci_scan() {
    write_serial_string("--- SKANOWANIE PCI START ---\n");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_word(bus, slot, 0, 0); 
            
            if (vendor != 0xFFFF) {
                uint16_t device = pci_read_word(bus, slot, 0, 2);
                uint16_t class_code = pci_read_word(bus, slot, 0, 10);
                
                write_serial_string("PCI Device: Bus "); write_serial_dec(bus);
                write_serial_string(" Slot "); write_serial_dec(slot);
                write_serial_string(" Vendor: "); write_serial_hex(vendor);
                write_serial_string(" Device: "); write_serial_hex(device);
                write_serial_string(" Class: "); write_serial_hex(class_code);
                
                if ((class_code >> 8) == 0x0C) {
                    write_serial_string(" -> USB CONTROLLER");
                }
                if ((class_code >> 8) == 0x06 && (class_code & 0xFF) == 0x01) {
                    write_serial_string(" -> ISA BRIDGE (PS/2)");
                }
                write_serial_string("\n");
            }
        }
    }
    write_serial_string("--- SKANOWANIE PCI KONIEC ---\n");
}

//pic remap (remapowanie PIC z 0-15 na 32-47)
void pic_remap() {
    // Inicjalizacja PIC
    outb(0x20, 0x11); // Master PIC: start initialization
    outb(0xA0, 0x11); // Slave PIC: start initialization
    outb(0x21, 0x20); // Master PIC: remap offset to 32 (0x20)
    outb(0xA1, 0x28); // Slave PIC: remap offset to 40 (0x28)
    outb(0x21, 0x04); // Master PIC: inform about slave at IRQ2 (0000 0100)
    outb(0xA1, 0x02); // Slave PIC: inform about cascade identity (0000 0010)
    outb(0x21, 0x01); // Master PIC: set mode to 8086/88 (MCS-80/85)
    outb(0xA1, 0x01); // Slave PIC: set mode to 8086/88 (MCS-80/85)
    outb(0x21, 0x00); // Master PIC: unmask all IRQs
    outb(0xA1, 0x00); // Slave PIC: unmask all IRQs
}