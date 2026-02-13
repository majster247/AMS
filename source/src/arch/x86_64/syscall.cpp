#include "ams_syscall.h"
#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "ext2.h"
#include "vfs.h"
#include "elf.h"
#include "gdt.h"
#include <stdint.h>
#include <setjmp.h>

extern jmp_buf kernel_jmp_buf;

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

extern "C" void syscall_entry(); 
extern "C" void write_serial_char(char c);
static vfs_node* open_files[100];


// kmalloc kfree oraz k_memcpy i k_memset są potrzebne do syscall_handler, więc deklarujemy je tutaj
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);

//k_strcpy jest potrzebne do syscall_handler, więc deklarujemy je tutaj
extern "C" void k_strcpy(char* dest, const char* src);
extern "C" int k_strcmp(const char* s1, const char* s2);


static uint64_t mmap_region_start = 0x400000000; // 16GB (bezpiecznie daleko)

void safe_kernel_memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while(num--) {
        *p++ = (unsigned char)value;
    }
}

extern "C" void syscall_init() {
    uint32_t lo, hi;
    
    // EFER.SCE
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080));
    lo |= 1; 
    asm volatile("wrmsr" :: "a"(lo), "d"(hi), "c"(0xC0000080));

    // STAR (Kluczowa zmiana dla Linux GDT)
    // Kernel CS: 0x08
    // User Base: 0x23 (Index 4 | 3)
    // -> SYSRET CS = Base + 16 = Index 6 (0x33) -> User Code 64
    // -> SYSRET SS = Base + 8  = Index 5 (0x2B) -> User Data 64
    uint32_t star_hi = (0x08 << 0) | (0x23 << 16); 
    asm volatile("wrmsr" :: "a"(0), "d"(star_hi), "c"(0xC0000081));

    // LSTAR
    uint64_t lstar = (uint64_t)syscall_entry;
    asm volatile("wrmsr" :: "a"(lstar & 0xFFFFFFFF), "d"(lstar >> 32), "c"(0xC0000082));

    // SFMASK
    asm volatile("wrmsr" :: "a"(0x300), "d"(0), "c"(0xC0000084));

    write_serial_string("[SYSCALL] System Calls (STAR 0x23 Base) ready.\n");
}

