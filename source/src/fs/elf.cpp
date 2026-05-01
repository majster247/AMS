#include "vfs.h"
#include "vmm.h"
#include "heap.h"
#include "kernel.h"
#include "ext2.h"
#include "elf.h"


//lokalna definicja strcpy dla tego pliku (aby nie mieszać z globalnym)
static void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

extern "C" void jump_to_ring3(uint64_t entry, uint64_t rsp);

void load_elf(const char* path) {
    vfs_node* file = vfs_find(path);
    if (!file) {
        write_serial_string("[ELF] Nie znaleziono pliku!\n");
        return;
    }

    //Tworzymy przestrzeń adresową dla procesu (PML4)
    uint64_t new_pml4 = vmm_create_user_pml4();

    // Odczyt nagłówka ELF
    Elf64_Ehdr ehdr;
    file->read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdr);

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E') {
        write_serial_string("[ELF] Bledna sygnatura ELF\n");
        return;
    }

   for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr phdr;
        file->read(file, ehdr.e_phoff + (i * ehdr.e_phentsize), sizeof(Elf64_Phdr), (uint8_t*)&phdr);

        if (phdr.p_type == 1) { // PT_LOAD
            uint64_t vaddr = phdr.p_vaddr;
            uint64_t filesz = phdr.p_filesz;
            uint64_t memsz = phdr.p_memsz;
            uint64_t offset = phdr.p_offset;

            uint64_t page_count = (memsz + 4095) / 4096;
            
            for (uint64_t j = 0; j < page_count; j++) {
                void* phys_page = pmm_alloc_frame();
                vmm_map_page_ex(new_pml4, vaddr + j * 4096, (uint64_t)phys_page, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                
                // POPRAWKA: Czyścimy ramkę przez HHDM, nie przez vaddr!
                uint64_t hhdm_vaddr = (uint64_t)phys_page + 0xFFFF800000000000ULL;
                memset((void*)hhdm_vaddr, 0, 4096);

                // POPRAWKA: Kopiujemy dane z pliku kawałek po kawałku do każdej ramki
                // Obliczamy ile danych z pliku wpada do tej konkretnej strony
                uint64_t page_offset = j * 4096;
                if (page_offset < filesz) {
                    uint64_t to_read = (filesz - page_offset > 4096) ? 4096 : (filesz - page_offset);
                    file->read(file, offset + page_offset, to_read, (uint8_t*)hhdm_vaddr);
                }
            }
            write_serial_string("[ELF] Zaladowano segment pod: ");
            write_serial_hex(vaddr);
            write_serial_string("\n");
        }
    }

    // 2. Przygotowanie Stosu (Zmień mapowanie na HHDM)
    uint64_t stack_top_virtual = 0x80000000;
    uint64_t stack_phys[4];
    for (int i = 0; i < 4; i++) {
        stack_phys[i] = (uint64_t)pmm_alloc_frame();
        vmm_map_page_ex(new_pml4, (stack_top_virtual - 16384) + i * 4096, stack_phys[i], PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
        memset((void*)(stack_phys[i] + 0xFFFF800000000000ULL), 0, 4096);
    }

    // ABI STACK SETUP (piszemy do ostatniej ramki stosu przez HHDM)
    uint64_t last_stack_frame_hhdm = stack_phys[3] + 0xFFFF800000000000ULL;
    uint64_t* rsp_hhdm = (uint64_t*)(last_stack_frame_hhdm + 4096);

    // Kopiowanie argumentów (uproszczone dla testu)
    rsp_hhdm = (uint64_t*)((uint64_t)rsp_hhdm - 16);
    strcpy((char*)rsp_hhdm, "/tools/compiler/tcc");
    uint64_t arg_vaddr = stack_top_virtual - 16; 

    rsp_hhdm--; *rsp_hhdm = 0;               // envp
    rsp_hhdm--; *rsp_hhdm = 0;               // argv[1]
    rsp_hhdm--; *rsp_hhdm = arg_vaddr;       // argv[0] (adres wirtualny!)
    rsp_hhdm--; *rsp_hhdm = 1;               // argc

    uint64_t final_user_rsp = stack_top_virtual - (uint64_t)( (last_stack_frame_hhdm + 4096) - (uint64_t)rsp_hhdm );

    // 3. Skok do Ring 3
    write_serial_string("[ELF] Skok do Entry Point: ");
    write_serial_hex(ehdr.e_entry);
    write_serial_string(" ze stosem: ");
    write_serial_hex(final_user_rsp);
    write_serial_string("\n");

    //przełączamy się do Ring 3, skacząc pod punkt wejścia z ustawionym RSP na przygotowany stos użytkownika
    //przełączamy przestrzeń adresową na tę z mapowaniem użytkownika (new_pml4) i skaczemy pod punkt wejścia z RSP ustawionym na przygotowany stos
    set_cr3(new_pml4); // Ustawiamy nowy PML4, żeby procesor widział mapowanie użytkownika
    write_serial_string("[ELF] CR3 ustawiony na nowy PML4. Przełączam do Ring 3...\n");
    // Dla debugowania, możemy wypisać pierwsze bajty kodu pod punktem wejścia, żeby upewnić się, że jest tam poprawny kod
     uint64_t entry = ehdr.e_entry;
     uint8_t* code = (uint8_t*)entry;
    
    write_serial_string("[DEBUG] First 5 bytes at entry: ");
    for(int i=0; i<5; i++) {
        write_serial_hex(code[i]);
        write_serial_string(" ");
    }
    write_serial_string("\n");

    jump_to_ring3(ehdr.e_entry, final_user_rsp);
}