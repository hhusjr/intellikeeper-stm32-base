//
// Created by 沈俊儒 on 2020/7/1.
//

#ifndef MQTT_H
#define MQTT_H

#define MQTT_BUF_SIZE 512

#define UPLOAD_TYPE_PROPERTIES  1
#define UPLOAD_TYPE_CONFIG_SYNC 2
#define UPLOAD_TYPE_SENSOR_EXCE 3

#include "stm32l4xx_hal.h"

void Mqtt_Connection_Config(char* _Device_ID, char* _Client_ID, char* _UserName, char* _EncodedPassword, uint16_t _KeepAlive_Interval);
void Mqtt_Client_Start();
void PublishQueue_Add(uint16_t len, uint8_t type, uint8_t* data);

#endif //MQTT_H
