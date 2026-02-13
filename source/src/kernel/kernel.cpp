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
#include <setjmp.h>

jmp_buf kernel_jmp_buf;

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

//kernel task
extern "C" task* kernel_task = nullptr;

extern "C" void gdt_init();
extern "C" void syscall_init();
extern "C" void switch_to_kernel_stack(void* new_stack, void (*func)());

Desktop* main_desktop = nullptr;
//dodatkowa funkcja kmalloc dla kernel.cpp
extern "C" void* kmalloc(size_t size);
extern "C" void* kfree(void* ptr);
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);


// --- GŁÓWNA FUNKCJA TWORZENIA PROCESU ---
extern "C" int sys_exec(const char* path, int argc, char** argv) {
    write_serial_string("[KERN] SPAWNING PROCESS: ");
    write_serial_string(path);
    write_serial_string("\n");
    
    write_serial_string("[KERN] argc=");
    write_serial_dec(argc);
    write_serial_string("\n");
    
    for (int i = 0; i < argc; i++) {
        write_serial_string("[KERN] argv[");
        write_serial_dec(i);
        write_serial_string("] = \"");
        if (argv[i]) write_serial_string(argv[i]);
        write_serial_string("\"\n");
    }
    
    vfs_node* file = vfs_find(path);
    if (!file) {
        write_serial_string("[KERN] Error: File not found.\n");
        return -1;
    }

    // 1. Kopiuj argumenty do bufora kernela
    char arg_buf[4096];
    char* k_argv[32];
    int offset = 0;
    for (int i = 0; i < argc && i < 32; i++) {
        k_argv[i] = &arg_buf[offset];
        k_strcpy(k_argv[i], argv[i]);
        offset += k_strlen(argv[i]) + 1;
    }

    // 2. Czytaj nagłówek ELF
    Elf64_Ehdr header;
    if (vfs_read(file, 0, sizeof(header), (uint8_t*)&header) != sizeof(header)) return -1;
    if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E') return -1;

    // 3. Czytaj PHDR
    uint8_t ph_buf[2048];
    vfs_read(file, header.e_phoff, header.e_phnum * header.e_phentsize, ph_buf);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)ph_buf;

    uint64_t max_vaddr = 0;

    // 4. Mapuj segmenty
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            uint64_t filesz = phdrs[i].p_filesz;

            uint64_t start_p = vaddr & ~0xFFF;
            uint64_t end_p = (vaddr + memsz + 0xFFF) & ~0xFFF;

            for (uint64_t p = start_p; p < end_p; p += 4096) {
                void* phys = pmm_alloc_frame();
                if (phys) {
                    vmm_map_page(p, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                    uint64_t* ptr = (uint64_t*)(p + 0xFFFF800000000000ULL);
                    k_memset(ptr, 0, 4096); // Zeruj przez identity mapping
                }
            }
            
            if (filesz > 0) {
                vfs_read(file, phdrs[i].p_offset, filesz, (uint8_t*)vaddr);
            }

            if (end_p > max_vaddr) {
                max_vaddr = end_p;
            }
        }
    }

    // 5. Stos użytkownika (1MB na końcu user space)
    uint64_t stack_top = 0xC0000000 - 0x1000;
    uint64_t stack_pages = 256; // 1MB
    
    for (uint64_t i = 0; i < stack_pages; i++) {
        uint64_t virt = stack_top - (i * 4096);
        void* phys = pmm_alloc_frame();
        if (phys) {
            vmm_map_page(virt, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
            // Zeruj przez identity mapping
            k_memset((void*)((uint64_t)phys + 0xFFFF800000000000ULL), 0, 4096);
        }
    }

    uint64_t user_rsp = stack_top;

    // ✅ KLUCZOWE: Pushuj argumenty na stos OD KOŃCA!
    
    // Krok 1: Pushuj same stringi (od końca)
    uint64_t* user_argv_ptrs = (uint64_t*)kmalloc(argc * sizeof(uint64_t));
    
    // Wykorzystaj fakt, że identity mapping (0-16GB) pokrywa user stack!
    for (int i = argc - 1; i >= 0; i--) {
        uint64_t arg_len = k_strlen(k_argv[i]) + 1;
        user_rsp -= arg_len;
        user_rsp &= ~0x7ULL;
        
        user_argv_ptrs[i] = user_rsp;
        
        // ✅ Kopiuj bezpośrednio (user_rsp < 16GB, więc jest w identity mapping!)
        if (user_rsp < 0x400000000ULL) { // < 16GB
            k_memcpy((void*)user_rsp, k_argv[i], arg_len);
        } else {
            write_serial_string("[KERN] ERROR: user_rsp above 16GB!\n");
            return -1;
        }

        
        write_serial_string("[KERN] arg[");
        write_serial_dec(i);
        write_serial_string("] at 0x");
        write_serial_hex(user_rsp);
        write_serial_string(" = \"");
        write_serial_string(k_argv[i]);
        write_serial_string("\"\n");
    }
    
    // Krok 2: Wyrównaj do 16 bajtów
    user_rsp &= ~0xFULL;
    
    // Krok 3: Pushuj NULL terminator dla argv
    user_rsp -= 8;
    uint64_t phys_null = vmm_get_phys(user_rsp);
    *(uint64_t*)(phys_null + 0xFFFF800000000000ULL) = 0;
    
    // Krok 4: Pushuj tablicę wskaźników argv[] (od końca)
    for (int i = argc - 1; i >= 0; i--) {
        user_rsp -= 8;
        uint64_t phys = vmm_get_phys(user_rsp);
        *(uint64_t*)(phys + 0xFFFF800000000000ULL) = user_argv_ptrs[i];
    }
    
    uint64_t argv_ptr = user_rsp; // Wskaźnik na argv[0]
    
    // Krok 5: Pushuj argc
    user_rsp -= 8;
    uint64_t phys_argc = vmm_get_phys(user_rsp);
    *(uint64_t*)(phys_argc + 0xFFFF800000000000ULL) = argc;

    // Wyrównaj stos do 16 bajtów (wymagane przez x86_64 ABI)
    user_rsp &= ~0xFULL;

    // Debug: Sprawdź co jest na stosie
    write_serial_string("[DEBUG] User RSP: 0x");
    write_serial_hex(user_rsp);
    write_serial_string("\n");
    write_serial_string("[DEBUG] argc at: 0x");
    write_serial_hex(user_rsp);
    write_serial_string(" = ");
    write_serial_dec(argc);
    write_serial_string("\n");
    write_serial_string("[DEBUG] argv at: 0x");
    write_serial_hex(argv_ptr);
    write_serial_string("\n");
    
    // Sprawdź pierwszy argument
    uint64_t first_argv_phys = vmm_get_phys(argv_ptr);
    uint64_t first_str_ptr = *(uint64_t*)(first_argv_phys + 0xFFFF800000000000ULL);
    uint64_t first_str_phys = vmm_get_phys(first_str_ptr);
    char* first_str = (char*)(first_str_phys + 0xFFFF800000000000ULL);
    
    write_serial_string("[DEBUG] First argv ptr: 0x");
    write_serial_hex(argv_ptr);
    write_serial_string(" First argv content: ");
    write_serial_string(first_str);
    write_serial_string("\n");
    write_serial_string("[DEBUG] argc: ");
    write_serial_dec(argc);
    write_serial_string("\n");

    kfree(user_argv_ptrs);

    // 6. Rejestr task
    current_task->virt_memory_top = max_vaddr + 0x80000000;
    
    scheduler_switch_to_user(header.e_entry, user_rsp);
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

    // tcc args
    const char* tcc_args[] = { "tcc", "-I/", "-c", "/kupa.c", nullptr };

    write_serial_string("[TEST] Spawning TCC with args:\n");
    for (int i = 0; tcc_args[i] != nullptr; i++) {
        write_serial_string(tcc_args[i]);
        write_serial_string(" ");
    }
    write_serial_string("\n");

    write_serial_string("[TEST] Preparing to spawn TCC, spawning kernel task for return like voldemort...\n");
    
    // ✅ Stwórz kernel task
    kernel_task = create_kernel_task();
    if (!kernel_task) {
        write_serial_string("[ERROR] Failed to create kernel task!\n");
        while(1) asm("hlt");
    }

    // ✅ Ustaw RIP na label PONIŻEJ
    kernel_task->rip = (uint64_t)&&after_exec;
    
    // ✅ Zapisz RSP i CR3
    asm volatile("mov %%rsp, %0" : "=r"(kernel_task->kstack_top));
    kernel_task->cr3 = get_cr3();   
    // ✅ Uruchom TCC (TYLKO RAZ, bez goto!)
    sys_exec("/tcc", 4, (char**)tcc_args);
    
    // ❌ NIGDY nie dotrzemy tutaj (sys_exec nie wraca)

    //jak to nie? wracamy i wykonujemy kod poniżej, bo sys_exec ustawił RIP kernel_task na after_exec i po EXIT z tcc skoczymy do after_exec, gdzie mamy main loop GUI
    const char* tcc_args2[] = { "tcc", "-run", "/kupa.c", nullptr };
    sys_exec("/tcc", 3, (char**)tcc_args2);

    
after_exec:
    // ✅ TUTAJ wrócisz po EXIT
    write_serial_string("\n");
    write_serial_string("╔════════════════════════════════════════╗\n");
    write_serial_string("║  🎉 RETURNED FROM TCC SUCCESSFULLY! 🎉 ║\n");
    write_serial_string("╚════════════════════════════════════════╝\n");
    write_serial_string("\n");
    
    // ✅ Main loop (GUI działa!)
    write_serial_string("[KERNEL] Entering main loop...\n");
    
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

    // Initialize kernel task
    kernel_task = current_task;

    asm volatile("cli");
    switch_to_kernel_stack((void*)current_task->kstack_top, kmain_post_stack_switch);
    while(1);
}