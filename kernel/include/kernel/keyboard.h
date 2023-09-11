#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"  // Include your custom types.h header

// Define keyboard-related constants
#define KEYBOARD_DATA_PORT 0x60

// Function to initialize the keyboard
void init_keyboard();

// Function to check if a key is pressed
int is_key_pressed();

// Function to read a key from the keyboard
uint8_t read_key();

char key_to_ascii(uint8_t key);

#endif
