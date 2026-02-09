// Definiujemy flagi, żeby TCC wiedział, że jest na AMS
#define TCC_TARGET_X86_64
#define ONE_SOURCE 1 
#define CONFIG_TCC_STATIC

// Dołączamy nasze nagłówki systemowe
#include "../../lib/ams_syscall.h"
#include "../../lib/stdio.h"
#include "../../lib/stdlib.h" // Jeśli masz, albo zadeklaruj malloc/free
#include "../../lib/string.h" // Jeśli masz

// Hack: TCC potrzebuje tych nagłówków standardowych. 
// Oszukamy go, definiując puste makra lub includując nasze wersje.
#define _STDIO_H // Żeby TCC nie szukał <stdio.h> w systemie hosta
#define _STDLIB_H
#define _STRING_H

// Includujemy CAŁE TCC jako jeden plik
#include "tcc.c"