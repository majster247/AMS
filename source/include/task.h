#pragma once
#include <stdint.h>
#include "vfs.h"

#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_SLEEPING 2
#define STATE_ZOMBIE 3
#define MAX_OPEN_FILES 16

// Układ ramki stosu identyczny z tym, co buduje interrupts.s
struct registers {
    // Rejestry wypychane przez SAVE_ALL
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Dane przerwania
    uint64_t int_no, err_code;

    // Ramka sprzętowa IRETQ (wypychana przez CPU lub ręcznie dla syscalli)
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct task {
    char name[32];
    uint64_t rsp;
    uint64_t rip;
    uint64_t cr3;
    uint64_t kstack_top;
    uint64_t id;
    int state;
    uint64_t ticks_to_sleep;
    
    uint64_t kernel_stack;
    
    vfs_node* file_descriptors[MAX_OPEN_FILES];
    uint64_t virt_memory_top;
    struct task* next;
};

extern "C" task* kernel_task;

extern "C" {
    extern struct task* current_task;
    void scheduler_init_kernel_task();
    uint64_t schedule(struct registers* regs);

    void create_kernel_task(void (*entry)());
}

extern "C" void scheduler_switch_to_user(uint64_t rip, uint64_t rsp);
