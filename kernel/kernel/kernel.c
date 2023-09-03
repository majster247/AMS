#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <kernel/tty.h>
#include <kernel/tui.h>



void kernel_main(void) {
	terminal_initialize();
    createTUI_windowFullscreen(23,78);

    while(1!=0){
        
    }
}
