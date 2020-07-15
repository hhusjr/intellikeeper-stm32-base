//
// Created by 沈俊儒 on 2020/6/27.
//

#ifndef ZIGBEE_H
#define ZIGBEE_H

#define TX_BUFFER_SIZE 3000
#define RX_BUFFER_SIZE 3000

#define COORDINATOR 1
#define ROUTER      2
#define END_DEVICE  3

#define TRANS_TRANSPARENT     1
#define TRANS_TRANSPARENT_ADR 3
#define TRANS_TRANSPARENT_MAC 4
#define TRANS_N2N             5

#define BAUD_115200 0x08

#define AERIAL_INTERNAL 0
#define AERIAL_EXTERNAL 1

#define MAX_ALLOWED_TAG_N 300
#define MAX_ALLOWED_READER_N 100
extern uint16_t Tag_Address_Mapping[MAX_ALLOWED_TAG_N][11];
extern uint16_t Reader_Address_Mapping[MAX_ALLOWED_READER_N][3];

struct Config {
    uint8_t connected;
    double version;

    uint8_t type;
    uint8_t channel;
    uint8_t Transport_Mode;
    uint16_t address;
    uint16_t Pan_ID;

    uint8_t Baud_Rate;
    uint8_t Aerial_Type;
};

extern struct Config Zigbee_Config;

void Zigbee_Connect();
void Zigbee_Read_Config();
void Zigbee_Print_Config();
uint8_t Zigbee_Write_Config();
void Zigbee_ConfigureAs(uint8_t type);
void Zigbee_Listen(void);
void Zigbee_UART_IRQ(void);
void Zigbee_SendData(uint8_t len);
void Zigbee_Transmit(uint8_t len, uint16_t address);
void Zigbee_Init();
void Zigbee_ReceivedCount(void);
void Zigbee_Ping(void);
void Zigbee_Enable_Listen(void);
void Zigbee_Sensor_Update(uint16_t Tag_Id, uint8_t light, uint8_t acc, uint8_t mute);

#endif //ZIGBEE_H
