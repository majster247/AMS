void _start() {
    __asm__ volatile (
        "mov $60, %rax\n"
        "mov $69, %rdi\n"
        "syscall"
    );
}