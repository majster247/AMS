#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "ext2.h"
#include "elf.h"
#include "vfs.h"
#include "kernel.h"
#include <stdint.h>

extern "C" void syscall_entry(); // ASM label
extern "C" void write_serial_char(char c);
extern "C" uint64_t sys_get_key();

static vfs_node* open_files[100];

extern "C" void syscall_init() {
    uint32_t lo, hi;
    
    // Włącz SCE (System Call Extensions) w EFER
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080));
    lo |= 1; 
    asm volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0xC0000080));

    // STAR: Kernel CS = 0x08, User CS = 0x1B (0x18 | 3)
    // d[47:32] = Kernel Syscall CS, d[63:48] = User Sysret CS
    uint32_t star_hi = (0x08 << 0) | (0x1B << 16);
    asm volatile("wrmsr" :: "a"(0), "d"(star_hi), "c"(0xC0000081));

    // LSTAR: Adres handlera
    uint64_t lstar = (uint64_t)syscall_entry;
    asm volatile("wrmsr" :: "a"(lstar & 0xFFFFFFFF), "d"(lstar >> 32), "c"(0xC0000082));

    // SFMASK: Wyłączamy przerwania (bit 9) i TF (bit 8) przy wejściu w syscall
    asm volatile("wrmsr" :: "a"(0x300), "d"(0), "c"(0xC0000084));

    write_serial_string("[SYSCALL] System Calls gotowe.\n");
}

