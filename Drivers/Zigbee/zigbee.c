//
// Created by 沈俊儒 on 2020/6/27.
//

#include <usart.h>
#include <tim.h>
#include "stm32l4xx_hal.h"
#include "zigbee.h"
#include "../StreamIO/stream_io.h"
#include "string.h"
#include "../MQTT/mqtt.h"

#define OUTPUT(w)         Lcd_Printf("ZB: "); Lcd_Printf(w);                Lcd_Printf("\n")
#define MESSAGE(fmt, ...) Lcd_Printf("ZB: "); Lcd_Printf(fmt, __VA_ARGS__); Lcd_Printf("\n")

struct Config Zigbee_Config;

static const char Type_Mapping[][20] = {
        "",
        "COORDINATOR",
        "ROUTER",
        "END_DEVICE"
};

static const uint32_t Baud_Rate_Mapping[] = {
        0,
        1200,
        2400,
        4800,
        9600,
        19200,
        38400,
        57600,
        115200
};

uint16_t Tag_Address_Mapping[MAX_ALLOWED_TAG_N][11];
uint16_t Reader_Address_Mapping[MAX_ALLOWED_READER_N][3];

uint8_t Tx_Buf[TX_BUFFER_SIZE], Rx_Buf[RX_BUFFER_SIZE];
uint8_t *Send_Buf = Tx_Buf + 2, *Transmit_Buf = Tx_Buf + 4;

volatile uint8_t Received_Length, Need_To_Receive, state = 0;

#define OP_CONFIG 0x00
#define OP_PING 0x01

#define OP_TAG_SAY_HELLO 0x00
#define OP_READER_SAY_HELLO 0x01
#define OP_TAG_PONG 0x02
#define OP_REPORT_INVALID 0x03

static void Address_Mapping_Insert_Tag(uint16_t tid, uint16_t Short_Address) {
    uint32_t time = HAL_GetTick();

    // Find if exists first, to avoid conflict
    for (uint16_t i = 0; i < MAX_ALLOWED_TAG_N; i++) {
        // if exists, update Short Address
        if (Tag_Address_Mapping[i][2] && Tag_Address_Mapping[i][0] == tid) {
            Tag_Address_Mapping[i][1] = Short_Address;
            *(uint32_t*) (&(Tag_Address_Mapping[i][9])) = time;
            // terminate
            return;
        }

        // if short address exists, update tag tid
        // TODO: consider address conflict
        if (Tag_Address_Mapping[i][2] && Tag_Address_Mapping[i][1] == Short_Address) {
            Tag_Address_Mapping[i][0] = tid;
            *(uint32_t*) (&(Tag_Address_Mapping[i][9])) = time;
            // terminate
            return;
        }
    }

    // Find the first position that is not alive, and insert
    for (uint16_t i = 0; i < MAX_ALLOWED_TAG_N; i++) {
        if (!Tag_Address_Mapping[i][2]) {
            Tag_Address_Mapping[i][2] = 1;
            Tag_Address_Mapping[i][0] = tid;
            Tag_Address_Mapping[i][1] = Short_Address;
            *(uint32_t*) (&(Tag_Address_Mapping[i][9])) = time;
            return;
        }
    }
}

static void Address_Mapping_Insert_Reader(uint16_t rid, uint16_t Short_Address) {
    // Find if exists first, to avoid conflict
    for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
        // if exists, update Short Address
        if (Reader_Address_Mapping[i][2] && Reader_Address_Mapping[i][0] == rid) {
            Reader_Address_Mapping[i][1] = Short_Address;
            // terminate
            return;
        }

        // if short address exists, update tag tid
        // TODO: consider address conflict
        if (Reader_Address_Mapping[i][2] && Reader_Address_Mapping[i][1] == Short_Address) {
            Reader_Address_Mapping[i][0] = rid;
            // terminate
            return;
        }
    }

    // Find the first position that is not alive, and insert
    for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
        if (!Reader_Address_Mapping[i][2]) {
            Reader_Address_Mapping[i][2] = 1;
            Reader_Address_Mapping[i][0] = rid;
            Reader_Address_Mapping[i][1] = Short_Address;
            return;
        }
    }
}