extern "C" void syscall_handler(registers* regs) {
    uint64_t syscall_id = regs->rax;
    //write_serial_string("Returning to RIP: ");
    //write_serial_hex(regs->rip);
    //write_serial_string("\n");
    
    if (regs->rip == 0) {
        write_serial_string("!!! ALARM: RIP na stosie to 0 (Stack Smash?) !!!\n");
    }
    
    // Sprawdzamy User RSP (powinno być ~0x7FFF...)
    if (regs->rsp < 0x1000) {
         write_serial_string("!!! ALARM: User RSP wyglada na smieci/null !!!\n");
    }

     if (syscall_id != 12) {
        write_serial_string("[SYS] ID: ");
        write_serial_dec(syscall_id);
        write_serial_string("\n");
    }

    switch (syscall_id) {
        case 0: { // READ
                write_serial_string(">>> READ called!\n");
                int fd = (int)regs->rdi;

                write_serial_string(">>> READ: FD=");
                write_serial_dec(fd);
                write_serial_string(" BUF=0x");
                write_serial_hex(regs->rsi);
                write_serial_string(" LEN=");
                write_serial_dec(regs->rdx);
                write_serial_string("\n");

                if (fd == 0) { 
                    regs->rax = 0; 
                    return; 
                }

                if (fd < 3 || fd >= 100 || !open_files[fd]) { 
                    write_serial_string(">>> READ: Invalid FD!\n");
                    regs->rax = -1; 
                    return; 
                }

                vfs_node* f = open_files[fd];

                // ✅ KLUCZOWE: Sprawdź czy buffer jest w user space!
                uint64_t user_buf = regs->rsi;
                if (user_buf < 0x1000 || user_buf >= 0x800000000000ULL) {
                    write_serial_string(">>> READ: Invalid user buffer! buf=");
                    write_serial_hex(user_buf);
                    write_serial_string("\n");
                    regs->rax = -14; // EFAULT
                    return;
                }

                size_t bytes_read = vfs_read(f, f->current_pos, regs->rdx, (uint8_t*)regs->rsi);

                write_serial_string(">>> READ: Read ");
                write_serial_dec(bytes_read);
                write_serial_string(" bytes from offset ");
                write_serial_dec(f->current_pos);
                write_serial_string("\n");

                f->current_pos += bytes_read;
                regs->rax = bytes_read;
                break;
            }
        
        case 1: { // WRITE
                int fd = (int)regs->rdi;

                if (fd < 3 || fd >= 100 || !open_files[fd]) {
                    regs->rax = -9; // EBADF
                    return;
                }

                vfs_node* f = open_files[fd];
                uint8_t* buf = (uint8_t*)regs->rsi;
                size_t count = regs->rdx;

                write_serial_string(">>> WRITE: FD=");
                write_serial_dec(fd);
                write_serial_string(" count=");
                write_serial_dec(count);
                write_serial_string("\n");

                // ✅ Jeśli to nowy plik (ma tar_data buffer)
                if (f->tar_data && f->max_size > 0) {
                    size_t pos = f->current_pos;
                    size_t to_write = count;

                    // Sprawdź czy mieścimy się w buforze
                    if (pos + to_write > f->max_size) {
                        to_write = f->max_size - pos;
                    }

                    // Kopiuj dane
                    k_memcpy(f->tar_data + pos, buf, to_write);

                    // Aktualizuj pozycję i rozmiar
                    f->current_pos += to_write;
                    if (f->current_pos > f->size) {
                        f->size = f->current_pos;
                    }

                    write_serial_string(">>> WRITE SUCCESS: wrote ");
                    write_serial_dec(to_write);
                    write_serial_string(" bytes\n");

                    regs->rax = to_write;
                    return;
                }

                // Dla STDOUT/STDERR
                if (fd == 1 || fd == 2) {
                    for (size_t i = 0; i < count; i++) {
                        write_serial_char(buf[i]);
                    }
                    regs->rax = count;
                    return;
                }

                regs->rax = -9; // EBADF
                break;
            }
        case 2: { // OPEN
                char* path = (char*)regs->rdi;
                int flags = (int)regs->rsi;
                int mode = (int)regs->rdx; // Permissions (ignorowane na razie)

                write_serial_string("!!! OPEN: \"");
                write_serial_string(path ? path : "(null)");
                write_serial_string("\" FLAGS: ");
                write_serial_hex(flags);
                write_serial_string("\n");

                if (!path || path[0] == '\0') {
                    regs->rax = -2; // ENOENT
                    return;
                }

                vfs_node* node = vfs_find(path);

                // ✅ Jeśli plik nie istnieje i jest O_CREAT (0x40)
                if (!node && (flags & 0x40)) {
                    write_serial_string(">>> CREATING NEW FILE: ");
                    write_serial_string(path);
                    write_serial_string("\n");

                    // Stwórz nowy plik w VFS (in-memory)
                    node = (vfs_node*)kmalloc(sizeof(vfs_node));
                    if (!node) {
                        regs->rax = -12; // ENOMEM
                        return;
                    }

                    // Inicjalizuj node
                    k_memset(node, 0, sizeof(vfs_node));
                    k_strcpy(node->name, path[0] == '/' ? path + 1 : path);
                    node->size = 0;
                    node->is_directory = false;

                    // Alokuj buffor dla danych (np. 64KB)
                    node->tar_data = (uint8_t*)kmalloc(65536);
                    if (!node->tar_data) {
                        kfree(node);
                        regs->rax = -12; // ENOMEM
                        return;
                    }

                    node->max_size = 65536; // Max rozmiar buffora

                    // Dodaj do VFS (na początek listy)
                    node->next = vfs_root;
                    vfs_root = node;

                    write_serial_string(">>> FILE CREATED: ");
                    write_serial_string(node->name);
                    write_serial_string("\n");
                }

                if (!node) {
                    write_serial_string(">>> OPEN FAILED: ");
                    write_serial_string(path);
                    write_serial_string("\n");
                    regs->rax = -2; // ENOENT
                    return;
                }

                // Jeśli O_TRUNC (0x200), wyzeruj plik
                if (flags & 0x200) {
                    node->size = 0;
                    k_memset(node->tar_data, 0, node->max_size);
                    node->current_pos = 0;
                }

                // Przydziel FD
                for(int i=3; i<100; i++) {
                    if (!open_files[i]) {
                        open_files[i] = node; 
                        node->current_pos = 0; 
                        regs->rax = i; 

                        write_serial_string(">>> OPEN SUCCESS: FD=");
                        write_serial_dec(i);
                        write_serial_string("\n");
                        return;
                    }
                }

                regs->rax = -24; // EMFILE
                break;
            }
        
        case 3: { // CLOSE
            int fd = (int)regs->rdi;
            if (fd >= 3 && fd < 100) open_files[fd] = nullptr;
            regs->rax = 0;
            break;
        }
        
        case 5: { // FSTAT
                int fd = (int)regs->rdi;
                void* statbuf = (void*)regs->rsi;

                write_serial_string(">>> FSTAT: FD=");
                write_serial_dec(fd);
                write_serial_string(" buf=0x");
                write_serial_hex((uint64_t)statbuf);
                write_serial_string("\n");

                if (fd < 3 || fd >= 100 || !open_files[fd]) {
                    write_serial_string(">>> FSTAT: Invalid FD!\n");
                    regs->rax = -9; // EBADF
                    return;
                }

                vfs_node* f = open_files[fd];

                // Struktura stat (uproszczona, Linux x86_64)
                struct stat {
                    uint64_t st_dev;
                    uint64_t st_ino;
                    uint64_t st_mode;
                    uint64_t st_nlink;
                    uint32_t st_uid;
                    uint32_t st_gid;
                    uint64_t st_rdev;
                    uint64_t st_size;      // ← TCC potrzebuje tego!
                    uint64_t st_blksize;
                    uint64_t st_blocks;
                    uint64_t st_atime;
                    uint64_t st_mtime;
                    uint64_t st_ctime;
                };

                struct stat* st = (struct stat*)statbuf;

                // Wyzeruj strukturę
                for (int i = 0; i < sizeof(struct stat); i++) {
                    ((char*)st)[i] = 0;
                }

                // Wypełnij podstawowe dane
                st->st_size = f->size;
                st->st_mode = 0100644; // Regular file, rw-r--r--
                st->st_nlink = 1;
                st->st_blksize = 4096;
                st->st_blocks = (f->size + 4095) / 4096;

                write_serial_string(">>> FSTAT: Success! size=");
                write_serial_dec(f->size);
                write_serial_string("\n");

                regs->rax = 0; // Success
                break;
            }
        
        case 8: { // LSEEK
                int fd = (int)regs->rdi;
                long offset = (long)regs->rsi;
                int whence = (int)regs->rdx;

                write_serial_string(">>> LSEEK: FD=");
                write_serial_dec(fd);
                write_serial_string(" offset=");
                write_serial_dec(offset);
                write_serial_string(" whence=");
                write_serial_dec(whence);
                write_serial_string("\n");

                if(fd < 3 || fd >= 100 || !open_files[fd]) {
                    write_serial_string(">>> LSEEK: Invalid FD!\n");
                    regs->rax = -9; // EBADF
                    return;
                }

                vfs_node* f = open_files[fd];

                if(whence == 0) { // SEEK_SET
                    f->current_pos = offset;
                } else if(whence == 1) { // SEEK_CUR
                    f->current_pos += offset;
                } else if(whence == 2) { // SEEK_END
                    f->current_pos = f->size + offset;
                }

                write_serial_string(">>> LSEEK: New position: ");
                write_serial_dec(f->current_pos);
                write_serial_string("\n");

                regs->rax = f->current_pos;
                break;
            }
        // -----------------------------------------------------------
        // Syscall 9: MMAP (TCC tego pragnie dla kodu)
        // -----------------------------------------------------------
        case 9: { // MMAP
            // ✅ Zmień z 16GB na 1GB (bezpieczniej, wciąż daleko od BRK)
            static uint64_t mmap_base = 0x40000000; // 1GB zamiast 16GB

            uint64_t len = regs->rsi;
            if (len == 0) { regs->rax = -22; break; }

            if (len % 4096) len = (len & ~0xFFF) + 4096;

            uint64_t ret = mmap_base;

            for (uint64_t i = 0; i < len; i+=4096) {
                void* phys = pmm_alloc_frame();
                if (!phys) {
                    regs->rax = (uint64_t)-1;
                    return;
                }

                vmm_map_page(mmap_base + i, (uint64_t)phys, 0x7); // User RWX
                memset((void*)((uint64_t)phys + 0xFFFF800000000000ULL), 0, 4096);
            }

            mmap_base += len;
            regs->rax = ret;
            break;
        }

        // -----------------------------------------------------------
        // Syscall 10: MPROTECT (TCC tego potrzebuje, żeby uwierzyć, że kod zadziała)
        // -----------------------------------------------------------
        case 10: { 
            // addr = rdi, len = rsi, prot = rdx
            // W naszym prostym OS ignorujemy uprawnienia (wszystko jest RWX)
            // Ale musimy zwrócić 0 (SUKCES), żeby TCC był szczęśliwy.
            
            // Debug info (żebyś widział, czy TCC o to pyta)
            write_serial_string("[MPROTECT] Fake success for: "); 
            write_serial_hex(regs->rdi); 
            write_serial_string("\n");

            regs->rax = 0; 
            break;
        }

        // -----------------------------------------------------------
        // Syscall 11: MUNMAP (Dla porządku, żeby nie było błędów -1)
        // -----------------------------------------------------------
        case 11: {
            regs->rax = 0; // Udajemy, że zwolniliśmy
            break;
        }
        
        case 12: { // SYS_BRK
                uint64_t new_brk = regs->rdi;
                uint64_t curr = current_task->virt_memory_top;

                if (curr == 0) {
                    current_task->virt_memory_top = 0x80000000;
                    curr = 0x80000000;
                }

                if (new_brk == 0 || new_brk < curr) {
                    regs->rax = curr;
                } else {
                    if (new_brk >= 0x100000000) { 
                        write_serial_string("[BRK] OOM!\n");
                        regs->rax = curr;
                        return;
                    }

                    uint64_t start = (curr + 0xFFF) & ~0xFFF;
                    uint64_t end   = (new_brk + 0xFFF) & ~0xFFF;
                
                    for (uint64_t virt = start; virt < end; virt += 4096) {
                        void* phys = pmm_alloc_frame();
                        if (!phys) {
                            write_serial_string("[BRK] PMM returned NULL!\n");
                            regs->rax = curr;
                            return;
                        }

                        // ✅ Sprawdź, czy adres fizyczny jest czysty (bez flag)
                        if ((uint64_t)phys & 0xFFF) {
                            write_serial_string("[BRK] ERROR: PMM returned address with flags! phys=");
                            write_serial_hex((uint64_t)phys);
                            write_serial_string("\n");
                            // Oczyść flagi
                            phys = (void*)((uint64_t)phys & ~0xFFF);
                        }

                        // ✅ Zeruj przez PHYSICAL_MEM_OFFSET (kernel mapping)
                        uint64_t phys_addr = (uint64_t)phys;
                        memset((void*)(phys_addr + 0xFFFF800000000000ULL), 0, 4096);

                        // ✅ Mapuj do user space
                        vmm_map_page(virt, phys_addr, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                    }

                    current_task->virt_memory_top = new_brk;
                    regs->rax = new_brk;
                }
                break;
            }
        
        case 60: { // EXIT
                int code = (int)regs->rdi;

                write_serial_string("[EXIT] Process finished with code: ");
                write_serial_dec(code);
                write_serial_string("\n");

                // Sprawdź kupa.o
                if (current_task) {
                    write_serial_string("[KERNEL] TCC finished! Checking kupa.o...\n");

                    vfs_node* kupa_o = vfs_find("kupa.o");
                    if (kupa_o) {
                        write_serial_string("[KUPA.O] Size: ");
                        write_serial_dec(kupa_o->size);
                        write_serial_string(" bytes\n");

                        write_serial_string("[KUPA.O] ELF Header: ");
                        for (int i = 0; i < 16 && i < kupa_o->size; i++) {
                            write_serial_hex(kupa_o->tar_data[i]);
                            write_serial_char(' ');
                        }
                        write_serial_string("\n");

                        if (kupa_o->size >= 4 && 
                            kupa_o->tar_data[0] == 0x7F &&
                            kupa_o->tar_data[1] == 'E' &&
                            kupa_o->tar_data[2] == 'L' &&
                            kupa_o->tar_data[3] == 'F') {
                            write_serial_string("[KUPA.O] ✅ Valid ELF object file!\n");
                        }
                    }
                }

                extern task* kernel_task;
                if (kernel_task && kernel_task->rip != 0) {
                    write_serial_string("[EXIT] Returning to kernel...\n");
                    write_serial_string("[EXIT] RIP = ");
                    write_serial_hex(kernel_task->rip);
                    write_serial_string("\n");
                    write_serial_string("[EXIT] RSP = ");
                    write_serial_hex(kernel_task->kstack_top);
                    write_serial_string("\n");
                    write_serial_string("[EXIT] CR3 = ");
                    write_serial_hex(kernel_task->cr3);
                    write_serial_string("\n");

                    // Przywróć CR3
                    asm volatile("mov %0, %%cr3" : : "r"(kernel_task->cr3));

                    // ✅ POPRAWKA: Zachowaj RIP na stosie PRZED czyszczeniem rejestrów
                    uint64_t target_rip = kernel_task->rip;
                    uint64_t target_rsp = kernel_task->kstack_top;

                    asm volatile(
                        "cli\n"
                        "mov %0, %%rsp\n"      // Załaduj RSP
                        "push %1\n"            // Push RIP na stos (PRZED czyszczeniem!)
                        "xor %%rax, %%rax\n"   // Teraz możemy czyścić
                        "xor %%rbx, %%rbx\n"
                        "xor %%rcx, %%rcx\n"
                        "xor %%rdx, %%rdx\n"
                        "xor %%rsi, %%rsi\n"
                        "xor %%rdi, %%rdi\n"
                        "xor %%rbp, %%rbp\n"
                        "xor %%r8, %%r8\n"
                        "xor %%r9, %%r9\n"
                        "xor %%r10, %%r10\n"
                        "xor %%r11, %%r11\n"
                        "xor %%r12, %%r12\n"
                        "xor %%r13, %%r13\n"
                        "xor %%r14, %%r14\n"
                        "xor %%r15, %%r15\n"
                        "ret\n"                // Return do RIP ze stosu
                        :
                        : "r"(target_rsp), "r"(target_rip)
                        : "memory"
                    );

                    // NIGDY nie dotrzemy tutaj
                }

                while(1) asm("hlt");
            }
        
        default: regs->rax = -1; break;
    }
}