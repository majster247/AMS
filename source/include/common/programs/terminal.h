#ifndef __MYOS__COMMON__PROGRAMS__TERMINAL_H
#define __MYOS__COMMON__PROGRAMS__TERMINAL_H

#include <common/types.h>

namespace myos{
    namespace common{
        namespace programs{

            namespace Terminal{
                static char command[100];
                static uint8_t status=0;
                void Initialize();
                void HandleCommand(char* command);
                 
            };
        }
    }
}

#endif