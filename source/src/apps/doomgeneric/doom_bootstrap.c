// Minimal bootstrap compiled by guest TCC.
// Linked with AMS crt0.o + ams_syscall.o, so no inline asm is needed here.

extern int sys_exec(const char* path, int argc, char** argv);

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char* pass_argv[2];
    pass_argv[0] = (char*)"/programs/doom/doom.base.elf";
    pass_argv[1] = 0;
    return sys_exec("/programs/doom/doom.base.elf", 1, pass_argv);
}
