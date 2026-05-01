# Debugowanie i testy

[PL](./06-debugowanie.md) | [EN](./en/06-debugging.md)

## 1. Standard obserwowalności

AMS-OS opiera diagnostykę głównie na:

- logach serial (`write_serial_string`),
- logach QEMU (`qemu.log`),
- inspekcji kodu wynikowego (`make dump` -> `kernel.asm`).

To jest podstawowy zestaw triage, który powinien być uruchamiany przy każdej regresji.

## 2. Sugerowany workflow debugowania

1. `make clean && make all`,
2. `make run_nograph` i analiza pełnego boot logu,
3. reprodukcja w `make run`,
4. porównanie zachowania z i bez GUI/GL,
5. analiza fragmentu assemblera lub konkretnej ścieżki syscall.

## 3. Debug ścieżki procesu user-space

Przy problemie `sys_exec` sprawdzaj w tej kolejności:

1. dostępność pliku w VFS,
2. poprawność parsera ELF (`Ehdr`, `Phdr`, `PT_LOAD`, `PT_INTERP`),
3. mapowanie stron i adresy fizyczne,
4. layout stosu i alignment,
5. selektory `CS/SS` i przejście `iretq`.

## 4. Debug pamięci

Objawy typu page fault / random crash zwykle wynikają z:

- błędnych mapowań user pages,
- użycia starego stanu taska po zmianie `CR3`,
- pomyłek HHDM vs fizyczny adres.

Weryfikuj spójnie:

- `vmm_get_phys_ex`,
- zerowanie stron po alokacji,
- reset pól taska zależnych od przestrzeni adresowej.

## 5. Testy TCC jako regresja ABI

Zestaw testów `/tests/tcc` służy jako szybki test kompatybilności userspace ABI i runtime.  
Fail testów TCC traktuj jako sygnał regresji w:

- syscallach,
- loaderze ELF,
- mapowaniu stosu/procesu,
- minimalnej libc.

## 6. Klasyczna checklista incydentu

- **Build failed**: brak narzędzi hosta lub problem z PATH.
- **Disk stage failed**: uprawnienia `mount`, brak loopback.
- **App not launching**: plik nie trafia do właściwej ścieżki VFS.
- **Ring3 crash**: niepoprawny stack/auxv/selektory.
- **GUI freeze**: problem schedulera, timera lub sterowników wejścia/wideo.
