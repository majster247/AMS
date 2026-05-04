#include "kernel.h"
#include "gdt.h"
#include "heap.h"
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


//wypisywanie na serial w kernelu (do debugowania)
extern "C" void write_serial_string(const char* str);
extern "C" void write_serial_hex(uint64_t value);


// --- KONFIGURACJA TESTÓW (przeniesiona do src/kernel/tests) ---
extern int current_test_idx;
extern const int TOTAL_TESTS;
extern const char* test_files[];

// --- DEFINICJE I ZMIENNE GLOBALNE ---
jmp_buf kernel_jmp_buf;

#define PT_LOAD 1
#define PT_INTERP 3
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

#define USER_CS 0x33
#define USER_SS 0x2B

extern "C" void pci_init(); 
extern ahci_port* sata_port;
task* kernel_task = nullptr;
extern "C" uint64_t g_kernel_cr3 = 0;

extern "C" void gdt_init();
extern "C" void syscall_init();
extern "C" void evdev_init();
extern "C" void drm_init();
extern "C" void switch_to_kernel_stack(void* new_stack, void (*func)());

Desktop* main_desktop = nullptr;

// --- PROTOTYPY FUNKCJI ZEWNĘTRZNYCH ---
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" void* k_memcpy(void* dest, const void* src, size_t count);
extern "C" void* k_memset(void* dest, int ch, size_t count);
extern "C" uint64_t get_cr3();
extern "C" void set_cr3(uint64_t cr3);

// --- HELPERY LOKALNE ---
static int k_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static bool is_canonical_addr(uint64_t addr) {
    uint64_t top = addr >> 48;
    return (top == 0x0000ULL) || (top == 0xFFFFULL);
}

static uint64_t lar_check(uint16_t sel, bool* ok) {
    uint64_t ar = 0;
    uint8_t zf = 0;
    asm volatile(
        "lar %2, %0\n"
        "setz %1\n"
        : "=r"(ar), "=qm"(zf)
        : "r"((uint64_t)sel)
        : "cc"
    );
    *ok = (zf != 0);
    return ar;
}

void k_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void enable_sse() {
    uint64_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Czysty bit EM (Emulation)
    cr0 |= (1ULL << 1);  // Ustaw bit MP (Monitor Coprocessor)
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Ustaw bit OSFXSR (FXSAVE/FXRSTOR support)
    cr4 |= (1ULL << 10); // Ustaw bit OSXMMEXCPT (SIMD Floating-Point Exception)
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}


// --- LOGIKA AUTOMATYZACJI TESTÓW ---

extern "C" void run_next_test();
extern "C" void kernel_return_point();
extern "C" int sys_exec(const char* path, int argc, char** argv);
static void launch_wayland_on_boot();

static void ensure_vfs_placeholder_file(const char* flat_name) {
    if (!flat_name || !flat_name[0]) return;
    if (vfs_find(flat_name)) return;

    vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
    if (!node) return;
    k_memset(node, 0, sizeof(vfs_node));
    k_strcpy(node->name, flat_name);
    node->type = FS_FILE;
    node->is_directory = false;
    node->source = FS_TAR;
    node->size = 0;
    node->max_size = 1024 * 1024;
    node->tar_data = (uint8_t*)kmalloc(node->max_size);
    if (!node->tar_data) {
        kfree(node);
        return;
    }
    k_memset(node->tar_data, 0, node->max_size);
    node->next = vfs_root;
    vfs_root = node;
}

static void launch_compiled_doom_or_fallback() {
    if (!vfs_find("/doom.elf")) {
        write_serial_string("[BOOT] TCC finished but /doom.elf still missing.\n");
        launch_wayland_on_boot();
        kernel_return_point();
        return;
    }

    write_serial_string("[BOOT] /doom.elf built successfully via TCC (autostart disabled).\n");
    launch_wayland_on_boot();
    kernel_return_point();
}

