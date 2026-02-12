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
        case 1: { // WRITE
            int fd = (int)regs->rdi;
            char* buf = (char*)regs->rsi;
            uint64_t len = regs->rdx;
            if (fd == 1 || fd == 2) {
                for(uint64_t i=0; i<len; i++) write_serial_char(buf[i]);
                regs->rax = len;
            } else if (fd >= 3 && open_files[fd]) {
                vfs_node* f = open_files[fd];
                uint64_t w = vfs_write(f, f->current_pos, len, (uint8_t*)buf);
                f->current_pos += w;
                regs->rax = w;
            } else regs->rax = -1;
            break;
        }
        case 2: { // OPEN
            char* path = (char*)regs->rdi;
            write_serial_string(" [OPEN] "); write_serial_string(path); write_serial_string("\n");
            vfs_node* node = vfs_find(path);
            if (!node) { regs->rax = -1; return; }
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
        case 12: { // BRK (Poprawione wyrównanie)
            uint64_t new_brk = regs->rdi;
            uint64_t curr = current_task->virt_memory_top;
            if (new_brk == 0) {
                regs->rax = curr;
            } else {
                uint64_t start = (curr + 0xFFF) & ~0xFFF;
                uint64_t end = (new_brk + 0xFFF) & ~0xFFF;
                for (uint64_t a = start; a < end; a+=4096) {
                    vmm_map_page(a, (uint64_t)pmm_alloc_frame(), PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                }
                current_task->virt_memory_top = new_brk;
                regs->rax = new_brk;
            }
            break;
        }
        case 60: { // EXIT
            write_serial_string("[EXIT] Process finished.\n");
            current_task->state = STATE_ZOMBIE;
            schedule(regs);
            break;
        }
        default: regs->rax = -1; break;
    }
}