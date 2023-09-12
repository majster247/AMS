#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <kernel/tty.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
	unsigned char uc = c;
	terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_row = 0;
	}
}

void terminal_write(const char* data, size_t size) {
    size_t i = 0;
    while (i < size) {
        if (data[i] == '\n') {
            terminal_row += 1;
            terminal_column = 0;
            i++;
        } else {
            // Regular character, print it
            terminal_putchar(data[i]);
            i++;
        }
    }
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

// Function to print a string at a specific row and column
void terminal_writestringat(const char* data, size_t row, size_t column) {
    // Check if the row and column are within bounds
    if (row >= VGA_HEIGHT || column >= VGA_WIDTH) {
        // Handle out-of-bounds row or column
        return;
    }

    // Save the current cursor position
    size_t original_row = terminal_row;
    size_t original_column = terminal_column;

    // Set the cursor position to the desired row and column
    terminal_row = row;
    terminal_column = column;

    // Write the string at the specified row and column
    terminal_writestring(data);

    // Restore the original cursor position
    terminal_row = original_row;
    terminal_column = original_column;
}



void terminal_writestringatrow(const char* data, size_t row) {
    // Check if the row is within bounds
    if (row >= VGA_HEIGHT) {
        // Handle out-of-bounds row
        return;
    }

    // Save the current cursor position
    size_t original_row = terminal_row;
    size_t original_column = terminal_column;

    // Set the cursor position to the desired row
    terminal_row = row;
    terminal_column = 0;

    // Write the string at the specified row
    terminal_writestring(data);

    // Restore the original cursor position
    terminal_row = original_row;
    terminal_column = original_column;
}


//Conversions

int chrASCII(char character) {
    return (int)character;
}

char ASCIIchr(int code) {
    return (char)code;
}

//give values
int TerminalColor(){
	return terminal_color;
}