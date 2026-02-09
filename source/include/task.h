#pragma once
#include <stdint.h>
#include "vfs.h"

#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_SLEEPING 2
#define STATE_ZOMBIE 3

#define MAX_OPEN_FILES 16

// Struktura rejestrów (musi pasować do tego, co odkłada assembler)
struct registers {
    // 1. Te są na samym dole stosu (pushowane ostatnie)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // 2. To jest błąd w Twojej strukturze! 
    // int_no i err_code muszą być TUTAJ, bo makra pushują je przed rejestrami.
    uint64_t int_no, err_code;

    // 3. To pushuje procesor (najwyżej na stosie)
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct task {
    uint64_t kstack_top; // Wskaźnik szczytu stosu (do przełączania kontekstu)
    uint64_t id;
    int state;
    uint64_t ticks_to_sleep;
    
    uint64_t kernel_stack; // Dno stosu jądra (dla TSS/Syscall) - TO BYŁO BRAKUJĄCE
    uint64_t cr3;          // Adres katalogu stron (Pamięć) - TO BYŁO BRAKUJĄCE
    
    vfs_node* file_descriptors[MAX_OPEN_FILES];

    uint64_t virt_memory_top; // Najwyższy adres wirtualny zajęty przez proces (do alokacji kolejnych stron)


    struct task* next;
};

extern task* current_task;

// Deklaracje funkcji
extern "C" uint64_t schedule(registers* regs);
void sleep(uint64_t ticks);
task* create_task(void (*entry_point)());
extern "C" void scheduler_add_user_task(void* entry_point, void* user_stack);
void scheduler_init_kernel_task();