extern "C" void syscall_handler(registers* regs) {
    uint64_t syscall_id = regs->rax;

    switch (syscall_id) {
        // --- SYS_READ (0) ---
        case 0: {
            int fd = (int)regs->rdi;
            char* buf = (char*)regs->rsi;
            uint64_t count = regs->rdx;
            
            if (fd < 3 || fd >= 100 || !open_files[fd]) {
                regs->rax = -1; // Błąd: nieprawidłowy FD
                return;
            }
            
            vfs_node* file = open_files[fd];
            // Czytamy z aktualnej pozycji
            uint64_t bytes_read = vfs_read(file, file->current_pos, count, (uint8_t*)buf);
            
            // Przesuwamy kursor w pliku!
            file->current_pos += bytes_read;
            
            regs->rax = bytes_read;
            break;
        }

        // --- SYS_WRITE (1) ---
        case 1: {
            int fd = (int)regs->rdi;
            char* buf = (char*)regs->rsi;
            uint64_t count = regs->rdx;

            if (fd == 1 || fd == 2) { 
                // stdout/stderr -> Serial Port
                write_serial_string("[DEBUG] Program chce pisac do FD: 1 Buf: ");
                for(uint64_t i = 0; i < count; i++) write_serial_char(buf[i]);
                write_serial_string("\n");
                regs->rax = count;
            } 
            else if (fd >= 3 && fd < 100 && open_files[fd]) {
                // Zapis do pliku
                vfs_node* file = open_files[fd];
                uint64_t written = vfs_write(file, file->current_pos, count, (uint8_t*)buf);
                file->current_pos += written;
                regs->rax = written;
            }
            else {
                regs->rax = -1;
            }
            break;
        }

        // --- SYS_OPEN (2) ---
        case 2: {
            char* path = (char*)regs->rdi;
            // int flags = regs->rsi; // na razie ignorujemy
            
            write_serial_string("[DEBUG] Program otwiera plik: ");
            write_serial_string(path);
            write_serial_string("\n");

            vfs_node* node = vfs_find(path);
            if (!node) {
                regs->rax = -1; // Nie znaleziono
                return;
            }

            // Znajdź wolny slot (zaczynając od 3)
            int fd = -1;
            for(int i=3; i<100; i++) {
                if (open_files[i] == nullptr) {
                    fd = i;
                    break;
                }
            }

            if (fd != -1) {
                open_files[fd] = node;
                node->current_pos = 0; // Reset kursora przy otwarciu
                regs->rax = fd;
            } else {
                regs->rax = -1; // Brak slotów
            }
            break;
        }

        // --- SYS_CLOSE (3) ---
        case 3: {
            int fd = (int)regs->rdi;
            if (fd >= 3 && fd < 100) {
                open_files[fd] = nullptr;
                regs->rax = 0;
            } else {
                regs->rax = -1;
            }
            break;
        }

        case 8: {
            int fd = (int)regs->rdi;
            long offset = (long)regs->rsi;
            int whence = (int)regs->rdx;

            if (fd < 3 || fd >= 100 || !open_files[fd]) {
                regs->rax = -1;
                return;
            }

            vfs_node* file = open_files[fd];
            
            switch(whence) {
                case 0: // SEEK_SET
                    file->current_pos = offset;
                    break;
                case 1: // SEEK_CUR
                    file->current_pos += offset;
                    break;
                case 2: // SEEK_END
                    file->current_pos = file->size + offset;
                    break;
            }
            
            // Zabezpieczenie przed wyjściem poza plik (opcjonalne)
            if (file->current_pos > file->size) file->current_pos = file->size;
            
            regs->rax = file->current_pos;
            break;
        }

        case 9: // sys_mmap
            {
                uint64_t size = regs->rsi;
                regs->rax = vmm_allocate_region(size, PAGE_USER | PAGE_WRITABLE);
            }
            break;

case 10: { // SYS_EXEC
            const char* path = (const char*)regs->rdi;
            int argc = (int)regs->rsi;
            char** argv = (char**)regs->rdx;

            write_serial_string("[EXEC] Loading: ");
            write_serial_string(path);
            write_serial_string("\n");

            // 1. Znajdź plik na dysku
            vfs_node* file = vfs_find(path);
            if (!file) {
                write_serial_string("[EXEC] File not found!\n");
                regs->rax = -1;
                return;
            }

            // 2. Skopiuj argumenty do Kernela (zanim zniszczymy pamięć Usera)
            // Limitujemy: Max 32 argumenty, max 4KB danych.
            char* k_argv_data[32]; 
            char arg_buffer[4096];
            int arg_offset = 0;

            for (int i = 0; i < argc && i < 32; i++) {
                const char* user_str = argv[i];
                k_argv_data[i] = &arg_buffer[arg_offset];
                
                // Kopiuj string (zakładamy, że jest w pamięci user, która jeszcze istnieje)
                while (*user_str && arg_offset < 4095) {
                    arg_buffer[arg_offset++] = *user_str++;
                }
                arg_buffer[arg_offset++] = 0; // Null terminator
            }

            // 3. TODO: Wyczyść przestrzeń użytkownika (Unmap User Pages)
            // Na razie to pominiemy, licząc na to, że TCC nadpisze pamięć Shella.
            // W przyszłości musisz tu zrobić: vmm_free_user_space(current_task->pml4);

            // 4. Wczytaj nagłówek ELF
            Elf64_Ehdr header;
            if (vfs_read(file, 0, sizeof(header), (uint8_t*)&header) != sizeof(header)) {
                write_serial_string("[EXEC] Read error.\n");
                regs->rax = -1;
                return;
            }

            // Sprawdź magię ELF: 0x7F, 'E', 'L', 'F'
            if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E') {
                write_serial_string("[EXEC] Not an ELF file!\n");
                regs->rax = -1;
                return;
            }

            // 5. Wczytaj segmenty programu (PHDRs)
            uint64_t ph_size = header.e_phnum * header.e_phentsize;
            // Alokujemy bufor na nagłówki programowe (można użyć kmalloc, tu na stosie ostrożnie)
            uint8_t ph_buf[2048]; 
            if (ph_size > 2048) ph_size = 2048; // Zabezpieczenie
            
            vfs_read(file, header.e_phoff, ph_size, ph_buf);
            Elf64_Phdr* phdrs = (Elf64_Phdr*)ph_buf;

            for (int i = 0; i < header.e_phnum; i++) {
                if (phdrs[i].p_type == PT_LOAD) {
                    // Oblicz ile stron potrzeba
                    uint64_t vaddr = phdrs[i].p_vaddr;
                    uint64_t memsz = phdrs[i].p_memsz;
                    uint64_t filesz = phdrs[i].p_filesz;
                    uint64_t offset = phdrs[i].p_offset;

                    // Alokuj i mapuj pamięć
                    uint64_t start_page = vaddr & ~0xFFF;
                    uint64_t end_page = (vaddr + memsz + 0xFFF) & ~0xFFF;

                    for (uint64_t page = start_page; page < end_page; page += 4096) {
                        void* phys = pmm_alloc_block();
                        // WAŻNE: Dodaj PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT
                        vmm_map_page((void*)page, phys, 0x07); 
                        
                        // Wyzeruj stronę (dla sekcji BSS)
                        memset((void*)page, 0, 4096);
                    }

                    // Wczytaj dane z pliku do pamięci
                    // Uwaga: vfs_read może nie obsłużyć zapisu pod dowolny vaddr jeśli nie jest zmapowany w Kernelu
                    // Ale w Identity Mapping (który masz) powinno zadziałać, jeśli vaddr < 4GB.
                    // Jeśli vaddr jest wysoki (np. 0x400000), to musisz uważać.
                    // Zakładam, że jesteśmy w tym samym CR3, więc po zmapowaniu widzimy to.
                    
                    vfs_read(file, offset, filesz, (uint8_t*)vaddr);
                }
            }

            // 6. Przygotuj nowy STOS
            // Alokujemy np. 16KB na stos na samym końcu User Space
            uint64_t stack_top = 0x7FFFFFFF0000; // Typowy adres user-stack
            uint64_t stack_size = 4 * 4096;
            
            for (uint64_t i = 0; i < stack_size; i+=4096) {
                void* p = pmm_alloc_block();
                vmm_map_page((void*)(stack_top - stack_size + i), p, 0x07);
            }

            // 7. Wepchnij argumenty na NOWY stos
            // Stos rośnie w dół. Najpierw wrzucamy stringi, potem wskaźniki do nich.
            
            uint64_t current_rsp = stack_top;
            uint64_t argv_ptrs[32]; // Tu zapiszemy adresy stringów na nowym stosie

            // A. Wrzucamy treści stringów (np. "tcc", "-c")
            for (int i = 0; i < argc; i++) {
                int len = strlen(k_argv_data[i]) + 1;
                current_rsp -= len;
                strcpy((char*)current_rsp, k_argv_data[i]);
                argv_ptrs[i] = current_rsp;
            }

            // B. Wyrównanie stosu do 16 bajtów (Wymagane przez ABI x64!)
            current_rsp &= ~0xF;

            // C. Wrzucamy tablicę wskaźników (argv)
            // argv[argc] = NULL (standard POSIX)
            current_rsp -= 8;
            *(uint64_t*)current_rsp = 0;

            for (int i = argc - 1; i >= 0; i--) {
                current_rsp -= 8;
                *(uint64_t*)current_rsp = argv_ptrs[i];
            }
            uint64_t final_argv = current_rsp;

            // D. Wrzucamy argc (opcjonalnie, zależy od crt0.s)
            // Ale System V ABI mówi: RDI = argc, RSI = argv.
            
            // 8. Ustaw rejestry dla nowego procesu
            regs->rip = header.e_entry;      // Punkt startu (z ELF)
            regs->rsp = current_rsp;         // Nowy stos
            regs->rdi = argc;                // 1. argument (argc)
            regs->rsi = final_argv;          // 2. argument (argv)
            
            // Resetujemy segmenty sterty w tasku
            current_task->virt_memory_top = header.e_entry + ph_size + 0x100000; // Heurystyka: Sterta za kodem

            regs->rax = 0; // Sukces (teoretycznie ta wartość nie ma znaczenia bo RIP się zmienia)
            
            write_serial_string("[EXEC] Success! Jumping to TCC...\n");
            break;
        }

        case 12: { // sys_brk
            uint64_t new_brk = regs->rdi;
            uint64_t current_brk = current_task->virt_memory_top;

            if (new_brk == 0) {
                regs->rax = current_brk;
            } 
            else if (new_brk > current_brk) {
                // Oblicz ile stron trzeba dodać
                uint64_t size = new_brk - current_brk;
                // Zaalokuj i zmapuj te strony w VMM!
                // Zakładam, że masz funkcję mapującą zakres wirtualny
                vmm_map_page(current_brk, size, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);

                current_task->virt_memory_top = new_brk;
                regs->rax = new_brk;
            }
            // Obsługa zmniejszania sterty (free) na razie opcjonalna
            else {
                regs->rax = current_brk; 
            }
            break;
        }
        case 60: // sys_exit
            write_serial_string("[SYSCALL] Process exit.\n");
            current_task->state = STATE_ZOMBIE;
            schedule(regs);
            break;

        default:
            regs->rax = -1;
            break;
    }
}