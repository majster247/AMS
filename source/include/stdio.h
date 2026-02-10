#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

/* Definicje stałych */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

#ifndef EOF
#define EOF (-1)
#endif

/* Struktura FILE */
typedef struct __FILE {
    int fd;
    int error;
    int eof;
    int has_unget;
    int unget_buf;
} FILE;

/* Makro fileno */
#define fileno(F) ((F)->fd)

#ifdef __cplusplus
extern "C" {
#endif

/* --- ZMIANA: TE DEKLARACJE MUSZĄ BYĆ TU, W ŚRODKU EXTERN C --- */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* --- Funkcje wejścia/wyjścia --- */
FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
FILE *fdopen(int fd, const char *mode);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int ungetc(int c, FILE *stream);

int fflush(FILE *stream);
void perror(const char *s);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

char *tmpnam(char *s);
FILE *tmpfile(void);

#ifdef __cplusplus
}
#endif

#endif
