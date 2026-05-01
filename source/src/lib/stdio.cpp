#include "stdio.h"
#include "unistd.h"  // To naprawi błędy 'write', 'read', 'open', 'close'
#include "string.h"  // To naprawi błędy 'strlen'
#include "ams_syscall.h" // Tutaj mamy open, close, read, write
#include <stdarg.h>

extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);
extern "C" size_t strlen(const char* s);

extern "C" {

static FILE __stdin = { 0, 0, 0, 0, 0 };  // FD 0 = Stdin
static FILE __stdout = { 1, 0, 0, 0, 0 }; // FD 1 = Stdout
static FILE __stderr = { 2, 0, 0, 0, 0 }; // FD 2 = Stderr

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;


FILE* fopen(const char* filename, const char* mode) {
    // Syscall 2 = SYS_OPEN
    int fd = (int)ams_syscall(2, (uint64_t)filename, 0, 0, 0, 0);
    
    if (fd < 0) return NULL; // Błąd otwarcia

    FILE* f = (FILE*)malloc(sizeof(FILE));
    f->fd = fd;
    // f->buffer... (jeśli chcesz buforowanie, na razie olej)
    return f;
}

// Przykład poprawki w fdopen:
FILE* fdopen(int fd, const char* mode) {
    FILE* f = (FILE*)malloc(sizeof(FILE));
    if (!f) return (FILE*)0;
    f->fd = fd;
    return f;
}

int fgetc(FILE* stream) {
    if (!stream) return EOF;

    // Obsługa ungetc
    if (stream->has_unget) {
        stream->has_unget = 0;
        return stream->unget_buf;
    }

    char c;
    // --- POPRAWKA TUTAJ: Używamy read() zamiast syscall() ---
    int ret = read(stream->fd, &c, 1);
    // --------------------------------------------------------
    
    if (ret <= 0) return EOF;
    return (unsigned char)c;
}

int ungetc(int c, FILE* stream) {
    if (!stream || c == EOF) return EOF;
    stream->unget_buf = c;
    stream->has_unget = 1;
    return c;
}

int fputc(int c, FILE* stream) {
    char ch = (char)c;
    int fd = (stream == stdout) ? 1 : (stream == stderr) ? 2 : stream->fd;
    write(fd, &ch, 1);
    return c;
}

int fputs(const char* s, FILE* stream) {
    int fd = (stream == stdout) ? 1 : (stream == stderr) ? 2 : stream->fd;
    write(fd, s, strlen(s));
    return 0;
}

int puts(const char* s) {
    if (!s) return -1;
    write(1, s, strlen(s));
    write(1, "\n", 1);
    return 0;
}

void print_dec(int fd, long num) {
    char buf[32];
    int i = 0;
    if (num == 0) {
        char z = '0';
        write(fd, &z, 1);
        return;
    }
    
    int neg = 0;
    if (num < 0) {
        neg = 1;
        num = -num;
    }

    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    if (neg) buf[i++] = '-';

    while (i > 0) {
        write(fd, &buf[--i], 1);
    }
}

void print_hex(int fd, unsigned long num) {
    char buf[32];
    int i = 0;
    const char* digits = "0123456789ABCDEF";
    if (num == 0) {
        char z = '0';
        write(fd, &z, 1);
        return;
    }
    while (num > 0) {
        buf[i++] = digits[num % 16];
        num /= 16;
    }
    write(fd, "0x", 2);
    while (i > 0) write(fd, &buf[--i], 1);
}
}

