# Sekwencja bootowania krok po kroku

[PL](./08-sekwencja-bootowania.md) | [EN](./en/08-boot-sequence-step-by-step.md)

Ta sekcja opisuje pełną ścieżkę wykonania od startu maszyny do pierwszego procesu użytkownika.

## 0. Firmware -> bootloader

Maszyna wirtualna uruchamia firmware, które przekazuje kontrolę do GRUB z obrazu `ams_run.iso`.

## 1. GRUB -> kernel entry

`grub.cfg` ładuje:

- kernel (`/boot/kernel.elf`),
- moduł initrd (`/boot/initrd.tar`).

Następnie wywoływany jest punkt wejścia kernela z informacją multiboot.

## 2. Wczesny start kernela

Kernel:

1. wyłącza przerwania na czas krytycznej inicjalizacji,
2. parsuje dane multiboot,
3. wykrywa i raportuje pamięć RAM.

## 3. PMM (Physical Memory Manager)

Następuje uruchomienie PMM i oznaczenie dostępnych obszarów pamięci:

- inicjalizacja struktur PMM,
- oznaczenie wolnych ramek,
- wydruk mapy pamięci do logu.

To fundament dla dalszych alokacji stron.

## 4. VMM (Virtual Memory Manager)

Kernel tworzy własny PML4, aktywuje go (`set_cr3`) i przygotowuje mapowania HHDM.

Rezultat:

- kernel działa w stabilnej przestrzeni wirtualnej,
- możliwy jest kontrolowany dostęp do ramek fizycznych przez offset HHDM.

## 5. Podsystemy jądra

Uruchamiane są kolejno:

- heap kernela,
- GDT,
- IDT,
- sterowniki wejścia (`keyboard`, `mouse`),
- interfejs syscall,
- VFS,
- inicjalizacja PCI i próba montowania EXT2.

## 6. Warstwa graficzna i scheduler

Kernel przygotowuje framebuffer/double buffer, tworzy desktop i terminal, następnie inicjalizuje scheduler i task kernela.

## 7. Przełączenie na docelowy stack kernela

Po inicjalizacji wykonywane jest przełączenie stosu na stack przypisany do bieżącego taska.  
To kończy etap bootstrap i rozpoczyna normalny runtime kernela.

## 8. Start sesji userspace

Kernel uruchamia domyślną ścieżkę sesji:

1. próba przygotowania uruchomienia Doom (w razie potrzeby build przez TCC),
2. start compositora Wayland,
3. fallback do klasycznego GUI AMS, jeśli sesja Wayland nie wystartuje.

## 9. Wejście procesu do ring 3

W momencie startu konkretnej aplikacji:

1. `sys_exec` przygotowuje przestrzeń procesu i stos,
2. scheduler przekazuje kontrolę do `jump_to_ring3`,
3. `iretq` wykonuje zmianę CPL i uruchamia kod user-space.

To końcowy etap sekwencji bootowania z perspektywy architektury.
