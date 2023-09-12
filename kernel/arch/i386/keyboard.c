#include <kernel/keyboard.h>
#include <kernel/io.h>// Include the I/O functions

static int keyboard_initialized = 0;

// Initialize the keyboard
void init_keyboard() {
    // You can add initialization code here if needed.
    // For a basic example, we'll assume no special initialization is required.
    keyboard_initialized = 1;
}

// Check if a key is pressed (non-blocking)
int is_key_pressed() {
    if (!keyboard_initialized) {
        // Keyboard not initialized, return an error code or handle it as needed.
        return -1;
    }
    
    // Read the status register (port 0x64)
    uint8_t status = inb(0x64);
    
    // Check bit 0 (output buffer status)
    return (status & 0x01);
}

// Read a key from the keyboard (blocking)
uint8_t read_key() {
    while (!is_key_pressed()) {
        // Wait until a key is pressed
    }
    
    // Read and return the key code
    return inb(KEYBOARD_DATA_PORT);
}

char key_to_ascii(uint8_t key) {
    if (key < 0x80) {
        switch(key) {
            case 0x31: return "1"; break;
            case 0x27: return "2"; break;
            case 0x28: return "3"; break;
            case 0x29: return "4"; break;
            case 0x2A: return "5"; break;
            case 0x2B: return "6"; break;
            case 0x2c: return "7"; break;
            case 0x2D: return "8"; break;
            case 0x2E: return "9"; break;
            case 0x25: return "0"; break;
            case 0x10: return "q"; break;
            case 0x11: return "w"; break;
            case 0x12: return "e"; break;
            case 0x13: return "r"; break;
            case 0x14: return "t"; break;
            case 0x05: return "z"; break;
            case 0x16: return "u"; break;
            case 0x17: return "i"; break;
            case 0x18: return "o"; break;
            case 0x19: return "p"; break;
            case 0x61: return "a"; break;
            case 0x73: return "s"; break;
            case 0x22: return "d"; break;
            case 0x2F: return "f"; break;
            case 0x23: return "g"; break;
            case 0x00: return "h"; break;
            case 0x44: return "j"; break;
            case 0x0B: return "k"; break;
            case 0x6C: return "l"; break;
            case 0x20: return "y"; break;
            case 0x21: return "x"; break;
            case 0x49: return "c"; break;
            case 0x45: return "v"; break;
            case 0x37: return "b"; break;
            case 0x36: return "n"; break;
            case 0x32: return "m"; break;
            case 0x33: return ","; break;
            case 0x34: return "."; break;
            case 0x35: return "-"; break;

            case 0x1e: return "\n"; break;
            case 0x50: return " "; break;
            default: return 0; // Unhandled key
        }
    } else {
        return 0; // Key release event (ignore)
    }
}
