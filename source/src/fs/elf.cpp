#include "vfs.h"
#include "vmm.h"
#include "heap.h"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

extern "C" void jump_to_ring3(uint64_t entry, uint64_t rsp);

void load_elf_and_run(const char* path) {
    vfs_node* file = vfs_find(path);
    if (!file) {
        write_serial_string("[ELF] Nie znaleziono pliku!\n");
        return;
    }

    Elf64_Ehdr ehdr;
    file->read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdr);

    // Sprawdzenie sygnatury \x7F ELF
    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E') return;

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        file->read(file, ehdr.e_phoff + (i * ehdr.e_phentsize), sizeof(Elf64_Phdr), (uint8_t*)&phdr);

        if (phdr.p_type == 1) { // PT_LOAD
            uint64_t pages = (phdr.p_memsz + 4095) / 4096;
            for (uint64_t j = 0; j < pages; j++) {
                uint64_t phys = (uint64_t)pmm_alloc_frame();
                vmm_map_user(phdr.p_vaddr + (j * 4096), phys, true); // Mapowanie USER
            }
            // Wczytanie danych sekcji
            file->read(file, phdr.p_offset, phdr.p_filesz, (uint8_t*)phdr.p_vaddr);
            // BSS (zerowanie reszty)
            if (phdr.p_memsz > phdr.p_filesz) {
                memset((void*)(phdr.p_vaddr + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
            }
        }
    }

    // Alokacja stosu użytkownika (wysoki adres)
    uint64_t stack_virt = 0x7FFFFFFFF000;
    uint64_t stack_phys = (uint64_t)pmm_alloc_frame();
    vmm_map_user(stack_virt, stack_phys, true);

    write_serial_string("[ELF] Skok do Ring 3...\n");
    jump_to_ring3(ehdr.e_entry, stack_virt + 4096);
}