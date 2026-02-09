#pragma once
#include "ams_syscall.h"
#include <stdarg.h>

typedef struct {
    int fd;
    int unget_buf; // Bufor na jeden znak (dla ungetc - parser TCC tego wymaga!)
    int has_unget;
} FILE;

// Standardowe strumienie (w Unixie 0=stdin, 1=stdout, 2=stderr)
// W C są to wskaźniki, więc musimy to jakoś zasymulować, ale na razie
// zrobimy proste define'y lub atrapy w .cpp
#define stdin  ((FILE*)0)
#define stdout ((FILE*)1)
#define stderr ((FILE*)2)

#define fileno(F) ((F)->fd)

#define EOF (-1)

extern "C" {
    FILE* fopen(const char* filename, const char* mode);
    int fclose(FILE* stream);
    int fgetc(FILE* stream);
    int ungetc(int c, FILE* stream);
    int fputc(int c, FILE* stream);
    int fputs(const char* s, FILE* stream);
    
    // Uproszczony printf (zrobimy go później dokładnie)
    int printf(const char* format, ...);
    int fprintf(FILE* stream, const char* format, ...);
}