static void launch_doom_on_boot() {
    // Flat VFS compatibility: use root aliases for staged Doom files.
    if (!vfs_find("/doom.elf")) {
        write_serial_string("[BOOT] /doom.elf missing, building via /tools/compiler/tcc...\n");
        // Flat VFS + TCC linker path quirk: pre-create target file entry.
        ensure_vfs_placeholder_file("doom.elf");
        char* tcc_argv[14];
        tcc_argv[0] = (char*)"/tools/compiler/tcc";
        tcc_argv[1] = (char*)"-nostdlib";
        tcc_argv[2] = (char*)"/tools/compiler/runtime/crt0.o";
        tcc_argv[3] = (char*)"/tools/compiler/runtime/syscall.o";
        tcc_argv[4] = (char*)"/tools/compiler/runtime/ams_syscall.o";
        tcc_argv[5] = (char*)"/tools/compiler/runtime/stdlib.o";
        tcc_argv[6] = (char*)"/tools/compiler/runtime/string.o";
        tcc_argv[7] = (char*)"/tools/compiler/runtime/ctype.o";
        tcc_argv[8] = (char*)"/doom_bootstrap.c";
        tcc_argv[9] = (char*)"-o";
        tcc_argv[10] = (char*)"/doom.elf";
        tcc_argv[11] = (char*)"-I/tools/compiler/include";
        tcc_argv[12] = (char*)"-I/";
        tcc_argv[13] = nullptr;

        if (current_task) {
            kernel_task = current_task;
            kernel_task->rip = (uint64_t)launch_compiled_doom_or_fallback;
            kernel_task->cr3 = get_cr3();
        }
        int brc = sys_exec("/tools/compiler/tcc", 13, tcc_argv);
        if (brc != 0) {
            write_serial_string("[BOOT] TCC doom bootstrap build failed.\n");
        }
        return;
    }

    launch_compiled_doom_or_fallback();
}

static void launch_wayland_on_boot() {
    char* session_argv[2];
    session_argv[0] = (char*)"/wayland-session";
    session_argv[1] = nullptr;

    write_serial_string("[BOOT] Launching Wayland-first session manager...\n");

    if (current_task) {
        kernel_task = current_task;
        // Professional mode: do not auto-fall back to legacy GUI.
        kernel_task->rip = (uint64_t)launch_wayland_on_boot;
        kernel_task->cr3 = get_cr3();
    }

    int rc = sys_exec("/wayland-session", 1, session_argv);
    if (rc != 0) {
        write_serial_string("[BOOT] Wayland session manager launch failed. Retrying...\n");
    }
}

// Punkt, do którego wracamy po GUI (oryginalny)
extern "C" void kernel_return_point() {
    write_serial_string("\n[KERNEL] All tests finished. Launching GUI...\n");
    
    while(true) {
        main_desktop->Update(mouse_x, mouse_y, mouse_left_pressed);
        main_desktop->Draw();
        mouse_draw();
        graphics_flip();
        asm volatile("int $0x20");
    }
}



// --- SYSCALL EXEC (Główna logika ładowania procesów) ---

