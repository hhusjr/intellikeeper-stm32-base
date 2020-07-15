//
// Created by 沈俊儒 on 2020/6/25.
//

#ifndef GPRS_H
#define GPRS_H

#include "stm32l4xx_hal.h"

#define GPRS_BUF_SIZE 1060

#define GPRS_QUERY_PIN() AT_Write("AT+CPIN?\r\n")
#define GPRS_SAY_HELLO() AT_Write("AT\r\n")
#define GPRS_QUERY_GSM() AT_Write("AT+CREG?\r\n")


void GPRS_Connect();

#endif //GPRS_H
