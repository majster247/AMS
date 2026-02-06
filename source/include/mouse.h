// include/drivers/mouse.h
#pragma once
#include <stdint.h>

extern int32_t mouse_x;
extern int32_t mouse_y;
extern int32_t old_mouse_x;
extern int32_t old_mouse_y;
extern uint8_t mouse_cycle;
extern uint8_t mouse_byte[3];
extern uint32_t mouse_back_buffer[16 * 16];

extern int32_t old_x;
extern int32_t old_y;

extern volatile bool mouse_left_pressed;
extern volatile bool mouse_right_pressed;
extern volatile bool mouse_moved;

void mouse_init();
void update_mouse_on_screen();
extern "C" void mouse_handler(struct regs *r);
extern "C" void mouse_write(uint8_t data);
extern "C" uint8_t mouse_read();
extern "C" void save_background(int x, int y);
extern "C" void restore_background(int x, int y);
extern "C" void draw_cursor_shape(int x, int y);
void mouse_erase();
void mouse_draw();
