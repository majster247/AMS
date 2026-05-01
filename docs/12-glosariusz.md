# Glosariusz

[PL](./12-glosariusz.md) | [EN](./en/12-glossary.md)

## Pojęcia bazowe

- **Ring 0** - poziom uprzywilejowany (kernel mode).
- **Ring 3** - poziom użytkownika (user mode).
- **CPL** - Current Privilege Level procesora.
- **GDT** - Global Descriptor Table.
- **IDT** - Interrupt Descriptor Table.
- **ISR** - Interrupt Service Routine.
- **ELF** - standard formatu binarek i obiektów.
- **PT_LOAD** - segment ELF mapowany do pamięci procesu.
- **PT_INTERP** - segment ELF wskazujący interpreter/loader.
- **PML4** - najwyższy poziom tablic stron x86_64.
- **CR3** - rejestr wskazujący aktywną przestrzeń adresową.
- **PMM** - Physical Memory Manager.
- **VMM** - Virtual Memory Manager.
- **HHDM** - wyższe mapowanie pamięci fizycznej do przestrzeni kernela.
- **VFS** - Virtual File System, wspólna abstrakcja plików.
- **EXT2** - system plików używany jako backend dysku AMS.
- **initrd** - archiwum inicjalizacyjne ładowane przez bootloader.
- **ABI** - kontrakt binarny między komponentami.
- **syscall** - kontrolowane wejście z user mode do kernel mode.
- **iretq** - instrukcja powrotu z przerwania/zmiany poziomu uprawnień.

## Słownik AMS-OS

- **`sys_exec`** - główny mechanizm uruchamiania procesu userspace.
- **`scheduler_switch_to_user`** - przełączenie wykonania na proces user-space.
- **`jump_to_ring3`** - assemblerowy most ring 0 -> ring 3.
- **`disk_run.img`** - obraz EXT2 z narzędziami i payloadem testowym.
- **`ams_run.iso`** - obraz bootowalny z kernelem i initrd.