extern "C" int sys_exec(const char* path, int argc, char** argv) {
    write_serial_string("[KERN] Executing: ");
    write_serial_string(path);
    write_serial_string("\n");

    vfs_node* file = vfs_find(path);
    if (!file) return -1;

    write_serial_string("[KERN] File found. Size: ");
    write_serial_dec(file->size);
    write_serial_string(" bytes\n");
    write_serial_string("[KERN] Preparing user-space environment...\n");
    // 1. Kopiowanie argumentów na stertę jądra (zabezpieczenie)
    static char tmp_arg_buf[4096];
    k_memset(tmp_arg_buf, 0, sizeof(tmp_arg_buf));
    int offset = 0;

    // Zamiast static, użyj kmalloc, aby każdy proces miał własne, pewne argumenty
    char** k_argv = (char**)kmalloc(argc * sizeof(char*));
    for (int i = 0; i < argc; i++) {
        int len = k_strlen(argv[i]) + 1;
        k_argv[i] = (char*)kmalloc(len);
        k_memcpy(k_argv[i], argv[i], len);
    }
    write_serial_string("[KERN] Arguments copied to kernel heap.\n");

    // 2. Tablica stron procesu
    uint64_t new_pml4 = vmm_create_user_pml4();
    if (!new_pml4) return -1;

    write_serial_string("[KERN] User PML4 created at physical address: ");
    write_serial_hex(new_pml4);
    write_serial_string("\n");


    Elf64_Ehdr header;
    vfs_read(file, 0, sizeof(header), (uint8_t*)&header);

    Elf64_Phdr phdrs[header.e_phnum];
    vfs_read(file, header.e_phoff, header.e_phnum * header.e_phentsize, (uint8_t*)phdrs);

    // Dynamic ELF path: jeśli binarka ma PT_INTERP, odpal interpreter jak w Linux.
    // Minimalna strategia: execve(interpreter, [interpreter, program, args...]).
    // Dzięki temu można uruchamiać dynamiczne ELF bez ręcznego wywoływania loadera.
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP && path) {
            if (phdrs[i].p_filesz == 0 || phdrs[i].p_filesz > 255) break;
            char interp_path[256];
            k_memset(interp_path, 0, sizeof(interp_path));
            vfs_read(file, phdrs[i].p_offset, phdrs[i].p_filesz, (uint8_t*)interp_path);
            interp_path[255] = '\0';

            // unikamy zapętlenia (interpreter uruchamiany bezpośrednio)
            if (strcmp(path, interp_path) != 0) {
                int new_argc = argc + 1;
                char** new_argv = (char**)kmalloc((new_argc + 1) * sizeof(char*));
                new_argv[0] = (char*)interp_path;
                new_argv[1] = (char*)path;
                for (int a = 1; a < argc; ++a) {
                    new_argv[a + 1] = argv[a];
                }
                new_argv[new_argc] = nullptr;

                write_serial_string("[KERN] PT_INTERP detected, chaining to loader: ");
                write_serial_string(interp_path);
                write_serial_string("\n");

                int rc = sys_exec(interp_path, new_argc, new_argv);
                kfree(new_argv);
                return rc;
            }
            break;
        }
    }

    write_serial_string("[KERN] ELF header and program headers read successfully.\n");
    write_serial_string("[KERN] Mapping ELF segments into memory...\n");

    uint64_t load_bias = 0;
    bool load_bias_set = false;
    // 3. Mapowanie sekcji PT_LOAD
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (!load_bias_set) {
                load_bias = phdrs[i].p_vaddr - phdrs[i].p_offset;
                load_bias_set = true;
            }
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            uint64_t filesz = phdrs[i].p_filesz;

            for (uint64_t p = (vaddr & ~0xFFF); p < (vaddr + memsz + 0xFFF); p += 4096) {
                void* phys = pmm_alloc_frame();
                // Mapujemy stronę dla USERA
                vmm_map_page_ex(new_pml4, p, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
                
                // CZYŚCIMY pamięć używając adresu wirtualnego jądra (HHDM)!
                // PHYS_OFFSET to 0xFFFF800000000000
                k_memset((void*)((uint64_t)phys + 0xFFFF800000000000ULL), 0, 4096);
            }

            // Kopiowanie danych z pliku do pamięci
            for(uint64_t f_off = 0; f_off < filesz; f_off += 4096) {
                uint64_t chunk = (filesz - f_off > 4096) ? 4096 : filesz - f_off;
                uint64_t target_vaddr = vaddr + f_off;
                uint64_t target_phys = vmm_get_phys_ex(new_pml4, target_vaddr);
                
                // TU BYŁ BŁĄD: Musi być phys + PHYS_OFFSET
                vfs_read(file, phdrs[i].p_offset + f_off, chunk, (uint8_t*)(target_phys + 0xFFFF800000000000ULL));
            }
        }
    }
    write_serial_string("[KERN] ELF segments mapped and loaded into memory.\n");

    // 4. Stos użytkownika
    uint64_t stack_limit = 0xC0000000;
    for (uint64_t i = 0; i < 256; i++) { 
        void* phys = pmm_alloc_frame();
        vmm_map_page_ex(new_pml4, stack_limit - (i+1)*4096, (uint64_t)phys, PAGE_USER | PAGE_WRITABLE | PAGE_PRESENT);
        k_memset((void*)((uint64_t)phys + PHYS_OFFSET), 0, 4096);
    }

    write_serial_string("[KERN] User stack allocated and mapped.\n");
    write_serial_string("[KERN] Preparing stack with arguments and auxiliary vectors...\n");

    uint64_t user_rsp = stack_limit;
    uint64_t* user_argv_ptrs = (uint64_t*)kmalloc(argc * 8);

    // Najpierw kładziemy "surowe" ciągi znaków (teksty argumentów)
    for (int i = argc - 1; i >= 0; i--) {
        uint64_t len = k_strlen(k_argv[i]) + 1;
        user_rsp -= len;
        uint64_t p = vmm_get_phys_ex(new_pml4, user_rsp);
        k_memcpy((void*)(p + PHYS_OFFSET), k_argv[i], len);
        user_argv_ptrs[i] = user_rsp;
    }

    write_serial_string("[KERN] Argument strings copied to user stack.\n");
    // Wyrównujemy stos do 16 bajtów.
    // Dla Linux x86_64 ABI ustawiamy startowe RSP tak, aby _start dostał
    // RSP % 16 == 0. Wtedy po "call main" wewnątrz crt0, main ma poprawne
    // wyrównanie ABI (RSP % 16 == 8 na wejściu do funkcji).
    user_rsp &= ~0xFULL;

    // Pchamy później (argc + 5) qwordów:
    // argc + argv[] + argv_null + envp_null + auxv(type,val).
    // Gdy argc jest parzyste, liczba qwordów jest nieparzysta, więc aby
    // końcowe RSP nadal było 16-byte aligned, dodajemy 8B paddingu.
    if ((argc & 1) == 0) {
        user_rsp -= 8;
    }

    write_serial_string("[KERN] Stack aligned to 16 bytes.\n");
    // 1. PUSH Auxv (minimum Linux ABI)
    // AT_PAGESZ
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 4096;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 6;
    // AT_PHENT
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = header.e_phentsize;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 4;
    // AT_PHNUM
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = header.e_phnum;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 5;
    // AT_PHDR
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = load_bias + header.e_phoff;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 3;
    // AT_ENTRY
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = header.e_entry;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 9;
    // AT_NULL
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 0;
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 0;


    // 2. PUSH envp NULL (oznacza pustą tablicę zmiennych środowiskowych)
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 0;

    // 3. PUSH argv NULL (koniec tablicy argumentów)
    user_rsp -= 8; *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = 0;

    // 4. PUSH tablica wskaźników argv
    for (int i = argc - 1; i >= 0; i--) {
        user_rsp -= 8;
        *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = user_argv_ptrs[i];
    }
    write_serial_string("[KERN] argv pointers pushed to user stack.\n");
    write_serial_string("[KERN] Final user RSP: ");
    write_serial_hex(user_rsp);
    write_serial_string("\n");

    
    // 5. PUSH argc
    user_rsp -= 8;
    *(uint64_t*)(vmm_get_phys_ex(new_pml4, user_rsp) + PHYS_OFFSET) = (uint64_t)argc;

    write_serial_string("[KERN] argc pushed to user stack.\n");
    write_serial_string("[KERN] User-space environment setup complete. Preparing to switch context...\n");

    kfree(user_argv_ptrs);
    for(int i=0; i<argc; i++) kfree(k_argv[i]);
    kfree(k_argv);

    write_serial_string("[KERN] Cleaned up kernel heap allocations for arguments.\n");

    // 5. Przełączenie kontekstu do nowego procesu
    // Ważne: task jest wielokrotnie używany przez test-runner.
    // Resetujemy stan sterty userspace (brk), bo mapowania są nowe (new_pml4),
    // a stary virt_memory_top z poprzedniego procesu powoduje page faulty.
    current_task->virt_memory_top = 0;
    current_task->cr3 = new_pml4;
    write_serial_string("[KERN] Updated current task's CR3 to new PML4.\n");
    write_serial_string("[KERN] Setting up user-space execution context and jumping to user code...\n");

    uint64_t entry_phys = vmm_get_phys_ex(new_pml4, header.e_entry);
    uint64_t rsp_phys = vmm_get_phys_ex(new_pml4, user_rsp);
    uint64_t rsp_top_phys = vmm_get_phys_ex(new_pml4, user_rsp + 8);

    write_serial_string("[KERN][DBG] entry canonical: ");
    write_serial_string(is_canonical_addr(header.e_entry) ? "yes\n" : "no\n");
    write_serial_string("[KERN][DBG] rsp canonical: ");
    write_serial_string(is_canonical_addr(user_rsp) ? "yes\n" : "no\n");
    write_serial_string("[KERN][DBG] entry phys: ");
    write_serial_hex(entry_phys);
    write_serial_string("\n[KERN][DBG] rsp phys: ");
    write_serial_hex(rsp_phys);
    write_serial_string("\n[KERN][DBG] rsp+8 phys: ");
    write_serial_hex(rsp_top_phys);
    bool user_cs_ok = false;
    bool user_ss_ok = false;
    uint64_t user_cs_ar = lar_check(0x33, &user_cs_ok);
    uint64_t user_ss_ar = lar_check(0x2B, &user_ss_ok);
    write_serial_string("\n[KERN][DBG] LAR user CS ok: ");
    write_serial_string(user_cs_ok ? "yes " : "no ");
    write_serial_hex(user_cs_ar);
    write_serial_string("\n[KERN][DBG] LAR user SS ok: ");
    write_serial_string(user_ss_ok ? "yes " : "no ");
    write_serial_hex(user_ss_ar);
    write_serial_string("\n");

    if (!entry_phys || !rsp_phys) {
        write_serial_string("[KERN][FATAL] User mapping missing before ring3 jump.\n");
        asm volatile("cli; hlt");
    }

    // Uaktualnij stos syscall-entry dla bieżącego taska przed wejściem do ring3.
    cpu_data.kernel_stack = kernel_task ? kernel_task->kstack_top : current_task->kstack_top;

    set_cr3(new_pml4); 
    write_serial_string("[KERN] CR3 switched to new PML4. Jumping to user-space entry point...\n");
    write_serial_string("[KERN] Entry point: ");
    write_serial_hex(header.e_entry);
    write_serial_string("\nUser stack (RSP): ");
    write_serial_hex(user_rsp);
    write_serial_string("\n");
    scheduler_switch_to_user(header.e_entry, user_rsp);
    
    return 0;
}

