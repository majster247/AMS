#include "kernel.h"
#include "vmm.h"
#include "task.h"
#include "io.h"
#include "vfs.h"
#include "pci.h"
#include "ext2.h"
#include "idt.h"
#include "graphics.h"
#include "mouse.h"
#include "gui.h"
#include "elf.h"

#define PT_LOAD 1
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

// DEFINICJE SELEKTORÓW (Mszą pasować do GDT w gdt.cpp!)
#define USER_CS 0x33  // Index 6 | 3
#define USER_SS 0x2B  // Index 5 | 3

// --- HELPERY ---
static int k_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static void k_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++)); // To jest OK, kopiuje \0 na końcu
}
// -------------------------------------------------

extern "C" void pci_init(); 
extern ahci_port* sata_port;
static uint8_t pmm_bitmap_buffer[132000]; 

extern "C" void gdt_init();
extern "C" void syscall_init();
extern "C" void switch_to_kernel_stack(void* new_stack, void (*func)());

Desktop* main_desktop = nullptr;



// --- GŁÓWNA FUNKCJA TWORZENIA PROCESU ---
// --- GŁÓWNA FUNKCJA TWORZENIA PROCESU (POPRAWIONA CAŁOŚĆ) ---
extern "C" int sys_exec(const char* path, int argc, char** argv) {
    write_serial_string("[KERN] SPAWNING PROCESS: ");
    write_serial_string(path);
    write_serial_string("\n");

    // 1. Znajdź plik
    vfs_node* file = vfs_find(path);
    if (!file) {
        write_serial_string("[KERN] Error: File not found.\n");
        return -1;
    }

    // 2. Kopiuj argumenty do bufora kernela (żeby nie zniknęły przy zmianie mapowania)
    char arg_buf[4096];
    char* k_argv[32];
    int offset = 0;
    for (int i = 0; i < argc && i < 32; i++) {
        k_argv[i] = &arg_buf[offset];
        k_strcpy(k_argv[i], argv[i]);
        offset += k_strlen(argv[i]) + 1;
    }

    // 3. Czytaj nagłówek ELF (TU DEKLARUJEMY header!)
    Elf64_Ehdr header;
    if (vfs_read(file, 0, sizeof(header), (uint8_t*)&header) != sizeof(header)) return -1;
    if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E') return -1;

    // 4. Czytaj PHDR (Program Headers)
    uint8_t ph_buf[2048];
    vfs_read(file, header.e_phoff, header.e_phnum * header.e_phentsize, ph_buf);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)ph_buf;

    uint64_t max_vaddr = 0;

    // 5. Mapuj segmenty do pamięci (LOAD)
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            uint64_t filesz = phdrs[i].p_filesz;

            uint64_t start_p = vaddr & ~0xFFF;
            uint64_t end_p = (vaddr + memsz + 0xFFF) & ~0xFFF;

            // Mapujemy strony
            for (uint64_t p = start_p; p < end_p; p += 4096) {
                void* phys = pmm_alloc_frame();
                if (phys) {
                    vmm_map_page(p, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                    // Zerujemy stronę (ważne dla BSS)
                    uint64_t* ptr = (uint64_t*)p;
                    for(int k=0; k<512; k++) ptr[k] = 0;
                }
            }
            
            // Wczytujemy dane z pliku
            if (filesz > 0) {
                vfs_read(file, phdrs[i].p_offset, filesz, (uint8_t*)vaddr);
            }

            if (end_p > max_vaddr) max_vaddr = end_p;
        }
    }

    // 6. Przygotuj Stos Użytkownika (1MB)
    uint64_t stack_top = 0x7FFFFFFF0000;
    uint64_t stack_size = 1024 * 1024; // 1MB
    for (uint64_t i = 0; i < stack_size; i+=4096) {
        void* p = pmm_alloc_frame();
        vmm_map_page(stack_top - stack_size + i, (uint64_t)p, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
    }

    // 7. BUDOWANIE STOSU (ARGV + ENVP + ALIGNMENT)
    uint64_t current_rsp = stack_top - 1024; 
    uint64_t argv_ptrs[32];

    // A. Kopiuj stringi na stos Ring 3
    for (int i = 0; i < argc; i++) {
        int len = k_strlen(k_argv[i]) + 1;
        current_rsp -= len;
        k_strcpy((char*)current_rsp, k_argv[i]); 
        argv_ptrs[i] = current_rsp;
    }
    
    // B. Wyrównanie bazowe do 16 bajtów
    current_rsp &= ~0xF; 

    // C. Obliczamy padding
    // Pychamy: 1x NULL(envp) + 1x NULL(argv_end) + argc * wskaźniki + 1x argc(val)
    int items_to_push = 1 + 1 + argc + 1;
    
    // Jeśli liczba elementów jest NIEPARZYSTA, to (16 - 8*Nieparzysta) % 16 != 0. 
    // Musimy dodać padding, żeby liczba elementów była PARZYSTA.
    if ((items_to_push % 2) != 0) {
        current_rsp -= 8; 
        *(uint64_t*)current_rsp = 0; // Padding
    }

    // D. Pychamy dane na stos
    current_rsp -= 8; *(uint64_t*)current_rsp = 0; // NULL (Koniec ENVP) - TEGO BRAKOWAŁO!
    current_rsp -= 8; *(uint64_t*)current_rsp = 0; // NULL (Koniec ARGV)

    for (int i = argc - 1; i >= 0; i--) {
        current_rsp -= 8;
        *(uint64_t*)current_rsp = argv_ptrs[i];
    }
    
    uint64_t user_argv_ptr = current_rsp; // Adres argv dla RSI

    current_rsp -= 8; 
    *(uint64_t*)current_rsp = (uint64_t)argc; // argc dla RDI

    // 8. Rejestracja zadania w Schedulerze
    scheduler_add_user_task((void*)header.e_entry, (void*)current_rsp);
    
    // Pobierz wskaźnik do nowego zadania
    extern task* task_list;
    task* t = task_list;
    while(t->next) t = t->next;

    // 9. USTAWIENIE REJESTRÓW (POPRAWKA WSKAŹNIKA!)
    // kstack_top w schedulerze wskazuje już na początek struktury, NIE ODEJMUJEMY sizeof!
    registers* r = (registers*)t->kstack_top; 
    
    r->rip = header.e_entry;
    r->rsp = current_rsp; 
    r->rdi = (uint64_t)argc; 
    r->rsi = user_argv_ptr;
    r->rdx = 0; // envp (puste)
    
    r->cs = 0x33; // User Code
    r->ss = 0x2B; // User Data
    r->rflags = 0x202; // IF on

    write_serial_string("[DEBUG] Regs set. RIP: ");
    write_serial_hex(r->rip);
    write_serial_string(" RSP: ");
    write_serial_hex(r->rsp);
    write_serial_string("\n");

    // 10. Inicjalizacja Sterty (Heap)
    t->virt_memory_top = 0x80000000;
    void* first_page = pmm_alloc_frame();
    vmm_map_page(0x80000000, (uint64_t)first_page, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
    write_serial_string("[KERN] Heap start set to: 0x80000000\n");

    write_serial_string("[KERN] Process spawned! Args pushed to stack.\n");
    return 0; 
}

void enable_sse() {
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Czysty EM (Emulation)
    cr0 |= (1 << 1);  // Ustaw MP (Monitor Coprocessor)
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Ustaw OSFXSR (FXSAVE/FXRSTOR support)
    cr4 |= (1 << 10); // Ustaw OSXMMEXCPT (SIMD Exception support)
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}

void log_step(const char* msg) {
    write_serial_string("[KERNEL] ");
    write_serial_string(msg);
    write_serial_string("\n");
}

void kmain_post_stack_switch() {
    asm volatile("sti");
    log_step("GUI Loop starting...");

    //sprawdzamy, czy TCC reaguje na help (jeśli nie, to znaczy, że coś poszło bardzo źle z mapowaniem lub argumentami)
    //pokaż jak tcc dostanie argsy w serialu dla testu
    const char* tcc_args[] = { "tcc", "-nostdlib", "-E", "/kupa.c", nullptr };
    for (int i = 0; tcc_args[i] != nullptr; i++) {
        write_serial_string(tcc_args[i]);
        write_serial_string(" ");
    }
    write_serial_string("\n");
    sys_exec("/tcc", 4, (char**)tcc_args);

    while(true) {
        main_desktop->Update(mouse_x, mouse_y, mouse_left_pressed);
        main_desktop->Draw();
        mouse_draw();
        graphics_flip();
        asm volatile("int $0x20");
    }
}

extern "C" void kmain(uint64_t multiboot_info_address) {
    init_serial();
    timer_init(100);
    pmm_init(4096, (void*)pmm_bitmap_buffer);
    parse_multiboot(multiboot_info_address);
    
    vmm_init_direct_map(0); 
    
    extern uint64_t initrd_end; 
    uint64_t heap_start = 0x2000000; 
    uint64_t heap_size = 1024*1024*1024; // 1GB dla kernela
    if (initrd_end > heap_start) heap_start = (initrd_end + 0x1FFFFF) & ~0x1FFFFFULL;
    heap_init((void*)heap_start, heap_size);
    pmm_mark_chunk_used(heap_start, heap_size);

    keyboard_init();
    mouse_init();
    
    gdt_init();     
    syscall_init();
    enable_sse();
    scheduler_init_kernel_task();
    idt_init();

    // DIAGNOSTYKA GDT/TSS (Poprawiona)
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) current_gdtr;
    uint16_t current_tss;
    asm volatile("sgdt %0" : "=m"(current_gdtr));
    asm volatile("str %0" : "=r"(current_tss));

    write_serial_string("[KERN] GDT Actual Base: "); write_serial_hex(current_gdtr.base);
    write_serial_string(" Limit: "); write_serial_hex(current_gdtr.limit);
    write_serial_string("\n[KERN] TSS Active Selector: "); write_serial_hex(current_tss);
    write_serial_string("\n");

    vfs_init();
    pci_init(); 
    
    if (sata_port && ext2_init(sata_port)) {
        log_step("BOOT: System plikow EXT2 zamontowany.");
    } else {
        log_step("BOOT: OSTRZEZENIE - Brak dysku lub EXT2.");
    }

    graphics_init_double_buffer();
    main_desktop = new Desktop();
    main_desktop->Init();
    main_desktop->AddWindow(new TerminalWindow(100, 100));

    asm volatile("cli");
    switch_to_kernel_stack((void*)current_task->kstack_top, kmain_post_stack_switch);
    while(1);
}