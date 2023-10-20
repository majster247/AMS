#include <common/programs/terminal.h>
#include <common/universalfunc.h>
#include <common/types.h>
using namespace myos;
using namespace myos::common;
using namespace myos::common::programs;

void printf(char* str);

void Terminal::Initialize(){
    printf("[DevMode]A:~:/>");
}

void Terminal::HandleCommand(char* command){
    
    uint16_t lenght=universalfunc::strlen(command);

    for(uint16_t i=0;i<=lenght;i++)
        if(command[i]!=' ' || command[i]!=0){
            if(command[i]=='x'){printf("Burn all CIA fucking niggers\n");}
        }
    printf("[DevMode]A:~:/>");
}
