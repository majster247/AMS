#include <common/programs/terminal.h>
#include <common/universalfunc.h>
#include <common/types.h>
#include <drivers/keyboard.h>
#include <filesystem/shadowwizard.h>
#include <common/programs/editor.h>
using namespace myos;
using namespace myos::common;
using namespace myos::common::programs;

void printf(char* str);

void Terminal::Initialize(){
    myos::common::programs::Terminal::status=1;
    printf("[DevMode]A:~:/>");

};

void myos::common::programs::Terminal::HandleCommand(char *command)
{

    if(universalfunc::strcmp(command, "sex") == 0){
        printf("More sex in future\n");
        printf("[DevMode]A:~:/>");
    }else if (universalfunc::strcmp(command, "HolyWords") == 0)
    {
        printf("Holy Words for you are...\n\n");
        switch (universalfunc::getRandomNumber(0, 12))
        {
            case(1):{printf("I said 'I will rebuild this temple in three days.' I could make a compiler in 3 days.\n");break;}
            case(2):{printf("India nigger retard doctor.\n");break;}
            case(3):{printf("Yeah, I killed a CIA nigger with my car in 1999. Score one for the good guys.\n");break;}
            case(4):{printf("If you guys have ideas for things to do, let me know. I probably will ignore them.\n");break;};
            case(5):{printf("I have God's official endorsement. I win and the CIA/FBI niggers lose. Just wait. Dumb fuck FBI niggers.\n");break;};
            case(6):{printf("I report to God. You report to me.\n");break;};
            case(7):{printf("I like big butts and I cannot lie.\n");break;};
            case(8):{printf("Computers went to shit when they started making them for niggers.\n");break;};
            case(9):{printf("God's world is perfectly just. Only a nigger cannot see. That's why niggers have no morals.\n");break;};
            case(10):{printf("CIA niggers glow in the dark. You can see them if you're driving. You just run them over. That's what you do.\n");break;};
            case(11):{printf("Maybe I'm just a bizarre little person who walks back and forth.\n");break;};
        }
        printf("\n");
        printf("[DevMode]A:~:/>");
    }else if(universalfunc::strcmp(command, "testRandr") == 0){
        for(int i=0;i<=36;i++){
            if(i%6==0){
                printf("\n");
            }
            printf(universalfunc::int2str(universalfunc::getRandomNumber(1,11)));
            printf("  ");
        }
    }else if (universalfunc::strcmp(command, "ls") == 0){
	    uint32_t fileNum = filesystem::FileList();
	    char* num = universalfunc::int2str(fileNum);
	
	    printf("\n");
	    printf(num);
	    printf(" files have been allocated.\n");
    }else if (universalfunc::strcmp(command, "ed") == 0)
    {
        fileTUI();
    
    }else{printf("[DevMode]A:~:/>");}
};
