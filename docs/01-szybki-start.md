# Szybki start

[PL](./01-szybki-start.md) | [EN](./en/01-quick-start.md)

Ta sekcja prowadzi przez minimalną ścieżkę: od pustego środowiska do działającego AMS-OS w QEMU.

## 1. Klonowanie i wejście do katalogu buildu

```bash
git clone <url-repozytorium>
cd AMS-1/source
```

## 2. Wymagania hosta

AMS-OS wymaga narzędzi systemowych, cross-toolchaina i narzędzi obrazów dysku:

- `make`, `bash`, `tar`, `cp`, `mkdir`,
- `nasm`,
- `x86_64-elf-gcc`, `x86_64-elf-g++`, `x86_64-elf-ld`,
- `qemu-system-x86_64`, `qemu-img`,
- `grub-mkrescue`,
- `mkfs.ext2`, `mount`, `umount`.

Rekomendowane środowisko: Linux lub WSL2.  
Pełny pipeline (`disk_run.img`) korzysta z montowania loopback i narzędzi typowo unixowych.

## 3. Build pełnego systemu

```bash
make all
```

To polecenie buduje:

- kernel (`kernel.elf`),
- aplikacje ring 3 (`build/*.elf`),
- obraz bootowalny (`ams_run.iso`),
- obraz dysku EXT2 (`disk_run.img`) z narzędziami i testami.

## 4. Start systemu

Standardowy tryb graficzny:

```bash
make run
```

Tryb bez okna GUI (najlepszy do szybkiego debugowania logów):

```bash
make run_nograph
```

Tryb z `virtio-vga-gl`:

```bash
make run_virgl
```

## 5. Najkrótsza diagnoza problemów

Jeśli build nie przechodzi:

1. potwierdź dostępność cross-toolchaina w `PATH`,
2. sprawdź czy host ma `grub-mkrescue` i `mkfs.ext2`,
3. przy błędach dysku sprawdź uprawnienia do `mount` i loopback.

## 6. Cleanup

```bash
make clean
```
