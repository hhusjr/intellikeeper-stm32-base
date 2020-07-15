//
// Created by 沈俊儒 on 2020/6/25.
//

#include <usart.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "stream_io.h"
#include "../LCD/terminal.h"

char StreamIO_Buf[STREAMIO_BUF_SIZE];

void Serial_Printf(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(StreamIO_Buf, STREAMIO_BUF_SIZE, fmt, args);
    va_end(args);

    size_t len = strlen(StreamIO_Buf);
    HAL_UART_Transmit(&huart1, (uint8_t*) StreamIO_Buf, len, 0xFFFF);
}

void Lcd_Printf(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(StreamIO_Buf, STREAMIO_BUF_SIZE, fmt, args);
    va_end(args);

    Terminal_Write(StreamIO_Buf);
}
