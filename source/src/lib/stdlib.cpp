// Zastąp/Uzupełnij src/lib/stdlib.cpp

#include "ams_syscall.h"
#include "ctype.h" // upewnij się, że masz ctype.h z isspace, isdigit, isalpha

static uint64_t heap_current = 0x70000000;

extern "C" {

    char *__env_internal[] = { 0 }; 
    char **environ = __env_internal;

void* malloc(size_t size) {
    void* addr = (void*)heap_current;
    heap_current += size;
    // Tutaj w wersji PRO powinieneś wołać syscall "sbrk" lub "mmap"
    // Ale na start, skoro zmapowałeś 4GB jako Identity, to zadziała "na chama"
    return addr;
}

void free(void* ptr) {
    (void)ptr; // Na razie nic nie rób, niech przecieka - TCC i tak zaraz skończy
}

// Potrzebne dla TCC (nawet jako atrapy)
void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    
    // NAJPROSTSZY REALLOC: Malloc + Memcpy + Free
    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    
    // Kopiujemy na pałę (nie wiemy ile stary miał, zakładamy że size)
    // To jest niebezpieczne, ale w 99% przypadków TCC powiększa bufory.
    // W prawdziwym OS malloc trzyma wielkość bloku przed wskaźnikiem.
    extern void* memcpy(void*, const void*, size_t);
    memcpy(new_ptr, ptr, size); // Ryzykowne, ale zadziała na start
    free(ptr);
    return new_ptr;
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = malloc(total);
    if (ptr) {
        extern void* memset(void*, int, size_t);
        memset(ptr, 0, total);
    }
    return ptr;
}

long strtol(const char* nptr, char** endptr, int base) {
    const char *s = nptr;
    unsigned long acc;
    int c;
    unsigned long cutoff;
    int neg = 0, any, cutlim;

    while (isspace(*s)) s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1]; s += 2; base = 16;
    }
    if (base == 0) base = c == '0' ? 8 : 10;

    acc = any = 0;
    while ((c = *s++)) {
        if (isdigit(c)) c -= '0';
        else if (isalpha(c)) c -= (isupper(c) ? 'A' - 10 : 'a' - 10);
        else break;
        if (c >= base) break;
        acc *= base;
        acc += c;
        any = 1;
    }
    if (endptr) *endptr = (char *)(any ? s - 1 : nptr);
    return (neg ? -acc : acc);
}

// Atrapy dla floatów (TCC ich wymaga do linkowania, ale możemy zwrócić 0)
double strtod(const char* nptr, char** endptr) { 
    if(endptr) *endptr = (char*)nptr; 
    return 0.0; 
}
float strtof(const char* nptr, char** endptr) { return 0.0f; }
unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long)strtol(nptr, endptr, base);
}

extern "C" void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // Bardzo słaby, ale działający bubble sort dla TCC
    char *base_ptr = (char *)base;
    for (size_t i = 0; i < nmemb; i++) {
        for (size_t j = 0; j < nmemb - 1; j++) {
            if (compar(base_ptr + j * size, base_ptr + (j + 1) * size) > 0) {
                for (size_t k = 0; k < size; k++) {
                    char tmp = base_ptr[j * size + k];
                    base_ptr[j * size + k] = base_ptr[(j + 1) * size + k];
                    base_ptr[(j + 1) * size + k] = tmp;
                }
            }
        }
    }
}

}