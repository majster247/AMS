# Aplikacje i narzędzia

[PL](./05-aplikacje-i-narzedzia.md) | [EN](./en/05-apps-and-tools.md)

## 1. Aplikacje userspace budowane przez pipeline

AMS-OS buduje zestaw aplikacji demonstracyjnych i systemowych:

- `hello.elf` - minimalny test procesu user-space,
- `tcc.elf` - lekki kompilator C uruchamiany wewnątrz systemu,
- `gcc.elf` - narzędzie kompilacyjne (ścieżka rozszerzania toolchaina),
- `bash.elf` - shell userspace,
- `jobctl.elf` - kontrola jobów/procesów,
- `apt.elf` - prototyp menedżera pakietów,
- `doom.elf` - aplikacja wymagająca szerszego runtime,
- `wayland_*` - smoke testy i komponenty sesji Wayland.

## 2. Rozmieszczenie w obrazie dysku

`disk_run.img` rozdziela payload logicznie:

- `/tools/system` - narzędzia operacyjne,
- `/tools/compiler` - kompilator + runtime + nagłówki,
- `/tools/toolchain` - payload hostowego toolchaina (jeśli dostępny),
- `/programs/doom` - aplikacja i assety,
- `/programs/wayland` - komponenty sesji Wayland.

## 3. Runtime kompilatorów

Kluczowe pliki runtime (`crt0.o`, `syscall.o`, `ams_syscall.o`, `stdlib.o`, `string.o`, `ctype.o`) są kopiowane tak, aby kompilacja i uruchamianie kodu w AMS mogły działać bez odwołań do hostowego systemu.

## 4. Doom jako test ścieżki end-to-end

Doom to istotny benchmark integracyjny:

- weryfikuje loader ELF i runtime userspace,
- testuje I/O, argumenty i wykonanie aplikacji większej niż proste testy C,
- wykorzystuje bootstrap oraz WAD (`freedoom1.wad`) dostarczane w obrazie.

## 5. Wayland jako nowy tor GUI

Stack Wayland w AMS służy do przejścia z klasycznego desktopu na bardziej nowoczesny model sesji:

- compositor uruchamiany jako domyślna sesja,
- smoke testy klient-serwer,
- wsparcie stage dla artefaktów Mesa/Wayland.

## 6. Rekomendacja produktowa

Aby zachować jakość jak w projektach klasy systemowej:

- każda nowa aplikacja powinna mieć sekcję dokumentacji (cel, ABI, ścieżki, zależności),
- każda zmiana ABI musi aktualizować dokumenty userspace i build pipeline,
- payload na `disk_run.img` powinien być wersjonowany i audytowalny.
