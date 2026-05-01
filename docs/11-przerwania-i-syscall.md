# Przerwania i syscall path

[PL](./11-przerwania-i-syscall.md) | [EN](./en/11-interrupts-and-syscall-path.md)

## 1. Rola GDT i IDT

`GDT` definiuje segmenty i poziomy uprzywilejowania, a `IDT` mapuje wyjątki i przerwania na handlery kernela.  
Te dwie struktury są absolutnym fundamentem stabilnego przełączania kontekstu.

## 2. Inicjalizacja ścieżki przerwań

W trakcie bootu kernel:

1. inicjalizuje GDT,
2. inicjalizuje IDT,
3. remapuje PIC,
4. aktywuje przerwania (`sti`) po wejściu w stabilny stan.

## 3. Syscall entry

Warstwa architektury (`arch/x86_64`) obsługuje wejście syscall z ring 3 do ring 0.  
Ważny element to poprawny kernel stack przypisany do bieżącego taska.

## 4. Powrót z syscala

Po obsłudze syscalla kernel:

- przywraca kontekst użytkownika,
- zwraca wartość do userspace,
- kontynuuje wykonanie procesu bez naruszenia izolacji przestrzeni adresowej.

## 5. Przejście do ring 3 przez `iretq`

Kod `user_jump.s` buduje frame powrotu i używa `iretq` do zmiany CPL.  
Błędne `CS/SS` lub uszkodzony frame prowadzi do natychmiastowych wyjątków, dlatego ta ścieżka musi być maksymalnie deterministyczna.

## 6. Typowe klasy błędów

- niepoprawne deskryptory GDT,
- nieprawidłowo zainstalowane entry w IDT,
- niespójny kernel stack przy wejściu syscall,
- zły układ ramki dla `iretq`.

## 7. Zalecenia jakościowe

Profesjonalny standard utrzymania tej warstwy:

- każda zmiana w `arch/x86_64` wymaga testu uruchomienia userspace,
- logowanie krytycznych przejść ring 0/ring 3 powinno być utrzymane,
- regresje w tej warstwie traktować jako blocker dla release.