static uint16_t Address_Mapping_Get_Reader_Rid(uint16_t Reader_Addr) {
    if (Reader_Addr == 0x00) {
        return -1;
    }
    for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
        if (Reader_Address_Mapping[i][2] && Reader_Address_Mapping[i][1] == Reader_Addr) {
            return Reader_Address_Mapping[i][0];
        }
    }
    return -1;
}

static uint16_t Address_Mapping_Get_Tag_Tid(uint16_t Tag_Addr) {
    if (Tag_Addr == 0x00) {
        return -1;
    }
    for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
        if (Tag_Address_Mapping[i][2] && Tag_Address_Mapping[i][1] == Tag_Addr) {
            return Tag_Address_Mapping[i][0];
        }
    }
    return -1;
}

static uint16_t Address_Mapping_Get_Tag_Addr(uint16_t Tag_Tid) {
    for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
        if (Tag_Address_Mapping[i][2] && Tag_Address_Mapping[i][0] == Tag_Tid) {
            return Tag_Address_Mapping[i][1];
        }
    }
    return 0x00;
}

static void Address_Mapping_Update(uint16_t Tag_Addr,
        uint16_t Router1_Addr, uint16_t Router1_Distance,
        uint16_t Router2_Addr, uint16_t Router2_Distance,
        uint16_t Router3_Addr, uint16_t Router3_Distance) {
    uint16_t rid1 = Address_Mapping_Get_Reader_Rid(Router1_Addr);
    uint16_t rid2 = Address_Mapping_Get_Reader_Rid(Router2_Addr);
    uint16_t rid3 = Address_Mapping_Get_Reader_Rid(Router3_Addr);

    for (uint16_t i = 0; i < MAX_ALLOWED_TAG_N; i++) {
        if (Tag_Address_Mapping[i][2] && Tag_Address_Mapping[i][1] == Tag_Addr) {
            Tag_Address_Mapping[i][3] = rid1;
            Tag_Address_Mapping[i][4] = Router1_Distance;
            Tag_Address_Mapping[i][5] = rid2;
            Tag_Address_Mapping[i][6] = Router2_Distance;
            Tag_Address_Mapping[i][7] = rid3;
            Tag_Address_Mapping[i][8] = Router3_Distance;
            *(uint32_t*) (&(Tag_Address_Mapping[i][9])) = HAL_GetTick();
            break;
        }
    }
}

#define TAG_REQUEST_BUF_SIZE 300
uint8_t Tag_Request_Buf[TAG_REQUEST_BUF_SIZE];
static void Zigbee_Handle() {
    if (Received_Length < 4) {
        // Malformed package
        return;
    }
    uint8_t len = Received_Length - 5;
    uint8_t* data = Rx_Buf + 3;
    uint16_t addr = Rx_Buf[Received_Length - 1] + (Rx_Buf[Received_Length - 2] << 8u);
    uint8_t op = Rx_Buf[2];

    switch (op) {
        case OP_TAG_SAY_HELLO: {
            if (len < 2) {
                return;
            }
            uint16_t Tag_Tid = *(uint16_t *) data;
            Address_Mapping_Insert_Tag(Tag_Tid, addr);
            Tag_Request_Buf[0] = 0x05;
            Tag_Request_Buf[1] = Tag_Tid >> 8u;
            Tag_Request_Buf[2] = Tag_Tid & 0x00FFu;
            PublishQueue_Add(3, UPLOAD_TYPE_CONFIG_SYNC, Tag_Request_Buf);
            break;
        }

        case OP_READER_SAY_HELLO:
            if (len < 2) {
                return;
            }
            uint16_t Reader_Rid = *(uint16_t*) data;
            Address_Mapping_Insert_Reader(Reader_Rid, addr);
            break;

        case OP_TAG_PONG:
            Address_Mapping_Update(addr, *(uint16_t*) (data + 1), data[0], *(uint16_t*) (data + 4), data[3], *(uint16_t*) (data + 7), data[6]);
            break;

        case OP_REPORT_INVALID: {
            if (len < 1) {
                return;
            }
            uint16_t tid = Address_Mapping_Get_Tag_Tid(addr);
            uint8_t sensor = data[0];
            Tag_Request_Buf[0] = 0x06;
            Tag_Request_Buf[1] = tid >> 8u;
            Tag_Request_Buf[2] = tid & 0x00FFu;
            Tag_Request_Buf[3] = sensor;
            PublishQueue_Add(4, UPLOAD_TYPE_SENSOR_EXCE, Tag_Request_Buf);
            break;
        }

        default:
            break;
    }
}

