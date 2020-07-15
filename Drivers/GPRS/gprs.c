//
// Created by 沈俊儒 on 2020/6/25.
//

#include "gprs.h"
#include "../LCD/terminal.h"
#include "../StreamIO/stream_io.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <usart.h>

#define OUTPUT(w)         Lcd_Printf("GS: "); Lcd_Printf(w);                Lcd_Printf("\n")
#define MESSAGE(fmt, ...) Lcd_Printf("GS: "); Lcd_Printf(fmt, __VA_ARGS__); Lcd_Printf("\n")

static char GPRS_Receive_Buf[GPRS_BUF_SIZE], GPRS_Send_Buf[GPRS_BUF_SIZE];

static void GPRS_Error_Handler() {
    OUTPUT("GPRS Hang...");
    while (1){
        asm("NOP");
    }
}

static uint8_t Expect_contains(const char* expect) {
    return strstr(GPRS_Receive_Buf, expect) != NULL;
}

static uint8_t Expect_OK() {
    return Expect_contains("OK");
}

static void AT_Write(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(GPRS_Send_Buf, GPRS_BUF_SIZE, fmt, args);
    va_end(args);

    memset(GPRS_Receive_Buf, 0, sizeof(GPRS_Receive_Buf));

    HAL_UART_Transmit(&hlpuart1, (uint8_t*) GPRS_Send_Buf, strlen(GPRS_Send_Buf), 0x100);
    __HAL_UART_CLEAR_IT(&hlpuart1, UART_CLEAR_NEF | UART_CLEAR_OREF);
    HAL_UART_Receive(&hlpuart1, (uint8_t*) GPRS_Receive_Buf, GPRS_BUF_SIZE, 0x100);
}

void GPRS_Connect() {
    OUTPUT("GPRS Initializing...");
    HAL_Delay(5000);

    while (1) {
        HAL_Delay(500);
        GPRS_QUERY_PIN();
        if (!Expect_OK()) {
            Lcd_Printf(GPRS_Receive_Buf);
            OUTPUT("GPRS Connect failed, PIN not ready.");
            continue;
        }
        OUTPUT("PIN Ready");
        break;
    }

    while (1) {
        HAL_Delay(500);
        GPRS_QUERY_GSM();
        if (!Expect_OK()) {
            OUTPUT("GPRS Query GSM failed.");
            continue;
        }
        if (Expect_contains("0,1")) {
            OUTPUT("GSM in local network!");
        } else if (Expect_contains("0,5")) {
            OUTPUT("GSM in roam network!");
        } else {
            OUTPUT("GSM network type unknown");
            OUTPUT(GPRS_Receive_Buf);
        }
    }
}
