#pragma once
#include <stdint.h>

struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct task {
    uint64_t id;
    uint64_t kstack_top;
    struct task* next;
};

// Deklaracje zmiennych i funkcji dostępnych globalnie
extern "C" {
    extern task* current_task;
    extern task* task_list;
    task* create_task(void (*entry_point)());
}