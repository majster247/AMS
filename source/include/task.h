#pragma once
#include <stdint.h>

#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_SLEEPING 2

// Struktura rejestrów (musi pasować do tego, co odkłada assembler)
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss; // Ramka przerwania
} __attribute__((packed));

struct task {
    uint64_t id;
    int state;
    uint64_t ticks_to_sleep;
    
    uint64_t kstack_top;   // Wskaźnik szczytu stosu (do przełączania kontekstu)
    uint64_t kernel_stack; // Dno stosu jądra (dla TSS/Syscall) - TO BYŁO BRAKUJĄCE
    uint64_t cr3;          // Adres katalogu stron (Pamięć) - TO BYŁO BRAKUJĄCE
    
    struct task* next;
};

extern task* current_task;

// Deklaracje funkcji
extern "C" uint64_t schedule(registers* regs);
void sleep(uint64_t ticks);
task* create_task(void (*entry_point)());
extern "C" void scheduler_add_user_task(void* entry_point, void* user_stack);