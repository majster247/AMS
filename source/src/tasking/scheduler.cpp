#include "task.h"
#include "vmm.h"
#include "kernel.h"
#include "io.h"

task* current_task = nullptr;
task* task_list = nullptr;

extern "C" uint64_t schedule(registers* regs) {
    if (!current_task) return (uint64_t)regs;

    // Loguj przełączenie (tylko do debugowania, bo to zwolni system)
    write_serial_string("Switching task...\n");

    if (!current_task) return (uint64_t)regs;

    current_task->kstack_top = (uint64_t)regs;

    if (current_task->next) {
        current_task = current_task->next;
    } else {
        current_task = task_list;
    }

    return current_task->kstack_top;
}


task* create_task(void (*entry_point)()) {
    task* t = (task*)kmalloc(sizeof(task));
    static uint64_t next_id = 1;
    t->id = next_id++;
    t->next = nullptr;

    if (entry_point == nullptr) {
        // To jest zadanie jądra (t1). Ono już działa, więc nie budujemy mu stosu.
        if (!current_task) current_task = t;
    } else {
        // To jest nowe zadanie (t2). Budujemy mu ramkę stosu.
        uint64_t stack_phys = (uint64_t)pmm_alloc_frame();
        uint64_t stack_virt = 0x20000000 + (t->id * 0x1000); 
        vmm_map(stack_virt, stack_phys, PAGE_PRESENT | PAGE_WRITABLE);

        uint64_t* stack = (uint64_t*)(stack_virt + 4096);
        
        // Ramka IRETQ (od góry stosu)
        *(--stack) = 0x10;                  // ss
        *(--stack) = stack_virt + 4096;     // rsp
        *(--stack) = 0x202;                 // rflags (Interrupt Enable)
        *(--stack) = 0x08;                  // cs
        *(--stack) = (uint64_t)entry_point; // rip

        // Rejestry r15-rax (15 rejestrów) zainicjowane na 0
        for(int i = 0; i < 15; i++) *(--stack) = 0;

        t->kstack_top = (uint64_t)stack;
    }

    // Dodanie do listy Round Robin
    if (!task_list) {
        task_list = t;
    } else {
        task* tmp = task_list;
        while(tmp->next) tmp = tmp->next;
        tmp->next = t;
    }
    return t;
}