/**
 * @file kernel.h
 * @author Majster
 * @brief Główny nagłówek jądra - funkcje systemowe, PMM, VMM i narzędzia.
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

extern "C" {
    /** === VMM Page Flags === */
    #define PAGE_PRESENT  (1ULL << 0) /**< Strona obecna w pamięci */
    #define PAGE_WRITABLE (1ULL << 1) /**< Dozwolony zapis */
    #define PAGE_USER     (1ULL << 2) /**< Dozwolony dostęp z Ring 3 */

    /** === KERNEL TERMINAL (Logowanie tekstowe) === */
    void terminal_initialize();
    void terminal_writestring(const char* data);
    void terminal_putchar(char c);
    void terminal_clear();
    void terminal_write_dec(uint32_t val);

    /** === KEYBOARD BUFFER === */
    extern volatile uint32_t kb_read_ptr;
    extern volatile uint32_t kb_write_ptr;
    extern volatile uint8_t kb_buffer[256];
    extern volatile bool key_shift_pressed;
    extern volatile bool key_ctrl_pressed;
    extern volatile bool key_alt_pressed;
    void keyboard_init();
    uint8_t keyboard_get_char();

    /** === SERIAL DEBUG (COM1) === */
    int init_serial();
    void write_serial_string(const char* str);
    void write_serial_dec(uint64_t val);
    void write_serial_hex(uint64_t val);
    void write_serial_char(char c);

    /** === MEMORY MANAGEMENT === */
    /** @brief Parsuje struktury Multiboot2 przekazane przez bootloader */
    void parse_multiboot(uint64_t addr);
    /** @brief Inicjalizuje Physical Memory Manager mapą bitową */
    void pmm_init(uint64_t mem_size, void* bitmap_address);
    /** @brief Alokuje ramkę fizyczną (4096 bajtów) */
    void* pmm_alloc_frame();
    /** @brief Mapuje adres wirtualny na fizyczny w tablicach stron */
    void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);

    /** === UTILS === */
    void* memset(void* dest, int ch, size_t count);
    void* memcpy(void* dest, const void* src, size_t count);
    /** @brief Szybkie kopiowanie pamięci wykorzystujące rejestry 64-bitowe */
    void fast_memcpy64(void* dst, const void* src, uint64_t count);
    /** @brief Formatuje ciąg znaków (podzbiór standardowego printf) */
    void sprintf(char* buffer, const char* format, ...);
    /** @brief Porównuje dwa ciągi znaków (zwraca 0 jeśli równe) */
    int strcmp(const char* s1, const char* s2);
    /** @brief Porównuje dwa ciągi znaków do n znaków (zwraca 0 jeśli równe) */
    char* strncpy(char* dest, const char* src, size_t n);

    /** === TIME === */
    /** @brief Inicjalizuje zegar PIT z wybraną częstotliwością */
    void timer_init(uint32_t freq);
    /** @brief Pobiera aktualny czas z układu RTC */
    void get_time(int &h, int &m, int &s);
    /** @brief Pobiera liczbę milisekund od startu systemu */
    uint64_t get_time_ms();
    /** @brief Pobiera liczbę tików systemowych od startu systemu */
    uint64_t get_system_ticks();

    /** @brief Przełącza kontekst i dodaje zadanie w trybie użytkownika */
    void scheduler_add_user_task(void* entry, void* stack);


    void* pmm_alloc_blocks(size_t block_count);
    void pmm_mark_used(uint64_t start, uint64_t size);
    void pmm_mark_free(uint64_t start, uint64_t size);
    void pmm_mark_chunk_used(uint64_t start_addr, size_t size_bytes);

    /** @brief Wymusza zmianę stosu i wywołuje funkcję */
    //extern "C" void force_stack_switch(uint64_t new_rsp, void (*func)());
}