#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <kernel/tui.h>
#include <kernel/tty.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

void createTUI_window(const int height, const int width) {
    // Check if height and width are within valid bounds
    if (height <= 0 || width <= 0 || height > VGA_HEIGHT || width > VGA_WIDTH) {
        printf("Invalid window dimensions.\n");
        return;
    }

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (j == 0) {printf("-");} else {
                if(j == 0 && i==width-1){printf("-\n");}else{    
                    if(j == height - 1){printf("-");}else{
                        if (i == 0) {printf("|");} else {
                            if(i == width - 1){printf("|\n");}else{
                                printf(" ");
                            }
                        }
                    }
                }
            }
        
        }
        printf("\n");
    }
}


