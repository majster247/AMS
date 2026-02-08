#include "kernel.h"
#include <stdint.h>

extern "C" void syscall_entry(); // ASM label

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

extern "C" void syscall_handler(uint64_t sys_num, uint64_t arg1) {
    if (sys_num == 1) { // 1 = write_serial
        // Zabezpieczenie: sprawdź czy wskaźnik jest w przestrzeni użytkownika
        if (arg1 < 0xFFFF800000000000) { 
            write_serial_string((const char*)arg1);
        }
    }
}