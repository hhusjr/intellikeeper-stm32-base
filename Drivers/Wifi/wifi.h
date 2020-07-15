//
// Created by 沈俊儒 on 2020/6/25.
//

#ifndef WIFI_H
#define WIFI_H

#include "stm32l4xx_hal.h"

#define WIFI_BUF_SIZE 1024

#define WIFI_MODE_STATION 1
#define WIFI_MODE_AP      2
#define WIFI_MODE_MIXED   3

#define WIFI_ENCRYPT_OPEN         0
#define WIFI_ENCRYPT_WEP          1
#define WIFI_ENCRYPT_WPA_PSK      2
#define WIFI_ENCRYPT_WPA2_PSK     3
#define WIFI_ENCRYPT_WPA_WPA2_PSK 4

#define WIFI_RESET() Wifi_AT_Async("AT+RST\r\n")
#define WIFI_SAY_HELLO() Wifi_AT_Async("AT\r\n")
#define WIFI_CHANGE_MODE(mode) Wifi_AT_Async("AT+CWMODE_DEF=%d\r\n", mode)
#define WIFI_AP_CONFIG(ssid, pwd, channel, ecn) Wifi_AT_Async("AT+CWSAP=\"%s\",\"%s\",%d,%d\r\n", ssid, pwd, channel, ecn)
#define WIFI_GET_CONNECTED_DEVICE_IPS() AT_Write("AT+CWLIF\r\n")
#define WIFI_GET_CURRENT_DEVICE_IP() AT_Write("AT+CIFSR\r\n")
#define WIFI_CREATE_SERVER(port) Wifi_AT_Async("AT+CIPSERVER=1,%d\r\n", port)
#define WIFI_SHUTDOWN_SERVER(p) Wifi_AT_Async("AT+CIPSERVER=0\r\n")
#define WIFI_ALLOW_MULTI_CONN() Wifi_AT_Async("AT+CIPMUX=1\r\n")
#define WIFI_REJECT_MULTI_CONN() Wifi_AT_Async("AT+CIPMUX=0\r\n")
#define WIFI_GET_CONNECTIONS() Wifi_AT_Async("AT+CIPSTATUS\r\n")
#define WIFI_CONNECT(protocol, ip, port) Wifi_AT_Async("AT+CIPSTART=\"%s\",\"%s\",%d\r\n", protocol, ip, port)
#define WIFI_SET_CIP() Wifi_AT_Async("AT+CIPMODE=1\r\n")
#define WIFI_SEND(id, len) Wifi_AT_Async("AT+CIPSEND=%d,%d\r\n", id, len)
#define WIFI_ENTER_CIP() Wifi_AT_Async("AT+CIPSEND\r\n")
#define WIFI_CONTENT_SEND(content) Wifi_AT_Async("%s", content)
#define WIFI_CLOSE_CIP(id) Wifi_AT_Async("AT+CIPCLOSE=%d\r\n", id)
#define WIFI_CWLAP() Wifi_AT_Async("AT+CWLAP\r\n")
#define WIFI_CONNECT_TO_AP(ssid, password) Wifi_AT_Async("AT+CWJAP_DEF=\"%s\",\"%s\"\r\n", ssid, password)
#define WIFI_GET_CONNECT_INFO() Wifi_AT_Async("AT+CWJAP?\r\n")
#define WIFI_RESTORE() Wifi_AT_Async("AT+RESTORE\r\n")

void Wifi_UART_IRQ(void);
void Wifi_Receive_Time(void);
void Wifi_Start();
void Wifi_TCP_Connect(const char* host, uint16_t port);
void Wifi_TCP_Send(uint8_t* buffer, uint16_t len);
uint16_t Wifi_TCP_Receive(uint8_t* buffer, uint16_t len, uint16_t timeout);

#endif //WIFI_H
