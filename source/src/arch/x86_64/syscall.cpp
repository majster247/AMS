#include "ams_syscall.h"
#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "ext2.h"
#include "vfs.h"
#include "elf.h"
#include "gdt.h"
#include <stdint.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

extern "C" void syscall_entry(); 
extern "C" void write_serial_char(char c);
static vfs_node* open_files[100];

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

    // Wypisz ID
    write_serial_string("[SYS] ID: "); write_serial_dec(syscall_id); write_serial_string("\n");

    switch (syscall_id) {
        case 0: { // READ
            int fd = (int)regs->rdi;
            if (fd == 0) { regs->rax = 0; return; }
            if (fd < 3 || fd >= 100 || !open_files[fd]) { regs->rax = -1; return; }
            vfs_node* f = open_files[fd];
            regs->rax = vfs_read(f, f->current_pos, regs->rdx, (uint8_t*)regs->rsi);
            f->current_pos += regs->rax;
            break;
        }
        
        case 1: { // SYS_WRITE
            // Jeśli TCC chce coś napisać (błąd), to wypisujemy to bez prefixów!
            char* buf = (char*)regs->rsi;
            for (size_t i = 0; i < regs->rdx; i++) {
                write_serial_char(buf[i]);
            }
            regs->rax = regs->rdx;
            break;
        }
        case 2: { // OPEN
            char* path = (char*)regs->rdi;
            write_serial_string("!!! OPEN SYSCALL: "); write_serial_string(path); write_serial_string("\n");
            write_serial_string(" [OPEN] "); write_serial_string(path); write_serial_string("\n");
            vfs_node* node = vfs_find(path);
            if (!node) { regs->rax = -1; write_serial_string("[OPEN] File not found.\n"); return; }
            for(int i=3; i<100; i++) {
                if (!open_files[i]) {
                    open_files[i] = node; node->current_pos = 0; regs->rax = i; return;
                }
            }
            regs->rax = -1;
            break;
        }
        
        case 3: { // CLOSE
            int fd = (int)regs->rdi;
            if (fd >= 3 && fd < 100) open_files[fd] = nullptr;
            regs->rax = 0;
            break;
        }
        
        case 5: regs->rax = 0; break; // FSTAT
        
        case 8: { // LSEEK
             int fd = (int)regs->rdi;
             if(fd>=3 && open_files[fd]) {
                 vfs_node* f = open_files[fd];
                 if(regs->rdx==0) f->current_pos = regs->rsi;
                 else if(regs->rdx==1) f->current_pos += regs->rsi;
                 else if(regs->rdx==2) f->current_pos = f->size + regs->rsi;
                 regs->rax = f->current_pos;
             } else regs->rax = -1;
             break;
        }
        // -----------------------------------------------------------
        // Syscall 9: MMAP (TCC tego pragnie dla kodu)
        // -----------------------------------------------------------
        case 9: { // MMAP
                // Ignoruj flagi, daj mu po prostu anonimową pamięć RWX
                uint64_t len = regs->rsi;
                if (len == 0) { regs->rax = -22; break; } // EINVAL
                    
                // Wyrównaj do strony
                if (len % 4096) len = (len & ~0xFFF) + 4096;
                    
                // Znajdź miejsce (użyj globalnego licznika dla mmap)
                // UWAGA: Nie używaj heap_start (0x80000000), bo tam jest BRK!
                // Użyj np. 0x400000000 (16GB) jako bazy dla MMAP
                static uint64_t mmap_base = 0x400000000; 
                    
                uint64_t ret = mmap_base;
                for (uint64_t i = 0; i < len; i+=4096) {
                    void* p = pmm_alloc_frame();
                    vmm_map_page(mmap_base + i, (uint64_t)p, 0x7); // User RWX
                    memset((void*)(mmap_base+i), 0, 4096);
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
            //asm volatile("cli");
            uint64_t new_brk = regs->rdi;
            uint64_t curr = current_task->virt_memory_top;

            write_serial_string("[BRK] Request: "); 
            write_serial_hex(new_brk);
            write_serial_string(" Current: "); 
            write_serial_hex(curr);
            write_serial_string("\n");

            // Jeśli to pierwsze wywołanie (new_brk == 0 lub mniejsze niż 1MB)
            // to inicjalizujemy stertę tam, gdzie proces chce (np. 0x670 + margines)
            if (curr == 0) {
                // Startujemy stertę tam, gdzie program TCC się kończy w pamięci
                // Dla bezpieczeństwa dajmy mu 0x10000000 (256MB), żeby nie kolidował z kodem
                current_task->virt_memory_top = 0x10000000;
                curr = 0x10000000;
            }
            
            //logujemy OOM jeśli nie możemy przydzielić więcej pamięci
            if (new_brk >= 0x100000000) { 
                write_serial_string("[BRK] OOM: Za duzo!\n");
                regs->rax = curr;
                return;
            }
        
            if (new_brk == 0 || new_brk < curr) {
                regs->rax = curr;
            } else {
                // Alokacja (tak jak miałeś z zerowaniem!)
                uint64_t start = (curr + 0xFFF) & ~0xFFF;
                uint64_t end   = (new_brk + 0xFFF) & ~0xFFF;

                for (uint64_t virt = start; virt < end; virt += 4096) {
                    void* phys = pmm_alloc_frame();
                    safe_kernel_memset(phys, 0, 4096);
                    vmm_map_page(virt, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                }
                current_task->virt_memory_top = new_brk;
                regs->rax = new_brk;
            }
            //asm volatile("sti");
            break;
        }
        
        case 60: { // EXIT
            write_serial_string("[EXIT] Process finished with code: ");
            write_serial_dec(regs->rdi); // W Linuxie kod wyjścia jest w RDI
            write_serial_string("\n");
            current_task->state = STATE_ZOMBIE;
            schedule(regs);
            break;
        }
        
        default: regs->rax = -1; break;
    }
}