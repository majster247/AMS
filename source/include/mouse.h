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

extern bool mouse_left_pressed;
extern bool mouse_right_pressed;



void mouse_init();
void update_mouse_on_screen();
extern "C" void mouse_handler();
extern "C" void draw_cursor_shape(int32_t x, int32_t y);