// Punkt powrotu po każdym teście TCC
extern "C" void kernel_test_return_point() {
    write_serial_string("[TEST RUNNER] TCC finished test ");
    write_serial_dec(current_test_idx + 1);
    write_serial_string(". Checking for next...\n");

    current_test_idx++;
    run_next_test();
}

extern "C" void run_next_test() {
    if (current_test_idx >= TOTAL_TESTS) {
        write_serial_string("\n╔════════════════════════════════════════╗\n");
        write_serial_string("║    🎉 ALL TCC UNIT TESTS COMPLETED!    ║\n");
        write_serial_string("╚════════════════════════════════════════╝\n");
        // Tutaj możesz dodać hlt, żeby system nie zapętlił się
        asm volatile("cli; hlt");
        return;
    }

    const char* c_file = test_files[current_test_idx];

    // Przygotowujemy argv:
    // argv[0] = "/tools/compiler/tcc"  (nazwa binarki)
    // argv[1] = "-run"  (kompiluj i uruchom od razu)
    // argv[2] = c_file  (ścieżka do źródła, np. "/tests/tcc/t1_basic.c")
    // argv[3] = nullptr (koniec tablicy)
    char* tcc_argv[5];
    tcc_argv[0] = (char*)"/tools/compiler/tcc";
    tcc_argv[1] = (char*)"-run";
    tcc_argv[2] = (char*)"-nostdlib";
    tcc_argv[3] = (char*)c_file;
    tcc_argv[4] = nullptr;

    write_serial_string("\n[TEST RUNNER] Running Test ");
    write_serial_dec(current_test_idx + 1);
    write_serial_string("/");
    write_serial_dec(TOTAL_TESTS);
    write_serial_string(": tcc -run -nostdlib ");
    write_serial_string(c_file);
    write_serial_string("\n");

    // Reset stosu jądra (tak jak miałeś - to jest poprawne)
    extern uint8_t main_kernel_stack[16384];
    uint64_t clean_stack = (uint64_t)main_kernel_stack + 16384;

    //Pobieramy kernel task i ustawiamy jego RIP na punkt powrotu testu

   if(current_task) {
        kernel_task = current_task;
    } else {
        write_serial_string("[ERROR] No current task found! Cannot set up test environment.\n");
        asm volatile("hlt");
    }
   

    kernel_task->rip = (uint64_t)kernel_test_return_point;
    kernel_task->kstack_top = clean_stack;
    kernel_task->cr3 = get_cr3();

    // KLUCZ: sys_exec musi wywołać "/tools/compiler/tcc", a nie plik ".c"
    // argc = 4 (tcc, -run, -nostdlib, plik.c)
    int res = sys_exec("/tools/compiler/tcc", 4, tcc_argv);
    
    if(res != 0) {
        write_serial_string("[ERROR] Failed to launch TCC binary!\n");
        asm volatile("hlt");
    }
}


