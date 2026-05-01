# GitHub Pages i publikacja

[PL](./07-github-pages.md) | [EN](./en/07-github-pages.md)

## 1. Model publikacji

Dokumentacja działa jako statyczna strona Jekyll oparta wyłącznie o Markdown.  
Nie wymaga Node, React ani dodatkowego kroku build na Twojej maszynie.

## 2. Ustawienie GitHub Pages

W repozytorium:

1. `Settings -> Pages`,
2. `Build and deployment -> Deploy from a branch`,
3. branch: główny (`main`/`master`),
4. folder: `/docs`.

Po deployu serwis wystawi stronę pod adresem projektu GitHub Pages.

## 3. Custom domain

Dokumentacja zawiera `docs/CNAME` z domeną:

- `ams-os.enigmasec.studio`

Warunki działania:

1. poprawne rekordy DNS do GitHub Pages,
2. domena ustawiona także w panelu Pages,
3. certyfikat HTTPS aktywny (opcja w panelu GitHub).

## 4. Zasady utrzymania

Aby dokumentacja zachowała poziom profesjonalny:

- każda większa zmiana architektury wymaga aktualizacji odpowiednich sekcji,
- każda zmiana build pipeline wymaga aktualizacji `01` i `02`,
- każda zmiana ABI/syscall wymaga aktualizacji `04` i `11`.

## 5. Minimalny standard PR dla docs

Każdy PR dotykający rdzenia systemu powinien zawierać:

- opis wpływu na architekturę,
- wpływ na kompatybilność userspace,
- wpływ na debugowanie/testy,
- aktualizację co najmniej jednej strony dokumentacji.
