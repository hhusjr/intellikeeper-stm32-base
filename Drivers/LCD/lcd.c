//
// Created by 沈俊儒 on 2020/6/24.
//

#include <spi.h>
#include "lcd.h"
#include "font.h"

static uint8_t LCD_Buf[LCD_BUF_SIZE];

static void LCD_Write_Cmd(uint8_t cmd) {
    LCD_DC(0);
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 0xFFFF);
}

static void LCD_Write_Data(uint8_t cmd) {
    LCD_DC(1);
    HAL_SPI_Transmit(&hspi2, &cmd, 1, 0xFFFF);
}

static void LCD_Write_Color(uint16_t color) {
    LCD_DC(1);
    HAL_SPI_Transmit(&hspi2, (uint8_t*) (&color), 2, 0xFFFF);
}

static void LCD_Addr_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    LCD_Write_Cmd(0x2a);
    LCD_Write_Data(x1 >> 8u);
    LCD_Write_Data(x1);
    LCD_Write_Data(x2 >> 8u);
    LCD_Write_Data(x2);

    LCD_Write_Cmd(0x2b);
    LCD_Write_Data(y1 >> 8u);
    LCD_Write_Data(y1);
    LCD_Write_Data(y2 >> 8u);
    LCD_Write_Data(y2);

    LCD_Write_Cmd(0x2C);
}

void LCD_Initialize(void) {
    LCD_PWR(0);
    LCD_RST(0);
    HAL_Delay(12);
    LCD_RST(1);

    HAL_Delay(12);

    /* Sleep Out */
    LCD_Write_Cmd(0x11);

    /* wait for power stability */
    HAL_Delay(12);

    /* Memory Data Access Control */
    LCD_Write_Cmd(0x36);
    LCD_Write_Data(0x00);

    /* RGB 5-6-5-bit  */
    LCD_Write_Cmd(0x3A);
    LCD_Write_Data(0x65);

    /* Porch Setting */
    LCD_Write_Cmd(0xB2);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x00);
    LCD_Write_Data(0x33);
    LCD_Write_Data(0x33);

    /*  Gate Control */
    LCD_Write_Cmd(0xB7);
    LCD_Write_Data(0x72);

    /* VCOM Setting */
    LCD_Write_Cmd(0xBB);
    LCD_Write_Data(0x3D);   //Vcom=1.625V

    /* LCM Control */
    LCD_Write_Cmd(0xC0);
    LCD_Write_Data(0x2C);

    /* VDV and VRH Command Enable */
    LCD_Write_Cmd(0xC2);
    LCD_Write_Data(0x01);

    /* VRH Set */
    LCD_Write_Cmd(0xC3);
    LCD_Write_Data(0x19);

    /* VDV Set */
    LCD_Write_Cmd(0xC4);
    LCD_Write_Data(0x20);

    /* Frame Rate Control in Normal Mode */
    LCD_Write_Cmd(0xC6);
    LCD_Write_Data(0x0F);	//60MHZ

    /* Power Control 1 */
    LCD_Write_Cmd(0xD0);
    LCD_Write_Data(0xA4);
    LCD_Write_Data(0xA1);

    /* Positive Voltage Gamma Control */
    LCD_Write_Cmd(0xE0);
    LCD_Write_Data(0xD0);
    LCD_Write_Data(0x04);
    LCD_Write_Data(0x0D);
    LCD_Write_Data(0x11);
    LCD_Write_Data(0x13);
    LCD_Write_Data(0x2B);
    LCD_Write_Data(0x3F);
    LCD_Write_Data(0x54);
    LCD_Write_Data(0x4C);
    LCD_Write_Data(0x18);
    LCD_Write_Data(0x0D);
    LCD_Write_Data(0x0B);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x23);

    /* Negative Voltage Gamma Control */
    LCD_Write_Cmd(0xE1);
    LCD_Write_Data(0xD0);
    LCD_Write_Data(0x04);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x11);
    LCD_Write_Data(0x13);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x3F);
    LCD_Write_Data(0x44);
    LCD_Write_Data(0x51);
    LCD_Write_Data(0x2F);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x20);
    LCD_Write_Data(0x23);

    /* Display Inversion On */
    LCD_Write_Cmd(0x21);
    LCD_Write_Cmd(0x29);
    LCD_Addr_Set(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    /* Display on */
    LCD_PWR(1);
}

void LCD_Enable(void) {
    LCD_PWR(1);
}

void LCD_Disable(void) {
    LCD_PWR(0);
}

void LCD_Clear(uint16_t color) {
    LCD_Addr_Set(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    for (uint16_t i = 0; i * 2 < LCD_BUF_SIZE; i++) {
        *((uint16_t*) (LCD_Buf + i * 2)) = color;
    }

    LCD_DC(1);
    for (uint16_t i = 0; i * LCD_BUF_SIZE < LCD_TOTAL_BUF_SIZE; i++) {
        HAL_SPI_Transmit(&hspi2, LCD_Buf, LCD_BUF_SIZE, 0xFFFF);
    }
}

void LCD_Draw_Terminal_Char(uint16_t x, uint16_t y, char chr, uint16_t FrontColor, uint16_t BackColor) {
    if (x > LCD_WIDTH - 8 || y > LCD_HEIGHT - 16) {
        return;
    }

    LCD_Addr_Set(x, y, x + 15, y + 31);

    chr -= ' ';
    if (chr >= 95 || chr < 0) {
        chr = '?' - ' ';
    }

    for (uint8_t t = 0; t < 64; t++) {
        uint8_t tmp = asc2_3216[chr][t];
        for (uint8_t t1 = 0; t1 < 8; t1++) {
            LCD_Write_Color(tmp & 0x80u ? FrontColor : BackColor);
            tmp <<= 1u;
        }
    }
}
