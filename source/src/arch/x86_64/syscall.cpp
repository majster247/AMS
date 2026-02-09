#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "ext2.h"
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

        case 12: // sys_brk
            {
                uint64_t new_brk = regs->rdi;
                if (new_brk == 0) regs->rax = current_task->virt_memory_top;
                else {
                    current_task->virt_memory_top = new_brk;
                    regs->rax = new_brk;
                }
            }
            break;

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