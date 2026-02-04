#include <stdint.h>
#include "kernel.h"
#include "io.h"

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

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
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

extern "C" void* memset(void* dest, int ch, size_t count) {
    uint8_t* p = (uint8_t*)dest;
    while (count--) {
        *p++ = (uint8_t)ch;
    }
    return dest;
}

extern "C" void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

static unsigned long int next = 1;

int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}

//timer sleep function

volatile uint64_t system_ticks = 0;
const int hz = 100; // 100 przerwani na sekundę (co 10ms)

void timer_handler() {
    system_ticks++;
}



#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

void get_time(int &h, int &m, int &s) {
    // Czekamy aż RTC nie będzie w trakcie aktualizacji
    while (read_cmos(0x0A) & 0x80);

    s = read_cmos(0x00);
    m = read_cmos(0x02);
    h = read_cmos(0x04);

    // Konwersja BCD na zwykły int
    s = (s & 0x0F) + ((s / 16) * 10);
    m = (m & 0x0F) + ((m / 16) * 10);
    h = (h & 0x0F) + ((h / 16) * 10);
    
    h = (h + 1) % 24; // Korekta strefy czasowej (np. +1 dla Polski)
}

//base sleep on system tics and cmos
void sleep(uint32_t ms) {
    uint64_t start_ticks = system_ticks;
    uint64_t wait_ticks = (ms * hz) / 1000;

    while ((system_ticks - start_ticks) < wait_ticks) {
        asm volatile("hlt"); // Uśpij CPU do następnego przerwania
    }
}