extern "C" {

// Prawdziwy printf obsługujący %s, %d, %x, %c
int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int fd = (stream == stdout) ? 1 : (stream == stderr) ? 2 : stream->fd;
    const char* p = format;
    
    while (*p) {
        if (*p == '%' && *(p+1)) {
            p++;
            switch (*p) {
                case 's': {
                    char* s = va_arg(args, char*);
                    if (!s) s = (char*)"(null)";
                    write(fd, s, strlen(s));
                    break;
                }
                case 'd': {
                    long d = va_arg(args, int); // int promuje się do int/long w va_arg
                    print_dec(fd, d);
                    break;
                }
                case 'x': 
                case 'p': {
                    unsigned long x = va_arg(args, unsigned long);
                    print_hex(fd, x);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    write(fd, &c, 1);
                    break;
                }
                default:
                    write(fd, "%", 1);
                    write(fd, p, 1);
            }
        } else {
            write(fd, p, 1);
        }
        p++;
    }
    
    va_end(args);
    return 0;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    // Wołamy fprintf na stdout, ale musimy przekazać va_list.
    // Żeby nie komplikować, skopiujmy logikę (w prawdziwym libC robi się vfprintf)
    // Ale tutaj zrobimy hacka i po prostu użyjemy kodu wyżej.
    // DLA UPROSZCZENIA: Zróbmy tak, że printf woła fprintf(stdout...)
    // Ale w C++ nie przekazemy "..." dalej łatwo bez vfprintf.
    // Więc wklej ciało funkcji fprintf tutaj, tylko zmieniając fd na 1.
    
    int fd = 1; // stdout
    const char* p = format;
    while (*p) {
        if (*p == '%' && *(p+1)) {
            p++;
            switch (*p) {
                case 's': { char* s = va_arg(args, char*); if(!s) s = (char*)"(null)"; write(fd, s, strlen(s)); break; }
                case 'd': { long d = va_arg(args, int); print_dec(fd, d); break; }
                case 'x': { unsigned long x = va_arg(args, unsigned long); print_hex(fd, x); break; }
                case 'c': { char c = (char)va_arg(args, int); write(fd, &c, 1); break; }
                default: write(fd, "%", 1); write(fd, p, 1);
            }
        } else {
            write(fd, p, 1);
        }
        p++;
    }
    va_end(args);
    return 0;
}
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    // Syscall 0 = SYS_READ
    uint64_t bytes_requested = size * nmemb;
    uint64_t bytes_read = ams_syscall(0, stream->fd, (uint64_t)ptr, bytes_requested, 0, 0);
    return bytes_read / size;
}

int fclose(FILE* stream) {
    // Syscall 3 = SYS_CLOSE
    ams_syscall(3, stream->fd, 0, 0, 0, 0);
    free(stream);
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    return (lseek(stream->fd, offset, whence) < 0) ? -1 : 0;
}

long ftell(FILE *stream) {
    if (!stream) return -1;
    return (long)lseek(stream->fd, 0, SEEK_CUR);
}

int remove(const char *pathname) {
    return unlink(pathname);
}

int rename(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    return -1;
}


// TCC używa też fwrite
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t total = size * nmemb;
    int fd = (stream == stdout) ? 1 : (stream == stderr) ? 2 : stream->fd;
    return write(fd, (const char*)ptr, total) / size;
}

// TCC używa fprintf na stderr
FILE* get_stderr() { return stderr; }
FILE* get_stdout() { return stdout; }
int putchar(int c) { return fputc(c, stdout); }
int putc(int c, FILE* stream) { return fputc(c, stream); }
int fflush(FILE* stream) { (void)stream; return 0; }
int sscanf(const char *str, const char *format, ...) {
    if (!str || !format) return 0;
    va_list ap;
    va_start(ap, format);

    while (*str == ' ' || *str == '\t' || *str == '\n') str++;
    while (*format == ' ') format++;

    int rc = 0;
    if (strcmp(format, "%x") == 0 || strcmp(format, "0x%x") == 0 || strcmp(format, "0X%x") == 0) {
        unsigned int* out = va_arg(ap, unsigned int*);
        unsigned int v = 0;
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) str += 2;
        while ((*str >= '0' && *str <= '9') || (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F')) {
            unsigned int d = 0;
            if (*str >= '0' && *str <= '9') d = (unsigned int)(*str - '0');
            else if (*str >= 'a' && *str <= 'f') d = 10u + (unsigned int)(*str - 'a');
            else d = 10u + (unsigned int)(*str - 'A');
            v = (v << 4) | d;
            str++;
            rc = 1;
        }
        if (rc && out) *out = v;
    } else if (strcmp(format, "%d") == 0 || strcmp(format, "%i") == 0) {
        int* out = va_arg(ap, int*);
        int neg = 0;
        if (*str == '-') { neg = 1; str++; }
        int v = 0;
        while (*str >= '0' && *str <= '9') {
            v = v * 10 + (*str - '0');
            str++;
            rc = 1;
        }
        if (rc && out) *out = neg ? -v : v;
    } else if (strcmp(format, "%o") == 0 || strcmp(format, "0%o") == 0) {
        unsigned int* out = va_arg(ap, unsigned int*);
        unsigned int v = 0;
        if (*str == '0') str++;
        while (*str >= '0' && *str <= '7') {
            v = (v << 3) | (unsigned int)(*str - '0');
            str++;
            rc = 1;
        }
        if (rc && out) *out = v;
    }

    va_end(ap);
    return rc;
}

}


