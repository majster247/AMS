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
int universalfunc::strcmp(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

char* universalfunc::int2str(uint32_t num) {
        uint32_t numChar = 1;
        uint8_t i = 1;
        if (num % 10 != num) {
                while ((num / (numChar)) >= 10) {
                        numChar *= 10;
                        i++;
                }
                char* str = "4294967296";
                uint8_t strIndex = 0;
                while (i) {
                        str[strIndex] = (char)(((num / (numChar)) % 10) + 48);
                        if (numChar >= 10) {
                                numChar /= 10;
                        }
                        strIndex++;
                        i--;
                }
                str[strIndex] = '\0';
                return str;
        }
        char* str = " ";
        str[0] = (num + 48);
        return str;
}

//TODO: RADOM NUMBER OVER CTIME
static uint32_t rand_seed = 42;  // Initial seed value

// Custom random number generator
uint32_t customRand() {
    // Linear congruential generator parameters
    const uint32_t a = 1664525;
    const uint32_t c = 1013904223;
    
    rand_seed = a * rand_seed + c;
    return rand_seed;
}

// Function to generate a random number within a specified range
uint32_t universalfunc::getRandomNumber(uint32_t min, uint32_t max) {
    // Ensure the range is valid
    if (min >= max) {
        return min;
    }

    // Generate a random number within the specified range
    uint32_t randomNum = customRand() % (max - min) + min;
    return randomNum;
}