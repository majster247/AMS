#include "shell.h"
#include "kernel.h"
#include "vfs.h"

// Odwołujemy się do zmiennych z keyboard.cpp
extern volatile char cmd_buffer[128];
extern volatile int cmd_index;
extern volatile bool line_ready;

void shell_prompt() {
    terminal_writestring("ams@os:/> ");
}

void shell_init() {
    terminal_writestring("\n========================\n");
    terminal_writestring("   AMS OS Shell v0.1    \n");
    terminal_writestring("========================\n");
    shell_prompt();
}

// Pomocnicza funkcja do kopiowania volatile -> zwykły char*
void copy_command(char* dest, volatile char* src) {
    int i = 0;
    while(src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void execute_command(char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("Komendy: help, ls, clear, cat [plik]\n");
    }
    else if (strcmp(cmd, "ls") == 0) {
        vfs_node* curr = vfs_root;
        while (curr) {
            terminal_writestring(curr->name);
            terminal_writestring("  ");
            curr = curr->next;
        }
        terminal_writestring("\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        terminal_initialize();
        shell_init();
        return; // shell_init rysuje prompt, więc wracamy
    }
    else if (cmd[0] != '\0') {
        terminal_writestring("Nieznana komenda.\n");
    }
}

void shell_update() {
    if (line_ready) {
        // Kopiujemy komendę do lokalnego bufora (zdejmujemy volatile)
        char local_cmd[128];
        copy_command(local_cmd, cmd_buffer);
        
        execute_command(local_cmd);
        
        // Reset
        cmd_index = 0;
        cmd_buffer[0] = '\0';
        line_ready = false;
        
        shell_prompt();
    }
}