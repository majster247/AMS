#pragma once
#include <stdint.h>
#include "vfs.h"

#define STATE_READY 0
#define STATE_RUNNING 1
#define STATE_SLEEPING 2
#define STATE_ZOMBIE 3
#define MAX_OPEN_FILES 16

// Układ musi pasować do syscall_entry.s (push od dołu do góry)
struct registers {
    // PUSHOWANE RĘCZNIE
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    
    // PUSHOWANE JAKO DUMMY (int_no, err_code)
    uint64_t int_no, err_code;
    
    // RAMKA PROCESORA (Dla IRETQ / SYSRETQ)
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

struct task {
    uint64_t kstack_top;
    uint64_t id;
    int state;
    uint64_t ticks_to_sleep;
    
    uint64_t kernel_stack;
    uint64_t cr3;
    
    vfs_node* file_descriptors[MAX_OPEN_FILES];
    uint64_t virt_memory_top;
    struct task* next;
};

extern task* current_task;
extern "C" uint64_t schedule(registers* regs);
void sleep(uint64_t ticks);
task* create_task(void (*entry_point)());
extern "C" void scheduler_add_user_task(void* entry_point, void* user_stack);
void scheduler_init_kernel_task();