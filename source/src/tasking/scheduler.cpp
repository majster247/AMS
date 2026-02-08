#include "task.h"
#include "io.h"
#include "heap.h"
#include "kernel.h"

// Definicje selektorów segmentów (zgodne z GDT)
#define USER_CS 0x23  // 0x20 | 3
#define USER_SS 0x1B  // 0x18 | 3
#define KERNEL_CS 0x08
#define KERNEL_SS 0x10
#define RFLAGS_IF 0x202

// Importujemy funkcję z vmm.cpp (rozwiązuje błąd 'not declared')
extern "C" uint64_t get_cr3();

task* current_task = nullptr;
task* task_list = nullptr;
task* ready_queue = nullptr; // Opcjonalnie używane

void sleep(uint64_t ticks) {
    if (current_task) {
        current_task->ticks_to_sleep = ticks;
        current_task->state = STATE_SLEEPING;
        asm volatile("int $0x20"); // Wymuszenie przełączenia (opcjonalne)
    }
}

extern "C" uint64_t schedule(registers* regs) {
    if (!current_task) return (uint64_t)regs;

    // 1. Zapisz stan obecnego zadania
    current_task->kstack_top = (uint64_t)regs;
    
    if (current_task->state == STATE_RUNNING) {
        current_task->state = STATE_READY;
    }

    // 2. Obsługa usypiania
    task* iter = task_list;
    while (iter) {
        if (iter->state == STATE_SLEEPING) {
            if (iter->ticks_to_sleep > 0) iter->ticks_to_sleep--;
            else iter->state = STATE_READY;
        }
        iter = iter->next;
    }

    // 3. Wybór następnego zadania (Round Robin)
    task* next_task = current_task->next;
    if (!next_task) next_task = task_list;

    task* start_node = next_task;
    while (next_task->state != STATE_READY) {
        next_task = next_task->next;
        if (!next_task) next_task = task_list;
        
        // Jeśli przeszliśmy całą listę i nic nie ma:
        if (next_task == start_node) {
             return current_task->kstack_top;
        }
    }

    current_task = next_task;
    current_task->state = STATE_RUNNING;

    // Tu w przyszłości dodasz aktualizację TSS RSP0 dla syscalli!
    
    return current_task->kstack_top;
}

task* create_task(void (*entry_point)()) {
    task* t = (task*)kmalloc(sizeof(task));
    write_serial_string("[SCHEDULER] Tworze zadanie KERNEL\n");
    
    static uint64_t next_id = 1;
    t->id = next_id++;
    t->state = STATE_READY;
    t->ticks_to_sleep = 0;
    t->next = nullptr;
    t->cr3 = get_cr3(); // Wątki jądra dzielą przestrzeń adresową

    // Alokacja stosu
    uint64_t stack_size = 4096;
    uint8_t* stack_mem = (uint8_t*)kmalloc(stack_size);
    uint64_t stack_top = (uint64_t)stack_mem + stack_size;
    
    t->kernel_stack = stack_top;

    // Przygotowanie rejestrów na stosie
    registers* r = (registers*)(stack_top - sizeof(registers));
    memset(r, 0, sizeof(registers));

    r->cs = KERNEL_CS;
    r->ss = KERNEL_SS; 
    r->rip = (uint64_t)entry_point;
    r->rflags = RFLAGS_IF;
    r->rsp = stack_top;

    t->kstack_top = (uint64_t)r;

    if (!task_list) task_list = t;
    else {
        task* curr = task_list;
        while(curr->next) curr = curr->next;
        curr->next = t;
    }
    return t;
}

void scheduler_add_user_task(void* entry_point, void* user_stack) {
    task* t = (task*)kmalloc(sizeof(task)); // Używamy małego 'task'
    memset(t, 0, sizeof(task));
    
    write_serial_string("[SCHEDULER] Tworze zadanie USER (Ring 3)\n");

    static uint64_t next_id = 1000;
    t->id = next_id++;
    t->state = STATE_READY;
    t->next = nullptr;
    t->cr3 = get_cr3(); // Na razie współdzielimy CR3

    // Stos jądra dla tego procesu (tutaj trafi CPU po przerwaniu/syscallu)
    uint64_t kstack_size = 4096;
    uint8_t* kstack_mem = (uint8_t*)kmalloc(kstack_size);
    uint64_t kstack_top = (uint64_t)kstack_mem + kstack_size;

    t->kernel_stack = kstack_top;

    // Budujemy sztuczną ramkę przerwania, żeby `iretq` nas przeniósł do Ring 3
    registers* r = (registers*)(kstack_top - sizeof(registers));
    memset(r, 0, sizeof(registers));

    r->ss = USER_SS;               // Segment Danych Użytkownika
    r->rsp = (uint64_t)user_stack; // Stos Użytkownika
    r->rflags = RFLAGS_IF;         // Przerwania włączone
    r->cs = USER_CS;               // Segment Kodu Użytkownika
    r->rip = (uint64_t)entry_point;// Punkt wejścia programu

    t->kstack_top = (uint64_t)r;

    if (!task_list) task_list = t;
    else {
        task* curr = task_list;
        while(curr->next) curr = curr->next;
        curr->next = t;
    }
}