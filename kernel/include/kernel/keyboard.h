#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Function to initialize the keyboard
void keyboard_init();

// Function to check if a key is pressed
int keyboard_is_pressed();

// Function to read the key code and return the character
char keyboard_read_char();

#endif // KEYBOARD_H
