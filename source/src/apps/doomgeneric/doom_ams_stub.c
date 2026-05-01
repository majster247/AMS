#include "stdio.h"
#include "unistd.h"
#include "string.h"

static void print_line(const char* s) {
    write(1, s, (int)strlen(s));
    write(1, "\n", 1);
}

int main(void) {
    print_line("=== AMS Doomgeneric Stub ===");
    print_line("Krok 3 aktywny: scaffold pod doomgeneric gotowy.");
    print_line("Nastepny krok:");
    print_line("1) wrzucic upstream doomgeneric.c/.h + doom sources");
    print_line("2) podpiac backend: src/apps/doomgeneric/doomgeneric_ams.c");
    print_line("3) odpalic /doom.elf z terminala");
    return 0;
}
