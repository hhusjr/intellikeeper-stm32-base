//
// Created by 沈俊儒 on 2020/6/24.
//
#include "stm32l4xx_hal.h"

#ifndef LCD_TERMINAL_H
#define LCD_TERMINAL_H

#define TERMINAL_BACK_COLOR 0x0000

#define CHAR_WIDTH 16
#define CHAR_HEIGHT 32

#define HORIZONTAL_MARGIN 5
#define VERTICAL_MARGIN 2

#define MAX_LINE_PER_SCREEN (LCD_HEIGHT / (CHAR_HEIGHT + VERTICAL_MARGIN))
#define LINE_BUF_SIZE (LCD_WIDTH / (CHAR_WIDTH + HORIZONTAL_MARGIN))
#define MAX_BUFFERED_LINE_CNT (MAX_LINE_PER_SCREEN * 10)

void Terminal_Initialize(void);
void Terminal_PutChar(char chr);
void Terminal_Write(const char* string);
void Terminal_ScrollUp();
void Terminal_ScrollDown();
void Terminal_Buf_Render();

#endif //LCD_TERMINAL_H
