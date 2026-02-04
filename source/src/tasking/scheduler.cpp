#include "task.h"
#include "io.h"
#include "heap.h"
#include "kernel.h"

task* current_task = nullptr;
task* task_list = nullptr;

// Funkcja uśpienia - ustawia stan i oddaje czas procesora
void sleep(uint64_t ticks) {
    if (current_task) {
        current_task->ticks_to_sleep = ticks;
        current_task->state = STATE_SLEEPING;
        // Tutaj można by wymusić przerwanie (int 0x20), żeby od razu przełączyć
    }
}

extern "C" uint64_t schedule(registers* regs) {
    if (!current_task || !task_list) return (uint64_t)regs;

    // 1. Zapisujemy stan obecnego zadania
    current_task->kstack_top = (uint64_t)regs;
    if (current_task->state == STATE_RUNNING) {
        current_task->state = STATE_READY;
    }

    // 2. Aktualizacja uśpionych zadań (budzenie)
    task* iter = task_list;
    while (iter) {
        if (iter->state == STATE_SLEEPING) {
            if (iter->ticks_to_sleep > 0) {
                iter->ticks_to_sleep--;
            } else {
                iter->state = STATE_READY;
            }
        }
        iter = iter->next;
    }

    // 3. Wybór kolejnego zadania (Round Robin)
    task* next_task = (current_task->next) ? current_task->next : task_list;
    
    // Szukamy pierwszego zadania, które jest READY
    task* search = next_task;
    while (search->state != STATE_READY) {
        search = (search->next) ? search->next : task_list;
        if (search == next_task) break; // Wszystkie śpią? Wróć do obecnego
    }

    current_task = search;
    current_task->state = STATE_RUNNING;

    return current_task->kstack_top;
}

task* create_task(void (*entry_point)()) {
    task* t = (task*)kmalloc(sizeof(task));
    write_serial_string("[SCHEDULER] Tworze zadanie o adresie: ");
    write_serial_hex((uint64_t)t);
    write_serial_string("\n");
    static uint64_t next_id = 1;
    t->id = next_id++;
    t->state = STATE_READY;
    t->ticks_to_sleep = 0;
    t->next = nullptr;

    if (entry_point != nullptr) {
        uint8_t* stack_mem = (uint8_t*)kmalloc(4096);
        uint64_t stack_top = ((uint64_t)stack_mem + 4096);

        // Mapujemy strukturę registers na samej górze stosu
        registers* r = (registers*)(stack_top - sizeof(registers));
        
        // Czyścimy wszystko
        uint8_t* p = (uint8_t*)r;
        for(uint32_t i = 0; i < sizeof(registers); i++) p[i] = 0;

        // --- Ustawienia pod IRETQ ---
        r->ss = 0x10;            // Data segment
        r->rsp = stack_top;      // Wskaźnik stosu
        r->rflags = 0x202;       // Przerwania włączone (IF=1)
        r->cs = 0x08;            // Code segment
        r->rip = (uint64_t)entry_point;

        t->kstack_top = (uint64_t)r;
    }

    // Dodawanie do listy
    if (!task_list) {
        task_list = t;
    } else {
        task* curr = task_list;
        while(curr->next) curr = curr->next;
        curr->next = t;
    }

    return t;
}