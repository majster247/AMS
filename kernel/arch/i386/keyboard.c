#include <kernel/keyboard.h>
#include <stdint.h>
#include <stdbool.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_COMMAND_REG 0x64
#define KEYBOARD_DATA_REG 0x60
#define KEYBOARD_ENABLE_SCAN_CODE_SET_1 0xF4
#define KEYBOARD_ACKNOWLEDGE 0xFA

// Function to initialize the keyboard
void keyboard_init() {
    // Send a command to enable keyboard scanning
    outb(KEYBOARD_COMMAND_REG, KEYBOARD_ENABLE_SCAN_CODE_SET_1);

    // Wait for an acknowledgment from the keyboard controller
    uint8_t ack = inb(KEYBOARD_DATA_REG);
    while (ack != KEYBOARD_ACKNOWLEDGE) {
        ack = inb(KEYBOARD_DATA_REG);
    }
}

// Function to check if a key is pressed
int keyboard_is_pressed() {
    // Check if there is data in the keyboard buffer
    return inb(KEYBOARD_DATA_PORT) != 0;
}

// Function to read the key code and return the character
char keyboard_read_char() {
    // Wait for a key press
    while (!keyboard_is_pressed());

    // Read the key code from the keyboard buffer
    uint8_t key_code = inb(KEYBOARD_DATA_PORT);

    // You will need to implement a lookup table to map key codes to characters.
    // This example assumes a simple mapping for lowercase letters.
    // You should expand this to handle other keys and modifiers.
    char characters[] = "abcdefghijklmnopqrstuvwxyz";
    if (key_code >= 0x10 && key_code <= 0x19) {
        return characters[key_code - 0x10];
    }

    // Handle other key codes here...

    return '\0'; // Return null character if the key code is not recognized
}
