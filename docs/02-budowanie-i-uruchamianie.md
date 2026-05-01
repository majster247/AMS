# Budowanie i uruchamianie

[PL](./02-budowanie-i-uruchamianie.md) | [EN](./en/02-build-and-run.md)

Ta sekcja definiuje oficjalny pipeline build/release dla AMS-OS.

## 1. Makefile jako pojedyncze źródło prawdy

Plik `source/Makefile` steruje całym procesem:

- kompilacją i linkowaniem kernela,
- kompilacją runtime i aplikacji user-space,
- przygotowaniem `initrd`, ISO i obrazu EXT2,
- uruchamianiem systemu w wariantach QEMU.

## 2. Profil kompilacji kernela (ring 0)

Kernel budowany jest jako freestanding, bez zależności runtime hosta:

- `-ffreestanding` - brak założeń o libc/systemie hosta,
- `-mno-red-zone` - bezpieczeństwo względem przerwań i stosu kernela,
- `-mno-mmx -mno-sse -mno-sse2` - ograniczenie instrukcji we wczesnym etapie,
- `-fno-exceptions -fno-rtti -fno-stack-protector` - uproszczony, kontrolowany runtime.

## 3. Profil user-space (ring 3)

Aplikacje użytkownika linkowane są linker scriptem `src/gui/apps/user.ld` z lokalnym runtime (`src/lib/*`).

To oznacza, że AMS-OS utrzymuje własny minimalny ABI userspace:

- własne `crt0`,
- własny syscall bridge,
- własne implementacje `string/stdio/stdlib/ctype`.

## 4. Artefakty i ich rola

- `kernel.elf` - jądro ładowane przez GRUB,
- `ams_run.iso` - nośnik bootowalny,
- `disk_run.img` - dysk danych i narzędzi dla userspace,
- `build/*.elf` - binaria procesów użytkownika.

## 5. Etap tworzenia `disk_run.img`

W trakcie buildu Makefile:

1. tworzy surowy obraz (`qemu-img`),
2. formatuje go jako EXT2,
3. montuje loopback,
4. buduje strukturę katalogów (`/programs`, `/tools`, `/tests`, ...),
5. kopiuje binaria, runtime, testy i payload stage,
6. synchronizuje i odmontowuje obraz.

Ten obraz jest równorzędnym elementem systemu, nie tylko dodatkiem testowym.

## 6. Etap tworzenia `ams_run.iso`

Pipeline ISO:

1. umieszczenie kernela w `iso/boot`,
2. zbudowanie `initrd.tar`,
3. wygenerowanie `grub.cfg`,
4. złożenie obrazu przez `grub-mkrescue`.

## 7. Uruchamianie w QEMU

- `make run` - standardowy GUI + AHCI + obraz dysku,
- `make run_virgl` - wariant GL (`virtio-vga-gl`),
- `make run_nograph` - tryb terminalowy do debugowania.

## 8. Wzorzec operacyjny dla contributorów

Dla spójności zespołu rekomendowana sekwencja:

1. `make clean`,
2. `make all`,
3. `make run_nograph` (walidacja logów),
4. `make run` (walidacja GUI/UX),
5. aktualizacja dokumentacji przy każdej zmianie ABI/pipeline.
