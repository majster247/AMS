#include <common/types.h>
#include <common/universalfunc.h>

using namespace myos;
using namespace myos::common;


uint16_t strlen(char* args){
    uint16_t length = 0;
    for (length = 0; args[length] != '\0'; length++) {}
    return length;
}