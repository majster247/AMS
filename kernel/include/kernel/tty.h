#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <kernel/types.h>

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
int chrASCII(char character);
char ASCIIchr(int code);

int TerminalColor();

#endif
