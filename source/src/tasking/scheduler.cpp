#include "task.h"
#include "gdt.h"
#include "vmm.h"
#include "kernel.h"
#include "heap.h"

extern "C" void* kmalloc(size_t size);
extern "C" void k_memset(void* dest, int val, size_t len);
extern "C" uint64_t get_cr3();
extern "C" void jump_to_ring3(uint64_t rip, uint64_t rsp);

// 🔥 KLUCZOWE: wymuszenie C-linkage (FIX LINKERA)
extern "C" void scheduler_switch_to_user(uint64_t rip, uint64_t rsp);

extern "C" void kernel_task_wrapper(void (*entry)()) {
    asm volatile("sti");

    if (entry)
        entry();

    write_serial_string("\n[SCHED] Kernel task ended\n");

    while (1) asm volatile("cli; hlt");
}

extern "C" {
    task* current_task = nullptr;
    task* task_list[64];
    int task_count = 0;
    int current_task_index = 0;
    static uint64_t g_next_task_id = 1;

    uint64_t schedule(registers* regs) {
        if (task_count <= 1)
            return (uint64_t)regs;

        // zapisz kontekst
        if (current_task)
            current_task->rsp = (uint64_t)regs;

        // round robin
        current_task_index = (current_task_index + 1) % task_count;
        task* next = task_list[current_task_index];

        current_task = next;

        // kernel stack dla ring0
        system_tss.rsp0 = next->kstack_top;
        cpu_data.kernel_stack = next->kstack_top;

        // CR3 switch (process isolation)
        uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));

        if (next->cr3 != cr3) {
            asm volatile("mov %0, %%cr3" :: "r"(next->cr3) : "memory");
        }

        return current_task->rsp;
    }

    void scheduler_init_kernel_task() {
        task* t = (task*)kmalloc(sizeof(task));
        k_memset(t, 0, sizeof(task));

        uint64_t rsp;
        uint64_t cr3;

        asm volatile("mov %%rsp, %0" : "=r"(rsp));
        asm volatile("mov %%cr3, %0" : "=r"(cr3));

        t->rsp = rsp;
        t->kstack_top = rsp;
        t->cr3 = cr3;
        t->id = g_next_task_id++;

        task_list[0] = t;
        task_count = 1;
        current_task_index = 0;
        current_task = t;
        cpu_data.kernel_stack = t->kstack_top;

        kernel_task = t;

        write_serial_string("[SCHED] Kernel task initialized\n");
    }

    void create_task(const char* name, uint64_t entry, uint64_t user_stack) {
        task* t = (task*)kmalloc(sizeof(task));
        k_memset(t, 0, sizeof(task));

        uint64_t kstack = (uint64_t)kmalloc(0x4000) + 0x4000;
        t->kstack_top = kstack;

        uint64_t* stack = (uint64_t*)kstack;

        *(--stack) = 0x2B;          // SS
        *(--stack) = user_stack;    // RSP
        *(--stack) = 0x202;         // RFLAGS
        *(--stack) = 0x33;          // CS
        *(--stack) = entry;        // RIP

        *(--stack) = 0; // dummy regs...

        for (int i = 0; i < 15; i++)
            *(--stack) = 0;

        t->rsp = (uint64_t)stack;

        uint64_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        t->cr3 = cr3;
        t->id = g_next_task_id++;

        task_list[task_count++] = t;
    }

} // extern "C"


// ================================
// USER MODE SWITCH WRAPPER FIX
// ================================

// 🔥 TO BYŁO BRAKUJĄCE W LINKERZE
extern "C" void scheduler_switch_to_user(uint64_t rip, uint64_t rsp) {
    jump_to_ring3(rip, rsp);
    __builtin_unreachable();
}