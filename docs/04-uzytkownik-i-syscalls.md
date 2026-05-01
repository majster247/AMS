# Użytkownik, ELF i syscalle

[PL](./04-uzytkownik-i-syscalls.md) | [EN](./en/04-userspace-elf-and-syscalls.md)

## 1. Model userspace w AMS-OS

AMS-OS uruchamia programy jako ELF64 w oddzielnej przestrzeni adresowej, z własnym stosem i ABI argumentów.

Warstwa userspace zawiera:

- runtime startowy (`crt0`),
- biblioteki bazowe (`string`, `stdio`, `stdlib`, `ctype`),
- syscall bridge do jądra.

## 2. `sys_exec` jako punkt wejścia procesu

Funkcja `sys_exec` w `src/kernel/kernel.cpp` realizuje pełny lifecycle startu procesu:

1. znajduje plik wykonywalny (`vfs_find`),
2. czyta nagłówek i program headers ELF,
3. obsługuje `PT_INTERP` (chaining do interpretera/loadera),
4. tworzy nowy PML4 (`vmm_create_user_pml4`),
5. mapuje segmenty `PT_LOAD`,
6. buduje stos użytkownika,
7. przełącza kontekst i uruchamia kod user-space.

## 3. Budowa stosu procesu

Kernel umieszcza na stosie:

- teksty argumentów,
- tablicę wskaźników `argv`,
- terminatory `argv` i `envp`,
- minimalny zestaw `auxv`,
- `argc`.

Dodatkowo pilnowane jest 16-bajtowe wyrównanie stosu, kluczowe dla x86_64 ABI.

## 4. Przejście ring 0 -> ring 3

Plik `src/arch/x86_64/user_jump.s` przygotowuje frame `iretq`:

- `SS = 0x2B`,
- `RSP = user stack`,
- `RFLAGS = 0x202`,
- `CS = 0x33`,
- `RIP = ELF entrypoint`.

To jest atomowy moment zmiany privilege level i rozpoczęcia wykonania kodu użytkownika.

## 5. Minimalna kompatybilność linuxowego modelu ELF

Obsługa `PT_INTERP` i `auxv` sprawia, że AMS-OS może uruchamiać bardziej złożone ścieżki loaderowe.  
To ważne dla narzędzi typu TCC/GCC i środowisk, które oczekują unix-like zachowania procesu.

## 6. Kontrakt ABI

Publiczny kontrakt dla aplikacji userspace obejmuje:

- nagłówki w `include/` (`ams_syscall.h`, `linux_syscalls.h`, inne libc-like),
- stabilne numery i semantykę syscalli,
- przewidywalny układ stosu przy starcie procesu.