void Zigbee_UART_IRQ(void) {
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE) != RESET) {
        uint8_t dt = huart3.Instance->RDR & 0x00FFu;
        Rx_Buf[Received_Length++] = dt;
        switch (state) {
            case 0:
                state = 1;
                break;

            case 1:
                state = 2;
                /*
                 * 1. 0x00 len ... addr addr
                 * 2. 0xFC len addr addr ...
                 * so 2 + dt
                 */
                Need_To_Receive = 2 + dt;
                break;

            case 2:
                Need_To_Receive--;
                if (Need_To_Receive == 0) {
                    Zigbee_Handle();
                    Received_Length = 0;
                    state = 0;
                }
                break;
        }
    } else {
        __HAL_UART_CLEAR_PEFLAG(&huart3);
        __HAL_UART_CLEAR_FEFLAG(&huart3);
        __HAL_UART_CLEAR_NEFLAG(&huart3);
        __HAL_UART_CLEAR_OREFLAG(&huart3);
    }
}
uint8_t Calc_Checksum(uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += Tx_Buf[i];
    }
    return sum & 0x00FFu;
}

void Zigbee_Connect() {
    Tx_Buf[0] = 0xFC;
    Tx_Buf[1] = 0x06;
    Tx_Buf[2] = 0x04;
    Tx_Buf[3] = 0x44;
    Tx_Buf[4] = 0x54;
    Tx_Buf[5] = 0x4B;
    Tx_Buf[6] = 0x52;
    Tx_Buf[7] = 0x46;
    Tx_Buf[8] = Calc_Checksum(8);

    try:
    HAL_UART_Transmit(&huart3, Tx_Buf, 9, 0x2000);
    if (HAL_OK != HAL_UART_Receive(&huart3, Rx_Buf, 10, 0x500)) {
        OUTPUT("Connect failed, retrying...");
        HAL_Delay(2000);
        goto try;
    }

    Zigbee_Config.connected = 1;
    Zigbee_Config.version = ((Rx_Buf[7] << 8u) + Rx_Buf[8]) / 10.0;
}

void Zigbee_Read_Config() {
    Tx_Buf[0] = 0xFC;
    Tx_Buf[1] = 0x06;
    Tx_Buf[2] = 0x05;
    Tx_Buf[3] = 0x44;
    Tx_Buf[4] = 0x54;
    Tx_Buf[5] = 0x4B;
    Tx_Buf[6] = 0x52;
    Tx_Buf[7] = 0x46;
    Tx_Buf[8] = Calc_Checksum(8);

    try:
    HAL_UART_Transmit(&huart3, Tx_Buf, 9, 0x2000);
    if (HAL_OK != HAL_UART_Receive(&huart3, Rx_Buf, 47, 0x1000)) {
        OUTPUT("Read config failed, retrying...");
        HAL_Delay(2000);
        goto try;
    }

    uint8_t* p = Rx_Buf + 4;
    Zigbee_Config.type = p[0];
    Zigbee_Config.channel = p[3];
    Zigbee_Config.Transport_Mode = p[4];
    Zigbee_Config.address = (p[5] << 8u) + p[6];
    Zigbee_Config.Baud_Rate = p[9];
    Zigbee_Config.Aerial_Type = p[15];
    Zigbee_Config.Pan_ID = (p[1] << 8u) + p[2];
}

