#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <kernel/tty.h>
#include <kernel/tui.h>

void createTUI_windowFullscreen(const int height, const int width) {
    if (height <= 0 || width <= 0 || height > 25 || width > 80) {
        terminal_writestring("Invalid window dimensions.\n");
        return;
    }

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if(((j==height-1) && (i==width-1))){printf("%c", ((char)219));terminal_writestring("\n");}else{
                if(((j == 0) && (i==0))){printf("%c", ((char)219));}else{   
                    if(((j == 0) && (i==width-1))){printf("%c", ((char)219));terminal_writestring("\n");}else{ 
                        if (j == 0) {printf("%c", ((char)223));} else {   
                            if(((j==height-1) && (i==0))){printf("%c", ((char)219));}else{    
                                if(j == height-1){printf("%c", ((char)220));}else{
                                    if (i == 0) {printf("%c", ((char)221));} else {
                                        if((i == width-1)){printf("%c", ((char)222));terminal_writestring("\n");}else{
                                            terminal_writestring(" ");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    terminal_writestring("\n");
}


void kernel_main(void) {
	terminal_initialize();
	createTUI_windowFullscreen(24,79);
    //terminal_writestring("Hello\n");
    //terminal_writestring("AMS-OS!\n\n\n");
    char info[] = "AMS-OS v0.1      developed by Hubert Topolski";
    for(int z=1;z<=(strlen(info));z++){
        terminal_putentryat(info[z-1], TerminalColor(), z, 1);
    }
    
    
}
