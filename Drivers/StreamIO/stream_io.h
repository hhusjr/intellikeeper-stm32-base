//
// Created by 沈俊儒 on 2020/6/25.
//

#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <stdio.h>

#define STREAMIO_BUF_SIZE 1024

void Serial_Printf(const char* fmt, ...);
void Lcd_Printf(const char* fmt, ...);

#endif //STREAM_IO_H