void Zigbee_Print_Config() {
    OUTPUT("Zigbee info");
    Lcd_Printf("Connected: %d\n", Zigbee_Config.connected);
    Lcd_Printf("Pan ID: %X\n", Zigbee_Config.Pan_ID);
    Lcd_Printf("Custom address: %X\n", Zigbee_Config.address);
    Lcd_Printf("Version: %lf\n", Zigbee_Config.version);
    Lcd_Printf("Type: %s\n", Type_Mapping[Zigbee_Config.type]);
    Lcd_Printf("Channel: %X\n", Zigbee_Config.channel);
    switch (Zigbee_Config.Transport_Mode) {
        case TRANS_TRANSPARENT:
            Lcd_Printf("Transport Mode: Transparent\n");
            break;

        case TRANS_TRANSPARENT_MAC:
            Lcd_Printf("Transport Mode: Transparent MAC\n");
            break;

        case TRANS_N2N:
            Lcd_Printf("Transport Mode: Node to Node\n");
            break;

        case TRANS_TRANSPARENT_ADR:
            Lcd_Printf("Transport Mode: Transparent Short Address\n");
            break;
    }
    Lcd_Printf("Baud Rate: %d\n", Baud_Rate_Mapping[Zigbee_Config.Baud_Rate]);
    Lcd_Printf("Aerial Type: %s\n", Zigbee_Config.Aerial_Type == AERIAL_INTERNAL ? "internal" : "external");
}

uint8_t Zigbee_Write_Config() {
    uint8_t* p = Tx_Buf + 3;
    
    p[0] = Zigbee_Config.type;
    p[1] = Zigbee_Config.Pan_ID >> 8u;
    p[2] = Zigbee_Config.Pan_ID & 0x00FFu;
    p[3] = Zigbee_Config.channel;
    p[4] = Zigbee_Config.Transport_Mode;
    p[5] = Zigbee_Config.address >> 8u;
    p[6] = Zigbee_Config.address & 0x00FFu;
    p[7] = 0xAA;
    p[8] = 0xBB;
    p[9] = Zigbee_Config.Baud_Rate;
    p[10] = 0x01;
    p[11] = 0x01;
    p[12] = 0x01;
    p[13] = 0x05;
    p[14] = 0xA6;
    p[15] = Zigbee_Config.Aerial_Type;

    p[16] = ROUTER;
    p[17] = Zigbee_Config.Pan_ID >> 8u;
    p[18] = Zigbee_Config.Pan_ID & 0x00FFu;
    p[19] = Zigbee_Config.channel;
    p[20] = Zigbee_Config.Transport_Mode;
    p[21] = Zigbee_Config.address >> 8u;
    p[22] = Zigbee_Config.address & 0x00FFu;
    p[23] = 0xCC;
    p[24] = 0xDD;
    p[25] = Zigbee_Config.Baud_Rate;
    p[26] = 0x01;
    p[27] = 0x01;
    p[28] = 0x01;
    p[29] = 0x05;
    p[30] = 0xA6;
    p[31] = Zigbee_Config.Aerial_Type;
    p[32] = 0x01;
    p[33] = 0x00;
    p[34] = 0x00;
    p[35] = 0x00;
    p[36] = 0x00;
    p[37] = 0x00;

    Tx_Buf[0] = 0xFC;
    Tx_Buf[1] = 0x27;
    Tx_Buf[2] = 0x07;

    Tx_Buf[41] = Calc_Checksum(41);

    try:
    HAL_UART_Transmit(&huart3, Tx_Buf, 42, 0x2000);
    if (HAL_OK != HAL_UART_Receive(&huart3, Rx_Buf, 5, 0x1000)) {
        OUTPUT("Write config failed, retrying...");
        HAL_Delay(2000);
        goto try;
    }

    if (Rx_Buf[2] != 0x0A) {
        OUTPUT("Write config failed.");
        return 0;
    }
    return 1;
}

#define PAN_ID 0x2A01
#define BAUD_RATE 0x06
#define CHANNEL 0x0F

void Zigbee_ConfigureAs(uint8_t type) {
    Zigbee_Connect();
    Zigbee_Config.type = type;
    Zigbee_Config.Pan_ID = PAN_ID;
    Zigbee_Config.Aerial_Type = (type == END_DEVICE ? AERIAL_INTERNAL : AERIAL_EXTERNAL);
    Zigbee_Config.Baud_Rate = BAUD_RATE;
    Zigbee_Config.address = 0x01;
    Zigbee_Config.channel = CHANNEL;
    Zigbee_Config.Transport_Mode = TRANS_TRANSPARENT_ADR;
    if (!Zigbee_Write_Config()) {
        MESSAGE("%s Configure Failed", Type_Mapping[type]);
        return;
    }
    MESSAGE("%s Configure OK", Type_Mapping[type]);
    Zigbee_Read_Config();
    Zigbee_Print_Config();
}

