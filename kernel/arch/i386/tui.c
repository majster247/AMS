#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <kernel/tui.h>
#include <kernel/tty.h>
//#include <kernel/keyboard.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;


int checked=0;

void createTUI_windowFullscreen(const int height, const int width) {
    if (height <= 0 || width <= 0 || height > 25 || width > 80) {
        terminal_writestring("Invalid window dimensions.\n");
        return;
    }
    terminal_writestring("\n\n");
    for (int j = 2; j < height; j++) {
        for (int i = 2; i < width; i++) {
            if(((j==height-1) && (i==width-1))){printf("%c", ((char)219));terminal_writestring("\n");}else{
                if(((j == 2) && (i==2))){printf("  %c", ((char)219));}else{   
                    if(((j == 2) && (i==width-1))){printf("%c", ((char)219));terminal_writestring("\n");}else{ 
                        if (j == 2) {printf("%c", ((char)223));} else {   
                            if(((j==height-1) && (i==2))){printf("  %c", ((char)219));}else{    
                                if(j == height-1){printf("%c", ((char)220));}else{
                                    if (i == 2) {printf("  %c", ((char)221));} else {
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

    //terminal_writestring("Hello\n");
    //terminal_writestring("AMS-OS!\n\n\n");
    char info[] = "AMS-OS v0.1      developed by Hubert Topolski";
    char start[]="Run programs";
    char Terminal[]="Terminal";
    char About[]="About OS";
    
    for(int z=4;z<=((int)strlen(info)+4);z++){
        terminal_putentryat(info[z-4], TerminalColor(), z, 3);
    }

    switch (checked)
    {
    case 0:{
        for(int x=4;x<=((int)strlen(start)+4);x++){
            terminal_putentryat(start[x-4], vga_entry_color(0, 15), x, 6);
        }
    
        for(int y=4;y<=((int)strlen(Terminal)+4);y++){
            terminal_putentryat(Terminal[y-4], vga_entry_color(0, 7), y, 8);
        }

        for(int t=4;t<=((int)strlen(About)+4);t++){
            terminal_putentryat(About[t-4], vga_entry_color(0, 7), t, 10);
        }}
    case 1:{
        for(int x=4;x<=((int)strlen(start)+4);x++){
            terminal_putentryat(start[x-4], vga_entry_color(0, 7), x, 6);
        }
    
        for(int y=4;y<=((int)strlen(Terminal)+4);y++){
            terminal_putentryat(Terminal[y-4], vga_entry_color(0, 15), y, 8);
        }

        for(int t=4;t<=((int)strlen(About)+4);t++){
            terminal_putentryat(About[t-4], vga_entry_color(0, 7), t, 10);
    }}
    case 2:{
        for(int x=4;x<=((int)strlen(start)+4);x++){
            terminal_putentryat(start[x-4], vga_entry_color(0, 7), x, 6);
        }
    
        for(int y=4;y<=((int)strlen(Terminal)+4);y++){
            terminal_putentryat(Terminal[y-4], vga_entry_color(0, 7), y, 8);
        }

        for(int t=4;t<=((int)strlen(About)+4);t++){
            terminal_putentryat(About[t-4], vga_entry_color(0, 15), t, 10);
        }}
    default:{
        for(int x=4;x<=((int)strlen(start)+4);x++){
            terminal_putentryat(start[x-4], vga_entry_color(0, 15), x, 6);
        }
    
        for(int y=4;y<=((int)strlen(Terminal)+4);y++){
            terminal_putentryat(Terminal[y-4], vga_entry_color(0, 7), y, 8);
        }

        for(int t=4;t<=((int)strlen(About)+4);t++){
            terminal_putentryat(About[t-4], vga_entry_color(0, 7), t, 10);
        }}
    }
    while(1!=0){
        /*if(keyboard_is_pressed()){
            terminal_writestring(keyboard_read_char());
        }*/
    }
}



