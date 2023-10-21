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
        }
    }
}
    
#endif