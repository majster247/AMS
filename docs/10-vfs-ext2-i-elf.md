# VFS, EXT2 i ELF loader

[PL](./10-vfs-ext2-i-elf.md) | [EN](./en/10-vfs-ext2-elf.md)

## 1. VFS jako wspólny interfejs plików

AMS-OS używa warstwy VFS jako abstrakcji nad źródłami danych (initrd, EXT2, pliki tymczasowe i placeholdery).  
Dzięki temu `sys_exec` i aplikacje korzystają z jednej semantyki ścieżek.

## 2. Operacje bazowe

Najczęstsze operacje:

- `vfs_find(path)` - wyszukanie węzła,
- `vfs_read(node, off, size, dst)` - odczyt danych,
- zarządzanie metadanymi `vfs_node` (typ, rozmiar, źródło, dane).

## 3. EXT2 w czasie bootu

Po inicjalizacji PCI/AHCI kernel próbuje podłączyć EXT2.  
Jeśli montowanie się powiedzie, system zyskuje trwały backend plików poza initrd.

## 4. ELF loader - ścieżka danych

Podczas `sys_exec`:

1. ELF jest czytany przez VFS,
2. parser odczytuje `Ehdr/Phdr`,
3. segmenty `PT_LOAD` są mapowane do pamięci procesu,
4. zawartość segmentów jest kopiowana,
5. entrypoint staje się adresem startowym procesu.

## 5. Obsługa `PT_INTERP`

Jeśli binarka zawiera `PT_INTERP`, kernel może uruchomić interpreter i przekazać mu program docelowy jako argument.  
To przybliża AMS-OS do unixowego modelu uruchamiania dynamicznych ELF.

## 6. Placeholder i zgodność ścieżek

W wybranych przypadkach kernel tworzy placeholder w VFS, aby uprościć ścieżki build/exec podczas bootstrappingu (np. dla automatycznego generowania plików wynikowych).

## 7. Dobre praktyki przy rozwijaniu loadera

- waliduj `e_phnum`, `e_phoff`, `p_filesz/p_memsz`,
- loguj każdy `PT_LOAD` i finalne mapowania,
- pilnuj spójności ścieżek VFS i payloadu kopiowanego do `disk_run.img`,
- utrzymuj testy TCC jako szybki test regresji loadera.
