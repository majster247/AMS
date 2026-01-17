#include <stdint.h>

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