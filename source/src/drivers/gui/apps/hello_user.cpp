extern "C" void _start() {
    const char* msg = "AMS-OS: Czesc z Ring 3!\n";

    // Syscall nr 1 (write_serial)
    asm volatile (
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "syscall\n"
        : : "r"(msg) : "rax", "rdi"
    );

    // Pętla nieskończona (nie używaj hlt!)
    while (1) {
        asm volatile("pause"); // Instrukcja 'pause' jest dozwolona w Ring 3
    }
}