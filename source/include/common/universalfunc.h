#ifndef __MYOS__COMMON__UNIVERSALFUNC_H
#define __MYOS__COMMON__UNIVERSALFUNC_H

#include <common/types.h>

namespace myos
{
    namespace common
    {
        namespace universalfunc{
            uint16_t strlen(char* args);
            char* strcat(char* destination, const char* source);
            int strcmp(const char *str1, const char *str2);
            char* int2str(uint32_t num);
            uint32_t getRandomNumber(uint32_t min, uint32_t max);
        }
    }
}
    
#endif