#include <common/universalfunc.h>
#include <common/types.h>
using namespace myos;
using namespace myos::common;
     

uint16_t universalfunc::strlen(char* args){
    uint16_t length = 0;
    for (length = 0; args[length] != '\0'; length++) {}
    return length;
}

char* universalfunc::strcat(char* destination, const char* source){
    char* ptr = destination + strlen(destination);
    while (*source != '\0') {*ptr++ = *source++;}
    *ptr = '\0';
    return destination;
}