void Zigbee_Init() {
    __HAL_UART_CLEAR_PEFLAG(&huart3);
    __HAL_UART_CLEAR_FEFLAG(&huart3);
    __HAL_UART_CLEAR_NEFLAG(&huart3);
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

void Zigbee_Enable_Listen(void) {
    OUTPUT("Wait for 20 seconds to initialize...");
    HAL_Delay(20000);
    OUTPUT("IntelliKeeper activated!");
    __HAL_TIM_CLEAR_FLAG(&htim15, TIM_SR_UIF);
    HAL_TIM_Base_Start_IT(&htim15);
}

#define ZIGBEE_REPORT_BUF_SIZE 1024
#define ZIGBEE_PING_FREQ (15 * 1000)
static uint8_t Zigbee_Report_Buf[ZIGBEE_REPORT_BUF_SIZE];
static uint16_t Listen_Counter = 0;
void Zigbee_Listen(void) {
    if (Listen_Counter++ <= 15) {
        return;
    }
    Listen_Counter = 0;

    uint16_t p = 0;
    // Kill those dead but not observed by others
    uint32_t time = HAL_GetTick();
    for (uint16_t i = 0; i < MAX_ALLOWED_TAG_N; i++) {
        uint32_t Last_Survived = *(uint32_t*) (&(Tag_Address_Mapping[i][9]));
        if (!Tag_Address_Mapping[i][2] || (time - Last_Survived > ZIGBEE_PING_FREQ)) {
            Tag_Address_Mapping[i][2] = 0;
            continue;
        }

        *(uint16_t*) (Zigbee_Report_Buf + 3 + p) = Tag_Address_Mapping[i][0];
        Zigbee_Report_Buf[3 + p] = Tag_Address_Mapping[i][0] >> 8u;
        Zigbee_Report_Buf[4 + p] = Tag_Address_Mapping[i][0] & 0x00FFu;
        p += 2;

        for (uint8_t j = 0; j < 6; j += 2) {
            Zigbee_Report_Buf[3 + p] = Tag_Address_Mapping[i][3 + j] >> 8u;
            Zigbee_Report_Buf[4 + p] = Tag_Address_Mapping[i][3 + j] & 0x00FFu;
            Zigbee_Report_Buf[5 + p] = Tag_Address_Mapping[i][4 + j] >> 8u;
            Zigbee_Report_Buf[6 + p] = Tag_Address_Mapping[i][4 + j] & 0x00FFu;
            p += 4;
        }
    }

    Zigbee_Report_Buf[0] = 0x00;
    Zigbee_Report_Buf[1] = p >> 8u;
    Zigbee_Report_Buf[2] = p & 0x00FFu;
    PublishQueue_Add(p + 3, UPLOAD_TYPE_PROPERTIES, Zigbee_Report_Buf);
    MESSAGE("Reported %d", p);
}

void Zigbee_SendData(uint8_t len) {
    Tx_Buf[0] = 0x00;
    Tx_Buf[1] = len;
    HAL_UART_Transmit(&huart3, Tx_Buf, len + 2, 500);
    HAL_Delay(200);
}

void Zigbee_Transmit(uint8_t len, uint16_t address) {
    Tx_Buf[0] = 0xFD;
    Tx_Buf[1] = len;
    Tx_Buf[2] = address >> 8u;
    Tx_Buf[3] = address & 0x00FFu;
    HAL_UART_Transmit(&huart3, Tx_Buf, len + 4, 500);
    HAL_Delay(200);
}

void Zigbee_Ping(void) {
    Send_Buf[0] = OP_PING;
    Zigbee_SendData(1);
}

void Zigbee_Sensor_Update(uint16_t Tag_Id, uint8_t light, uint8_t acc, uint8_t mute) {
    uint16_t Tag_Addr = Address_Mapping_Get_Tag_Addr(Tag_Id);
    if (Tag_Addr == 0x00) {
        return;
    }
    Transmit_Buf[0] = OP_CONFIG;
    Transmit_Buf[1] = light;
    Transmit_Buf[2] = acc;
    Transmit_Buf[3] = mute;
    Zigbee_Transmit(4, Tag_Addr);
}
