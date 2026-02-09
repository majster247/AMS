#include "vfs.h"
#include "vmm.h"
#include "heap.h"
#include "kernel.h"
#include "heap.h"
#include "ext2.h"
#include "elf.h"

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

bool elf_load(const char* path) {
    write_serial_string("[ELF] Loading: ");
    write_serial_string(path);
    write_serial_string("\n");

    // 1. Otwórz plik
    // Uwaga: używamy ext2_read_file, która alokuje bufor w jądrze. 
    // Docelowo lepiej czytać kawałkami, ale na start to wystarczy.
    char* file_buffer = ext2_read_file(path);
    if (!file_buffer) {
        write_serial_string("[ELF] Error: File not found.\n");
        return false;
    }

    Elf64_Ehdr* hdr = (Elf64_Ehdr*)file_buffer;

    // 2. Sprawdź magię
    if (*(uint32_t*)hdr->e_ident != ELF_MAGIC) {
        write_serial_string("[ELF] Error: Invalid MAGIC.\n");
        kfree(file_buffer);
        return false;
    }

    // 3. Iteruj po Program Headers (Segmentach)
    Elf64_Phdr* phdr = (Elf64_Phdr*)(file_buffer + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // Dla każdego segmentu LOAD:
            // Musimy zmapować pamięć pod adres p_vaddr.
            
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;

            // Wyrównanie do strony
            uint64_t start_page = vaddr & ~0xFFF;
            uint64_t end_page = (vaddr + memsz + 0xFFF) & ~0xFFF;
            uint64_t page_count = (end_page - start_page) / 4096;

            // Alokujemy fizyczne strony i mapujemy je "na sztywno" tam, gdzie chce ELF
            for (uint64_t j = 0; j < page_count; j++) {
                void* phys = pmm_alloc_frame();
                write_serial_string("[ELF] Mapuje segment PT_LOAD...\n");
                write_serial_string("  Virt: ");
                write_serial_hex(start_page + j*4096);
                write_serial_string(" Phys: ");
                write_serial_hex((uint64_t)phys);
                write_serial_string("\n");
                vmm_map_page(start_page + j*4096, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                // Zerujemy na wszelki wypadek (BSS tego wymaga)
                memset((void*)(start_page + j*4096), 0, 4096);
            }

            // Kopiujemy dane z pliku do pamięci
            // Uwaga: To zadziała tylko, jeśli mamy Identity Mapping dla tego zakresu
            // lub jesteśmy w kontekście CR3, gdzie te strony są widoczne.
            // W Twoim kernelu taski dzielą CR3 (na razie), więc to zadziała.
            write_serial_string("[ELF] Kopiuje segment do: ");
            write_serial_hex(vaddr);
            write_serial_string("\n");
            memcpy((void*)vaddr, file_buffer + offset, filesz);
            
            // Jeśli memsz > filesz, reszta jest już wyzerowana (BSS)
        }
    }

    // 4. Stwórz Stos Użytkownika
    // Alokujemy np. 16KB na stos pod adresem np. 0x80000000
    uint64_t user_stack_top = 0x80000000;
    for(int i=0; i<4; i++) { // 4 strony stosu
        void* phys = pmm_alloc_frame();
        vmm_map_page(user_stack_top - (i+1)*4096, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
    }

    write_serial_string("[ELF] Entry Point: ");
    write_serial_hex(hdr->e_entry); // Masz funkcję write_serial_hex w kernel.h?
    write_serial_string("\n");

    if (hdr->e_entry < 0x4000000) {
        write_serial_string("[ELF] WARNING: Suspiciously low Entry Point!\n");
    }

    // 5. Dodaj zadanie do schedulera
    scheduler_add_user_task((void*)hdr->e_entry, (void*)user_stack_top);

    write_serial_string("[ELF] Process started!\n");
    kfree(file_buffer);
    return true;
}