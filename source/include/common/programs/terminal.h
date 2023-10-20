#ifndef __MYOS__COMMON__PROGRAMS__TERMINAL_H
#define __MYOS__COMMON__PROGRAMS__TERMINAL_H

namespace myos{
    namespace common{
        namespace programs{

            namespace Terminal{
                static char command[100];
                void Initialize();
                void HandleCommand(char* command);
                 
            };
        }
    }
}

#endif