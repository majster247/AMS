// Będziemy używać Twojej nowej LibC!
#include <stdio.h>

int main() {
    printf("POZDROWIENIA Z KODU SKOMPILOWANEGO PRZEZ TCC NA AMS-1!\n");
    printf("Liczba pi to mniej wiecej: %d (bo nie mamy floatow jeszcze)\n", 3);
    
    FILE* f = fopen("wynik.txt", "w");
    if(f) {
        fprintf(f, "TCC tu byl i to zapisal!");
        fclose(f);
    }
    
    return 0;
}