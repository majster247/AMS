# Pamięć krok po kroku

[PL](./09-pamiec-krok-po-kroku.md) | [EN](./en/09-memory-step-by-step.md)

Ta sekcja opisuje jak AMS-OS zarządza pamięcią fizyczną i wirtualną oraz jak pamięć procesu jest budowana podczas `exec`.

## 1. PMM - zarządzanie ramkami fizycznymi

PMM odpowiada za:

- ewidencję ramek fizycznych,
- alokację i zwalnianie ramek,
- raportowanie stanu mapy pamięci.

Po starcie kernela PMM jest inicjalizowany na podstawie informacji o RAM i oznacza obszary wolne/zarezerwowane.

## 2. VMM - mapowanie stron

VMM odpowiada za:

- tworzenie tablic stron kernela i użytkownika,
- mapowanie `virt -> phys`,
- odczyt mapowań (`vmm_get_phys_ex`),
- wsparcie przestrzeni adresowej per-proces (`new_pml4`).

## 3. HHDM i dostęp kernela do pamięci fizycznej

Kernel używa offsetu HHDM, dzięki czemu może pisać do ramek fizycznych przez ich aliasy w przestrzeni wirtualnej kernela.  
To kluczowe przy:

- zerowaniu nowo alokowanych stron,
- kopiowaniu segmentów ELF do mapowań procesu.

## 4. Pamięć procesu podczas `sys_exec`

Przy uruchamianiu procesu:

1. tworzony jest nowy PML4,
2. mapowane są wszystkie `PT_LOAD`,
3. strony są zerowane,
4. dane segmentów kopiowane są z pliku ELF do pamięci procesu.

To buduje pełen obraz pamięci aplikacji zanim otrzyma ona CPU.

## 5. Stos użytkownika

Kernel mapuje obszar stosu i umieszcza na nim:

- argumenty,
- tablice wskaźników,
- `auxv`,
- `argc`.

Alignment stosu jest wymuszany zgodnie z x86_64 ABI.

## 6. Przełączenie przestrzeni adresowej

Po przygotowaniu pamięci:

- task otrzymuje nowe `CR3`,
- resetowane są pola zależne od starej przestrzeni (np. stan `brk`),
- wykonywany jest skok do entrypointa userspace.

To moment, w którym proces zaczyna działać we własnej izolacji pamięci.

## 7. Typowe źródła błędów pamięci

- nieuwzględnienie offsetu HHDM,
- mapowanie niepełnego zakresu segmentu,
- niezerowane strony,
- niespójność `CR3` i stanu taska po `exec`.

W praktyce większość błędów ring 3 wynika z tych czterech obszarów.
