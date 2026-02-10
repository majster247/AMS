// src/lib/string.cpp
#include "string.h"
#include "stdlib.h" // Potrzebne dla malloc w strdup

extern "C" {

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memset(void* s, int c, size_t n) {
    char* p = (char*)s;
    while (n--) *p++ = (char)c;
    return s;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while ((*ptr++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return 0;
    }
    return (char*)s;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new_str = (char*)malloc(len); // malloc jest w stdlib.h
    if (new_str) memcpy(new_str, s, len);
    return new_str;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}



char* strrchr(const char* s, int c) {
    const char* last = 0;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char *p1 = (const unsigned char*)s1;
    const unsigned char *p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char*)haystack;
        }
    }
    return 0;
}

char* strerror(int errnum) {
    return (char*)"Unknown error";
}

// Te funkcje systemowe zazwyczaj są w unistd/stdlib, ale TCC czasem szuka ich "gdziebądź".
// Zostawiamy je tu na razie, bo operują na stringach.
/*
char *realpath(const char *path, char *resolved_path) {
    if (resolved_path) {
        strcpy(resolved_path, path);
        return resolved_path;
    }
    return 0;
}*/

char *getcwd(char *buf, size_t size) {
    if (size > 1) {
        strcpy(buf, "/");
        return buf;
    }
    return 0;
}

} // extern "C"

extern "C" char *getenv(const char *name) { return 0; }
//extern "C" int atoi(const char *nptr) {  extern long strtol(const char*, char**, int); return (int)strtol(nptr, 0, 10); }