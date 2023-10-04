#ifndef __MYOS__COMMON__PROGRAMS__TERMINAL_H
#define __MYOS__COMMON__PROGRAMS__TERMINAL_H

namespace myos{
    namespace common{
        namespace programs{

            class Terminal{
                public:
                    static char* command[100];

                    Terminal();
                    ~Terminal();
                 
            };
        }
    }
}

#endif