#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <kernel/tty.h>
#include <kernel/tui.h>
#include <kernel/keyboard.h>
#include <kernel/io.h>

void kernel_main(void) {
    terminal_initialize();
    //terminal_writestring("AMS test unit for v0.1 - Author: Hubert Topolski\n\n");
    createTUI_windowFullscreen(15, 52);
    terminal_writestringat("AMS-OS v0.1  created by Hubert Topolski\n\n", 3, 3);
    terminal_writestringat("Jebac Slask i Invil", 5, 3);


    // Initialize the keyboard
    /*init_keyboard();

   while (1) {
    if (is_key_pressed()) {
        uint8_t key = read_key();
        if (key & 0x80) {
            // Key release event, ignore it
        } else {
            char key_char = key_to_ascii(key);
            if (key_char != 0) {
                if(key_char != '\b'){
                    printf(key_to_ascii(key));
                }
            }
        }
    }*/
}

