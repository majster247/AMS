#include "task.h"
#include "io.h"
#include "heap.h"
#include "kernel.h"
#include "gdt.h"

// Definicje selektorów segmentów (zgodne z GDT)
#define USER_CS 0x33  // Zmiana z 0x23
#define USER_SS 0x2B  // Zmiana z 0x1B
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
    // 1. Zapisz stan obecnego zadania (To zapisze stan pętli while kernela!)
    if (current_task) {
        current_task->kstack_top = (uint64_t)regs;
        if (current_task->state == STATE_RUNNING) {
            current_task->state = STATE_READY;
        }
    }

    // 2. Prosty Round-Robin
    task* next = current_task ? current_task->next : task_list;
    if (!next) next = task_list;

    // Pętla szukająca żywego zadania
    int attempts = 0;
    while(attempts < 20) { // Zabezpieczenie
        if (!next) next = task_list;
        
        // Jeśli to ZOMBIE, olej go
        if (next->state == STATE_ZOMBIE) {
            next = next->next;
            attempts++;
            continue;
        }

        // Jeśli to SLEEPING
        if (next->state == STATE_SLEEPING) {
             if (get_system_ticks() >= next->ticks_to_sleep) next->state = STATE_READY;
             else {
                 next = next->next;
                 attempts++;
                 continue;
             }
        }
        
        // Mamy kandydata!
        break; 
    }
    
    // Jeśli nie znaleźliśmy nikogo (np. user umarł), wracamy do Task 0 (Kernel)
    if (!next || next->state == STATE_ZOMBIE) {
        // Znajdź kernela
        next = task_list;
        while(next && next->id != 0) next = next->next;
    }

    current_task = next;
    current_task->state = STATE_RUNNING;

    if (current_task->id >= 1000) {
        write_serial_string("[SCHEDULER] SKACZE DO RING 3! RIP: ");
        registers* r = (registers*)current_task->kstack_top;
        write_serial_hex(r->rip);
        write_serial_string("\n");
    }
    
    // Tutaj normalnie ładujesz TSS->rsp0 = current_task->kernel_stack
    // Ale dla Task 0 (Kernel) kernel_stack może być 0 lub nieustawiony, 
    // bo Kernel działa w Ring 0 i nie potrzebuje zmiany stosu przy przerwaniu.
    // DLA USERA TRZEBA USTAWIAĆ TSS!

    system_tss.rsp0 = current_task->kernel_stack;
    cpu_data.kernel_stack = current_task->kernel_stack;
    

    //Bardzo ochydny sposób na debugowanie, ale przynajmniej widać, że scheduler działa i przełącza zadania.
    /*
    write_serial_string("[SCHEDULER] Przelaczam na zadanie ID: ");
    write_serial_dec(current_task->id);
    write_serial_string(" RIP: ");
    registers* r = (registers*)current_task->kstack_top;
    write_serial_hex(r->rip);
    write_serial_string("\n");
    write_serial_string("[SCHEDULER] Kernel Stack: ");
    write_serial_hex(current_task->kernel_stack);
    write_serial_string("\n");
    */

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
    asm volatile("cli");

    task* t = (task*)kmalloc(sizeof(task));
    memset(t, 0, sizeof(task));
    
    write_serial_string("[SCHEDULER] Tworze zadanie USER (Ring 3)\n");

    t->id = 1000; // lub next_id++
    t->state = STATE_READY;
    t->cr3 = get_cr3(); 

    // 1. Alokacja stosu jądra
    uint64_t kstack_size = 8192; // Zwiększmy do 8KB dla bezpieczeństwa
    uint8_t* kstack_mem = (uint8_t*)kmalloc(kstack_size);
    uint64_t kstack_top = (uint64_t)kstack_mem + kstack_size;
    t->kernel_stack = kstack_top;

    // 2. Przygotowanie struktury registers na stosie jądra
    // Musimy upewnić się, że struktura jest na samym szczycie
    registers* r = (registers*)(kstack_top - sizeof(registers));
    memset(r, 0, sizeof(registers));

    // Symulujemy stan, jakby procesor właśnie skończył obsługę przerwania
    r->ss = USER_SS;            // Selector 0x1B
    r->rsp = (uint64_t)user_stack; 
    r->rflags = 0x202;          // Interrupts enabled
    r->cs = USER_CS;            // Selector 0x23
    r->rip = (uint64_t)entry_point;

    // WAŻNE: Musisz ustawić rejestry segmentowe w strukturze, 
    // jeśli Twój kod ASM je przywraca (pop rax, pop rbx itd.)
    r->rax = 0;
    r->rbx = 0;
    // ... reszta na 0

    t->kstack_top = (uint64_t)r;

    // 3. Dodawanie do listy
    if (!task_list) task_list = t;
    else {
        task* curr = task_list;
        while(curr->next) curr = curr->next;
        curr->next = t;
    }

    asm volatile("sti");
}

void scheduler_init_kernel_task() {
    task* t = (task*)kmalloc(sizeof(task));
    memset(t, 0, sizeof(task));
    
    t->id = 0;
    t->state = STATE_RUNNING;
    t->next = nullptr;
    t->cr3 = get_cr3();

    // 16KB stosu
    uint64_t kstack_size = 16384;
    void* kstack_mem = kmalloc(kstack_size);
    t->kernel_stack = (uint64_t)kstack_mem + kstack_size;
    
    // STARTUJEMY Z CZYSTEGO STOSU (BEZ OFFSETU!)
    t->kstack_top = t->kernel_stack; 

    // TSS musi wskazywać na ten sam stos
    system_tss.rsp0 = t->kernel_stack; 

    if (!task_list) task_list = t;
    else {
        t->next = task_list;
        task_list = t;
    }
    
    current_task = t;
    write_serial_string("[SCHEDULER] Kernel registered with CLEAN STACK at: ");
    write_serial_hex(t->kernel_stack);
    write_serial_string("\n");
}