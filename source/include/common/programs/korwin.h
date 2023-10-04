//Korwin app and package manager

#ifndef __MYOS__COMMON__PROGRAMS__KORWIN_H
#define __MYOS__COMMON__PROGRAMS__KORWIN_H

#include <common/programs/terminal.h>
#include <common/types.h>

namespace myos{
    namespace common{
        namespace programs{

            class korwin{
                    //some kind of protecion is needed for list of kernelprograms and aliases
                public:
                    KorwinDPKG();
                    ~KorwinDPKG();
                    static void AddProgram();
                    static void DeleteProgram();
                    static int32_t CountPrograms();
                    char KorwinHEAD[5] = {"KorwinDPKG", "Terminal", "colorscheme", "ASM-TUI", "ASM-nano"};
                    static int programNumber = 5;

            };
        }
    }
}

#endif