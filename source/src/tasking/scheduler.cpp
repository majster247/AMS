#include "task.h"
#include "vmm.h"
#include "io.h"
#include "kernel.h"
#include "heap.h"

task* current_task = nullptr;
task* task_list = nullptr;

static uint64_t next_id = 1; // Zaczynamy od 1, bo 0 to zadanie jądra

extern "C" uint64_t schedule(registers* regs) {
    // Każde wejście do schedulera zmienia kolor znaku w rogu ekranu
    static uint8_t color = 0;
    *((uint8_t*)0xB8001) = ++color; 

    if (!current_task || !task_list) return (uint64_t)regs;

    current_task->kstack_top = (uint64_t)regs;
    current_task = (current_task->next) ? current_task->next : task_list;

    // Jeśli przełączyło na Task 2, wypisz '2' na ekranie VGA
    if (current_task->id == 2) {
        *((uint16_t*)0xB8002) = (uint16_t)('2' | (0x02 << 8)); // Zielona dwójka
    }

    return current_task->kstack_top;
}

task* create_task(void (*entry_point)()) {
    task* t = (task*)kmalloc(sizeof(task));
    static uint64_t next_id = 1;
    t->id = next_id++;
    t->next = nullptr;

    if (entry_point != nullptr) {
        // Używamy statycznego bufora na stos, żeby nie ufać kmallocowi
        static uint8_t task_b_stack[4096] __attribute__((aligned(16)));
        
        // Ustawiamy szczyt stosu na KONIEC tablicy i wyrównujemy do 16 bajtów
        uint64_t stack_top = ((uint64_t)task_b_stack + 4096) & ~0x0FULL;
        uint64_t* stack = (uint64_t*)stack_top;

        // --- KONSTRUKCJA STOSU IRETQ ---
        *(--stack) = 0x10;             // SS
        *(--stack) = stack_top;        // RSP
        *(--stack) = 0x202;            // RFLAGS (IF=1)
        *(--stack) = 0x08;             // CS
        *(--stack) = (uint64_t)entry_point;

        // --- 15 REJESTRÓW (RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8-R15) ---
        for(int i = 0; i < 15; i++) {
            *(--stack) = 0;
        }

        t->kstack_top = (uint64_t)stack;
    }

    // Standardowe dopinanie do listy (upewnij się, że kmain wywołuje to dla nullptr i dla second_task)
    if (!task_list) {
        task_list = t;
    } else {
        task* curr = task_list;
        while(curr->next) curr = curr->next;
        curr->next = t;
    }

    return t;
}