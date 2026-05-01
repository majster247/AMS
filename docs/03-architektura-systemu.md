# Architektura systemu

[PL](./03-architektura-systemu.md) | [EN](./en/03-system-architecture.md)

## 1. Warstwy systemu

AMS-OS używa klasycznego podziału warstw, z jasną separacją uprawnień:

- **Boot layer** - GRUB + przekazanie kontroli do kernela.
- **Kernel layer (ring 0)** - pamięć, przerwania, scheduler, I/O, VFS.
- **User layer (ring 3)** - procesy ELF i narzędzia systemowe.

## 2. Rdzeń architektury kernelowej

Centralne podsystemy:

- `arch/x86_64` - GDT, IDT, wejście/wyjście syscall, ring transition,
- `memory` - PMM, VMM, heap,
- `drivers` - serial, timer, input, storage, video,
- `fs` - VFS, loader ELF, EXT2,
- `tasking` - scheduler i kontekst zadania,
- `kernel` - orkiestracja startupu i ścieżek wykonania.

## 3. Model ochrony i granice odpowiedzialności

AMS-OS utrzymuje twardą granicę:

- kernel mapuje i przygotowuje przestrzeń procesu,
- użytkownik uruchamia się wyłącznie przez kontrolowany punkt wejścia (`iretq`),
- syscall path jest jedyną oficjalną drogą przejścia ring 3 -> ring 0.

## 4. Przepływ życia procesu

Proces user-space przechodzi przez:

1. lookup binarki w VFS,
2. walidację ELF i mapowanie segmentów,
3. budowę stosu (`argc/argv/envp/auxv`),
4. przypisanie nowego `CR3`,
5. skok do entrypointa użytkownika.

To podejście daje AMS-OS semantykę zbliżoną do systemów unixowych, przy zachowaniu prostoty implementacji.

## 5. Interfejs systemowy

Warstwa user-space (`src/lib`) pełni rolę mini-libc i runtime:

- `crt0` uruchamia program,
- wrappery syscall stanowią ABI między aplikacją a jądrem,
- podstawowe biblioteki C zapewniają przenośną bazę dla narzędzi (`tcc`, `gcc`, `bash`).

## 6. Strategia startu sesji użytkownika

Po inicjalizacji podsystemów kernel:

1. przygotowuje środowisko narzędzi i aplikacji,
2. uruchamia domyślną ścieżkę Wayland,
3. utrzymuje fallback do klasycznego desktopu AMS.

To umożliwia równoległy rozwój starszego GUI i nowego stacka Wayland.
