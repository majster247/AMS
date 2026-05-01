# AMS-OS Documentation

[PL](./index.md) | [EN](./en/index.md)

AMS-OS to 64-bitowy system operacyjny x86_64 rozwijany jako projekt inżynierski low-level.  
Ta dokumentacja opisuje system tak, jak dokumentuje się dojrzałe platformy: od architektury i bootu, przez model pamięci i procesów, po narzędzia użytkownika i operacje developerskie.

<div class="hero">
  <h2>System Documentation Hub</h2>
  <p>Kompletna dokumentacja AMS-OS: architektura, internals, operacje, debug i workflow contributorski.</p>
</div>

<div class="doc-cards">
  <a class="doc-card" href="./13-getting-started.md"><strong>Getting Started</strong><span>Onboarding, wymagania i pierwszy poprawny run.</span></a>
  <a class="doc-card" href="./14-architecture-overview.md"><strong>Architecture</strong><span>Warstwy systemu i model odpowiedzialności.</span></a>
  <a class="doc-card" href="./15-internals.md"><strong>Internals</strong><span>Ścieżki krytyczne: boot, pamięć, syscalle, ring transition.</span></a>
  <a class="doc-card" href="./16-operations.md"><strong>Operations</strong><span>Codzienna praca: build, run, triage i utrzymanie jakości.</span></a>
</div>

## Cele dokumentacji

- dostarczyć pełny, techniczny opis działania systemu,
- wyjaśnić **krok po kroku** przepływ od startu maszyny do uruchomienia procesu user-space,
- ustandaryzować sposób budowania, uruchamiania i debugowania AMS-OS,
- przygotować bazę pod dalszy rozwój projektu i onboarding nowych contributorów.

## Nawigacja

### Getting Started

1. [Getting Started](./13-getting-started.md)
2. [Szybki start](./01-szybki-start.md)
3. [Budowanie i uruchamianie](./02-budowanie-i-uruchamianie.md)

### Architecture

4. [Architecture Overview](./14-architecture-overview.md)
5. [Architektura systemu](./03-architektura-systemu.md)
6. [Użytkownik, ELF i syscalle](./04-uzytkownik-i-syscalls.md)
7. [Aplikacje i narzędzia](./05-aplikacje-i-narzedzia.md)

### Internals

8. [Internals](./15-internals.md)
9. [Sekwencja bootowania krok po kroku](./08-sekwencja-bootowania.md)
10. [Pamięć krok po kroku](./09-pamiec-krok-po-kroku.md)
11. [VFS, EXT2 i ELF loader](./10-vfs-ext2-i-elf.md)
12. [Przerwania i syscall path](./11-przerwania-i-syscall.md)

### Operations

13. [Operations](./16-operations.md)
14. [Debugowanie i testy](./06-debugowanie.md)
15. [GitHub Pages i publikacja](./07-github-pages.md)

### Project Governance

16. [Contributing](./17-contributing.md)
17. [Release Notes](./18-release-notes.md)
18. [Glosariusz](./12-glosariusz.md)

## Status

Dokumentacja jest utrzymywana razem z kodem źródłowym i powinna być aktualizowana przy każdej istotnej zmianie architektury, ABI lub pipeline builda.
