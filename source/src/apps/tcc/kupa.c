// Prosty program testowy
void _start() {
    __asm__ volatile (
        "mov $60, %%rax\n"   // SYS_EXIT
        "mov $42, %%rdi\n"   // exit code 42
        "syscall\n"
        :
        :
        : "rax", "rdi"
    );
}