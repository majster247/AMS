/**
 * @file idt.h
 * @author Majster
 * @brief Interrupt Descriptor Table - tablica wektorów przerwań.
 */

#pragma once
#include <stdint.h>

extern "C" {
    /** @brief Stub asemblerowy ignorujący przerwanie */
    void isr_ignore_stub();
    /** @brief Stub asemblerowy dla przerwania klawiatury IRQ1 */
    void isr_keyboard_stub();
}

/** @brief Struktura wpisu IDT (16 bajtów w trybie 64-bitowym) */
struct idt_entry {
    uint16_t isr_low;    /**< Dolne 16 bitów adresu handlera */
    uint16_t kernel_cs;  /**< Selektor segmentu kodu jądra */
    uint8_t  ist;        /**< Interrupt Stack Table offset */
    uint8_t  attributes; /**< Typ bramki i uprawnienia (Ring 0/3) */
    uint16_t isr_mid;    /**< Środkowe 16 bitów adresu handlera */
    uint32_t isr_high;   /**< Górne 32 bity adresu handlera */
    uint32_t reserved;
} __attribute__((packed));

/** @brief Struktura rejestru IDTR (ładowana przez LIDT) */
struct idtr {
    uint16_t limit;      /**< Rozmiar IDT - 1 */
    uint64_t base;       /**< Adres liniowy początku tablicy IDT */
} __attribute__((packed));

/** @brief Alternatywna nazwa struktury wskaźnika IDT */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/** @brief Globalny licznik tyknięć zegara systemowego (od startu) */
extern volatile uint64_t system_ticks;

extern "C" {
    /** @brief Handler przerwania 0 (Keyboard/PIT zależnie od mapowania) */
    void isr0_handler();
    /** @brief Initializer IDT - ustawia bramki i ładuje IDTR */
    void idt_init();
}