#include <unistd.h>
#include "stdio.h"
#include "ams_syscall.h"

// Definicja fileno jeśli jej brakuje w stdio.h
#ifndef fileno
#define fileno(F) ((F)->fd)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

int main() {
    printf("Testowanie AMS LibC...\n");
    printf("Liczba: %d, Hex: %x, String: %s\n", 12345, 0xBEEF, "Dziala!");

    FILE* f = fopen("hello.txt", "r");
    if (f) {
        char buf[10];
        // Używamy Twojego read(fd, buf, count)
        int bytes = read(fileno(f), buf, 5);
        if (bytes > 0) {
            buf[bytes] = 0;
            printf("Poczatek: %s\n", buf);
        }

        lseek(fileno(f), 10, SEEK_SET);
        
        bytes = read(fileno(f), buf, 4);
        if (bytes > 0) {
            buf[bytes] = 0;
            printf("Srodek (po lseek): %s\n", buf);
        }

        fclose(f);
    } else {
        printf("Blad otwarcia hello.txt\n");
    }

    return 0;
}