//
// Created by 沈俊儒 on 2020/6/24.
//
#include "stm32l4xx_hal.h"

#ifndef LCD_H
#define LCD_H

#define LCD_BUF_SIZE 1152

#define LCD_WIDTH 240
#define LCD_HEIGHT 240

#define LCD_TOTAL_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT * 2)

#define LCD_PWR(s) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, s)
#define LCD_RST(s) HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, s)
#define LCD_DC(s) HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, s)

#define WHITE 0xFFFF

void LCD_Clear(uint16_t color);
void LCD_Enable(void);
void LCD_Disable(void);
void LCD_Initialize(void);
void LCD_Draw_Terminal_Char(uint16_t x, uint16_t y, char chr, uint16_t FrontColor, uint16_t BackColor);
void LCD_Erase_Terminal_Char(uint16_t x, uint16_t y, uint16_t BackColor);

#endif //LCD_H