void kmain_post_stack_switch() {
    write_serial_string("[KERNEL] Stack switched successfully. Continuing initialization...\n");
    write_serial_string("[KERNEL] Initializing remaining subsystems...\n");
    pic_remap();
    write_serial_string("[KERNEL] PIC Remapped.\n");
    write_serial_string("[KERNEL] Enabling interrupts...\n");
    asm volatile("sti");
    write_serial_string("[KERNEL] Interrupts enabled.\n");
    write_serial_string("[KERNEL] System reach stable post-stack state.\n");
    
    // Tryb profesjonalny: Wayland jest jedyną domyślną sesją desktopową.
    launch_wayland_on_boot();
    while (1) {
        asm volatile("hlt");
    }
}

extern "C" uint64_t stack_bottom; // Symbole ze start.s
extern "C" uint64_t stack_top;

extern "C" void kmain(uint64_t multiboot_info_address) {
    __asm__ volatile("cli"); // Wyłącz przerwania na czas inicjalizacji
    write_serial_string("\n--- AMS KERNEL BOOTING ---\n");

    write_serial_string("Multiboot pointer: ");
    write_serial_hex(multiboot_info_address);
    write_serial_string("\n");

    parse_multiboot(multiboot_info_address);
    uint64_t ram_size_mb = total_ram_bytes / (1024 * 1024);
    // BEZPIECZNIK: Jeśli parser nie wykrył RAMu, ustawiamy na sztywno 1GB
    if (ram_size_mb == 0) {
        write_serial_string("[WARN] Multiboot failed to report RAM. Forcing 1024MB.\n");
        ram_size_mb = 1024;
        total_ram_bytes = 1024ULL * 1024 * 1024;
    }

    write_serial_string("Detected RAM: ");
    write_serial_dec(ram_size_mb);
    write_serial_string(" MB\n");
    write_serial_string("Initializing Physical Memory Manager...\n");
    pmm_init(ram_size_mb, (void*)0x500000); 
    write_serial_string("Physical Memory Manager Initialized.\n");
    // Oznaczanie wolnej pamięci
    write_serial_string("Marking free memory...\n");
    uint64_t safe_start = 0x800000; 
    pmm_mark_free(safe_start, 0xBFFDF000 - safe_start);
    if (total_ram_bytes > 0x100000000ULL) {
        pmm_mark_free(0x100000000ULL, total_ram_bytes - 0x100000000ULL);
    }
    write_serial_string("Free memory marked.\n");
   //wypisz ramki wolne i zajęte (podaj do czego są używane jeśli są zajęte)
    //wypisz w formie paska w ramach ascii art, gdzie # to zajęte, a - to wolne
    pmm_dump_memory_map();

    write_serial_string("Initializing Virtual Memory Manager...\n");
    uint64_t kernel_pml4 = vmm_create_kernel_pml4();
    g_kernel_cr3 = kernel_pml4;
    vmm_hhdm_ready = true; 


    write_serial_string("Kernel PML4 created at physical address: ");
    write_serial_hex(kernel_pml4);
    write_serial_string("\n");
    write_serial_string("\n");
    write_serial_string("Stack Bottom Phys: ");
    write_serial_hex((uint64_t)&stack_bottom);
    write_serial_string("\n");
    write_serial_string("\nStack Top Phys: ");
    write_serial_hex((uint64_t)&stack_top);
    write_serial_string("\n");
    set_cr3(kernel_pml4); 
    write_serial_string("Virtual Memory Manager Initialized.\n");

    heap_init((void*)(0xFFFF800000000000ULL + 0x10000000), 128 * 1024 * 1024);
    write_serial_string("Kernel Heap Initialized.\n");
    gdt_init();
    write_serial_string("[BOOT] GDT Initialized.\n");
    idt_init();
    write_serial_string("[BOOT] IDT Initialized.\n");
    keyboard_init();
    write_serial_string("[BOOT] Keyboard Initialized.\n");
    mouse_init();
    write_serial_string("[BOOT] Mouse Initialized.\n");
    syscall_init();
    write_serial_string("[BOOT] Syscall Interface Initialized.\n");
    evdev_init();
    drm_init();
    enable_sse();
    write_serial_string("[BOOT] SSE Enabled.\n");
    
    vfs_init();
    write_serial_string("[BOOT] Virtual File System Initialized.\n");
    pci_init(); 
    write_serial_string("[BOOT] PCI Bus Scanned.\n");

    if (sata_port) {
        if (ext2_init(sata_port)) {
            write_serial_string("[BOOT] EXT2 File System Mounted.\n");
        }
    }

    graphics_init_double_buffer();
    main_desktop = new Desktop();
    main_desktop->Init();
    main_desktop->AddWindow(new TerminalWindow(100, 100));

    scheduler_init_kernel_task();
    write_serial_string("[BOOT] Kernel Task Initialized.\n");
    write_serial_string("[BOOT] Total RAM Detected: ");
    write_serial_dec(ram_size_mb);
    write_serial_string(" MB\n");
    write_serial_string("[BOOT] Entering main loop...\n");
    kernel_task = current_task;

    write_serial_string("Switching stack to: ");
    uint64_t new_stack_top = (uint64_t)stack_top;
    write_serial_hex((uint64_t)new_stack_top);
    write_serial_string(" Jumping to: ");
    write_serial_hex((uint64_t)kmain_post_stack_switch);

    write_serial_string("Current task ptr: ");
    write_serial_hex((uint64_t)current_task);
    write_serial_string("\nValue of kstack_top: ");
    write_serial_hex(current_task->kstack_top);

    switch_to_kernel_stack((void*)current_task->kstack_top, kmain_post_stack_switch);
    
    while(1) { asm volatile("hlt"); }
}