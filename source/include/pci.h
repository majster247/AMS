#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicjalizacja i skanowanie magistrali PCI
void pci_init();

// Funkcja pomocnicza do czytania konfiguracji (może się przydać w sterownikach)
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#ifdef __cplusplus
}
#endif