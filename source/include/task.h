#pragma once
#include <stdint.h>

// Definiujemy stany, żeby scheduler nie marnował czasu na uśpione zadania
enum TASK_STATE {
    STATE_RUNNING,
    STATE_READY,
    STATE_SLEEPING,
    STATE_ZOMBIE
};

struct registers {
    // Rejestry wypychane przez nas w stubie (15 sztuk)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8; 
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Rejestry wypychane automatycznie przez procesor przy przerwaniu
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct task {
    uint64_t kstack_top;      // MUSI BYĆ PIERWSZE! (dla mov rsp, [task])
    uint64_t id;
    TASK_STATE state;
    uint64_t ticks_to_sleep;
    struct task* next;
};

extern "C" {
    extern task* current_task;
    extern task* task_list;
    
    task* create_task(void (*entry_point)());
    uint64_t schedule(registers* regs);
    void sleep(uint64_t ticks); // Nowa potężna funkcja
}