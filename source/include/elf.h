#pragma once
#include <stdint.h>

#define ELF_MAGIC 0x464C457F // ".ELF"

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;     // Punkt wejścia (tam skoczymy)
    uint64_t e_phoff;     // Offset tablicy Program Headers
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize; // Rozmiar jednego Program Header
    uint16_t e_phnum;     // Liczba Program Headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
    uint32_t p_type;      // Typ segmentu (1 = LOAD)
    uint32_t p_flags;
    uint64_t p_offset;    // Gdzie są dane w pliku
    uint64_t p_vaddr;     // Gdzie mają trafić w pamięci
    uint64_t p_paddr;
    uint64_t p_filesz;    // Ile bajtów w pliku
    uint64_t p_memsz;     // Ile bajtów w pamięci (może być więcej -> BSS)
    uint64_t p_align;
} __attribute__((packed));

#define PT_LOAD 1

bool elf_load(const char* path);