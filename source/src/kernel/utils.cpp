#include <stdint.h>
#include <stdarg.h>
#include "kernel.h"
#include "io.h"
#include "task.h"
#include "heap.h"

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

 // 

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

//sprintf trzeba w końcu napisać w całości
void sprintf(char* buffer, const char* format, ...) {
    // Prosta implementacja obsługująca tylko %d i %s oraz %x i %u i %lu oraz %f
    const char* traverse = format;
    char* buf_ptr = buffer;

    va_list args;
    va_start(args, format);

    while (*traverse) {
        if (*traverse == '%') {
            traverse++;
            if (*traverse == 'd') {
                int num = va_arg(args, int);
                // Konwersja int na string (bez sprintf)
                char num_buffer[12]; // -2147483648 do 2147483647
                int num_len = 0;
                if (num < 0) {
                    *buf_ptr++ = '-';
                    num = -num;
                }
                do {
                    num_buffer[num_len++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0);
                for (int i = num_len - 1; i >= 0; i--) {
                    *buf_ptr++ = num_buffer[i];
                }
            } else if (*traverse == 's') {
                const char* str_arg = va_arg(args, const char*);
                while (*str_arg) {
                    *buf_ptr++ = *str_arg++;
                }
            } else if (*traverse == 'x') {
                unsigned int num = va_arg(args, unsigned int);
                char num_buffer[9]; // 8 znaków + null terminator
                int num_len = 0;
                do {
                    int digit = num & 0xF;
                    num_buffer[num_len++] = "0123456789ABCDEF"[digit];
                    num >>= 4;
                } while (num > 0);
                for (int i = num_len - 1; i >= 0; i--) {
                    *buf_ptr++ = num_buffer[i];
                }
            } else if (*traverse == 'u') {
                unsigned int num = va_arg(args, unsigned int);
                char num_buffer[11]; // max 10 cyfr + null terminator
                int num_len = 0;
                do {
                    num_buffer[num_len++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0);
                for (int i = num_len - 1; i >= 0; i--) {
                    *buf_ptr++ = num_buffer[i];
                }
            } else if (*traverse == 'l' && *(traverse + 1) == 'u') {
                traverse++; // Skip 'l'
                unsigned long num = va_arg(args, unsigned long);
                char num_buffer[21]; // max 20 digits for 64-bit + null terminator
                int num_len = 0;
                do {
                    num_buffer[num_len++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0);
                for (int i = num_len - 1; i >= 0; i--) {
                    *buf_ptr++ = num_buffer[i];
                }
            }else if (*traverse == 'f') {
                //niestety nie możemy używać va_arg bo nie ma SSE więc trzeba pobrać double jako uint64_t i bez używania va_arg
                uint64_t num = 0; // na razie tak bo nie możemy użyć va_arg dla double bez SSE, więc musimy ręcznie pobrać 64 bity z argumentów
                if (num < 0) {
                    *buf_ptr++ = '-';
                    num = -num;
                }
                int int_part = (int)num;
                double frac_part = num - int_part;

                // Konwersja części całkowitej
                char int_buffer[12];
                int int_len = 0;
                do {
                    int_buffer[int_len++] = '0' + (int_part % 10);
                    int_part /= 10;
                } while (int_part > 0);
                for (int i = int_len - 1; i >= 0; i--) {
                    *buf_ptr++ = int_buffer[i];
                }

                *buf_ptr++ = '.'; // Separator dziesiętny

                // Konwersja części ułamkowej (2 miejsca po przecinku)
                for (int i = 0; i < 2; i++) {
                    frac_part *= 10;
                    int digit = (int)frac_part;
                    *buf_ptr++ = '0' + digit;
                    frac_part -= digit;
                }
            } else {
                // Nieobsługiwany specyfikator, po prostu go wypisz
                *buf_ptr++ = '%';
                *buf_ptr++ = *traverse;
            }
        } else {
            *buf_ptr++ = *traverse;
        }
        traverse++;
    }
    *buf_ptr = '\0';
    va_end(args);
}

//fast memcpy dla 64-bitowych wartości (np. do kopiowania bitmapy)
//PO CO: bo memcpy jest strasznie wolny, a często kopiujemy duże bloki danych (np. tapetę)
// i chcemy to robić jak najszybciej, zwłaszcza przy rysowaniu tapety czy okienek GUI

//Jak to działa: zamiast kopiować bajt po bajcie, kopiujemy całe 64-bitowe słowa (8 bajtów) na raz. 
//To jest znacznie szybsze, zwłaszcza dla dużych bloków danych. Oczywiście musimy upewnić się, 
//że zarówno źródło, jak i cel są odpowiednio wyrównane (aligned), ale w naszym przypadku, 
//gdy kopiujemy bitmapę do backbuffera, możemy to zagwarantować.

//Przeskok w prędkości: PRZED: 1280x720x4 bajty = 3,686,400 operacji kopiowania bajt po bajcie. 
//PO: 1280x720/8 = 115,200 operacji kopiowania słowo po słowie. 
//To jest ogromna różnica i znacząco przyspiesza rysowanie tapety czy okienek GUI!

// Kopiuje pamięć używając 64-bitowych rejestrów (8 bajtów na cykl + optymalizacje CPU)
// count to liczba 64-bitowych słów (czyli liczba bajtów / 8)
extern "C" void fast_memcpy64(void* dst, const void* src, uint64_t count) {
    asm volatile (
        "cld; rep movsq" 
        : "+D" (dst), "+S" (src), "+c" (count) // Output registers
        : // No input registers explicitly needed
        : "memory" // Clobbers memory
    );
}


extern "C" void __cxa_pure_virtual() {
    // We can't use standard IO here usually, just hang or print to serial
    extern void write_serial_string(const char*);
    write_serial_string("CRASH: Pure Virtual Function Call!\n");
    while (1) asm volatile("hlt");
}

//__cxa_guard_release to funkcja wywoływana przez konstruktor statycznych obiektów C++ do zwolnienia mutexa po zakończeniu inicjalizacji.
extern "C" int __cxa_guard_release(int* guard) {
    // W prostym kernelu możemy po prostu ustawić wartość na 1, co oznacza, że obiekt został zainicjalizowany
    *guard = 1;
    return 1; // Zwracamy 1, aby poinformować, że inicjalizacja zakończyła się sukcesem
}

// __cxa_guard_acquire i __cxa_guard_abort są opcjonalne, ale można je zaimplementować, jeśli chcemy obsłużyć przypadki, gdy inicjalizacja obiektu statycznego nie powiedzie się (np. z powodu wyjątku). W naszym prostym kernelu możemy je pominąć lub zaimplementować jako no-op.
extern "C" int __cxa_guard_acquire(int* guard) {
    // Jeśli guard jest 0, to obiekt nie został jeszcze zainicjalizowany, więc próbujemy go zainicjalizować
    // W prostym kernelu bez wielowątkowości możemy po prostu zwrócić 1, co oznacza, że możemy przejść do inicjalizacji
    return (*guard == 0);
}

extern "C" void __cxa_guard_abort(int* guard) {
    // Jeśli inicjalizacja obiektu statycznego nie powiedzie się, możemy ustawić guard na -1, aby oznaczyć, że inicjalizacja zakończyła się niepowodzeniem
    *guard = -1;
}


uint64_t get_time_ms() {
    // Prosta funkcja do zwracania czasu w ms od startu systemu
    // Zakładamy, że system_ticks jest aktualizowany przez timer_handler
    return (system_ticks * 1000) / hz;
}


//strncpy - kopiuje n znaków z src do dest, dodając null terminator
extern "C" char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    if (i < n) {
        dest[i] = '\0'; // Null terminator
    }
    return dest;
}
