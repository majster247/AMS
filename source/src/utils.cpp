#include <stdint.h>
#include "kernel.h"

static char hex_buffer[20]; // 0x + 16 znaków + null terminator

extern "C" const char* to_hex(uint64_t val) {
    const char* digits = "0123456789ABCDEF";
    hex_buffer[0] = '0';
    hex_buffer[1] = 'x';
    hex_buffer[18] = '\0';

    for (int i = 0; i < 16; i++) {
        // Wypełniamy od końca, aby zachować kolejność bajtów
        hex_buffer[17 - i] = digits[val & 0xF];
        val >>= 4;
    }
    return hex_buffer;
}

extern "C" void write_serial_hex(uint64_t val) {
    const char* hex_str = to_hex(val);
    write_serial_string(hex_str);
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}