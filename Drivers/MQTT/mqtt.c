//
// Created by 沈俊儒 on 2020/7/1.
//

#include <MQTTConnect.h>
#include <transport.h>
#include <MQTTPacket.h>
#include <tim.h>
#include "../LCD/terminal.h"
#include "../StreamIO/stream_io.h"
#include "mqtt.h"
#include "string.h"
#include "../Wifi/wifi.h"
#include "../Zigbee/zigbee.h"

#define OUTPUT(w)         Lcd_Printf("MQ: "); Lcd_Printf(w);                Lcd_Printf("\n")
#define MESSAGE(fmt, ...) Lcd_Printf("MQ: "); Lcd_Printf(fmt, __VA_ARGS__); Lcd_Printf("\n")

static unsigned char buf[MQTT_BUF_SIZE];

#define STATE_CNT       6

#define STATE_CONNECT      0
#define STATE_SUBSCRIBE    1
#define STATE_SUB_LOOP     2
#define STATE_STUCK        3
#define STATE_PUBLISH      4
#define STATE_TERMINATED   5

#define START_STATE STATE_CONNECT

static char* Device_ID;
static char* Client_ID;
static char* UserName;
static char* EncodedPassword;
static uint16_t KeepAlive_Interval;

char Topic_Buf[256];

static volatile uint16_t PublishQueue[MQTT_BUF_SIZE];
static volatile int16_t PublishQueue_Tail = 0, PublishQueue_Length = 0;
static int16_t PublishQueue_Head = 0;

static void PublishQueue_Push(uint8_t data) {
    PublishQueue[PublishQueue_Tail] = data;
    PublishQueue_Tail = (PublishQueue_Tail + 1) % MQTT_BUF_SIZE;
    PublishQueue_Length++;
}

void PublishQueue_Add(uint16_t len, uint8_t type, uint8_t* data) {
    PublishQueue_Push(len);
    PublishQueue_Push(type);
    for (uint16_t off = 0; off < len; off++) {
        PublishQueue_Push(data[off]);
    }
}

static uint8_t PublishQueue_Empty(void) {
    return PublishQueue_Tail % MQTT_BUF_SIZE == PublishQueue_Head % MQTT_BUF_SIZE;
}

static uint16_t PublishQueue_Pop(void) {
    uint16_t result = PublishQueue[PublishQueue_Head];
    PublishQueue_Head = (PublishQueue_Head + 1) % MQTT_BUF_SIZE;
    PublishQueue_Length--;
    return result;
}

static char* getTopicName(const char* path) {
    snprintf(Topic_Buf, 256, "$oc/devices/%s%s", Device_ID, path);
    return Topic_Buf;
}

static uint8_t Action_Connect() {
    memset(buf, 0, sizeof(buf));

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

    data.clientID.cstring = Client_ID;
    data.keepAliveInterval = KeepAlive_Interval;
    data.cleansession = 1;
    data.username.cstring = UserName;
    data.password.cstring = EncodedPassword;

    int len = MQTTSerialize_connect(buf, MQTT_BUF_SIZE, &data);
    int rc = transport_sendPacketBuffer(0, buf, len);
    if (rc != len) {
        return STATE_CONNECT;
    }

    // ConnAck
    if (MQTTPacket_read(buf, MQTT_BUF_SIZE, transport_getdata) == CONNACK) {
        unsigned char Session_Present, ConnAck_RC;

        if (MQTTDeserialize_connack(&Session_Present, &ConnAck_RC, buf, MQTT_BUF_SIZE) != 1
        || ConnAck_RC != 0) {
            MESSAGE("Unable to MQTT connect, return code %d\n", ConnAck_RC);
            return STATE_CONNECT;
        }

        MESSAGE("Connect OK", ConnAck_RC);
        return STATE_SUBSCRIBE;
    }

    return STATE_CONNECT;
}

static uint8_t Action_Subscribe() {
    memset(buf, 0, sizeof(buf));

    int Req_QOS = 0;
    MQTTString Request_Topic = MQTTString_initializer;
    Request_Topic.cstring = getTopicName("/sys/commands/#");
    int len = MQTTSerialize_subscribe(buf, MQTT_BUF_SIZE, 0, 1, 1, &Request_Topic, &Req_QOS);
    transport_sendPacketBuffer(0, buf, len);
    if (MQTTPacket_read(buf, MQTT_BUF_SIZE, transport_getdata) == SUBACK) {
        unsigned short Subscribe_MessageId;
        int Subscribe_Count;
        int Granted_QOS;

        MQTTDeserialize_suback(&Subscribe_MessageId, 1, &Subscribe_Count, &Granted_QOS, buf, MQTT_BUF_SIZE);
        if (Granted_QOS != 0) {
            OUTPUT("Granted QOS must be 0");
            return STATE_STUCK;
        }

        OUTPUT("Subscribe OK");
    }

    OUTPUT("Listening...");
    return STATE_SUB_LOOP;
}

static uint8_t Action_Stuck() {
    return STATE_STUCK;
}

uint8_t Publish_Buf[MQTT_BUF_SIZE];
static uint8_t Action_Publish() {
    uint16_t len = PublishQueue_Pop();
    uint8_t type = (uint8_t) PublishQueue_Pop();
    for (uint16_t i = 0; i < len; i++) {
        Publish_Buf[i] = PublishQueue_Pop();
    }
    MQTTString topic = MQTTString_initializer;
    switch (type) {
        case UPLOAD_TYPE_PROPERTIES:
            topic.cstring = getTopicName("/sys/properties/report");
            break;

        case UPLOAD_TYPE_CONFIG_SYNC:
            topic.cstring = getTopicName("/user/sync_config_request");
            break;

        case UPLOAD_TYPE_SENSOR_EXCE:
            topic.cstring = getTopicName("/user/sensor_exception");
            break;

        default:
            break;
    }
    int Send_Len = MQTTSerialize_publish(buf, MQTT_BUF_SIZE, 0, 0, 0, 1, topic, Publish_Buf, len);
    MESSAGE("Published %d", len);
    transport_sendPacketBuffer(0, buf, Send_Len);
    return STATE_SUB_LOOP;
}

char Response_Topic_Buf[256];
uint8_t Response_Payload[128];
static uint8_t Action_SubLoop() {
    memset(buf, 0, sizeof(buf));

    // 每一次先响应服务器端的请求
    if (MQTTPacket_read(buf, MQTT_BUF_SIZE, transport_getdata) == PUBLISH) {
        unsigned char dup;
        int qos;
        unsigned char retained;
        unsigned short msgid;
        int PayloadLen_In;
        unsigned char* Payload_In;
        MQTTString Received_Topic;

        MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &Received_Topic, &Payload_In, &PayloadLen_In, buf, MQTT_BUF_SIZE);

        int Start_Pos = 0;
        for (Start_Pos = 0; Start_Pos < Received_Topic.lenstring.len; Start_Pos++) {
            if (Received_Topic.lenstring.data[Start_Pos] == '=') {
                break;
            }
        }

        snprintf(Response_Topic_Buf, 256, "/sys/commands/response/request_id=%.*s", Received_Topic.lenstring.len - Start_Pos - 1, Received_Topic.lenstring.data + Start_Pos + 1);

        uint16_t Response_Payload_Len = 0;

        // 响应命令
        uint8_t Message_Id = Payload_In[0];
        Response_Payload[1] = Payload_In[1]; // mid
        Response_Payload[2] = Payload_In[2];
        switch (Message_Id) {
            // 传感器配置
            case 0x01: {
                uint16_t Tag_Id = (Payload_In[3] << 8u) + Payload_In[4];
                uint8_t Acc_Sensor = Payload_In[5], Light_Sensor = Payload_In[6], Mute_Mode = Payload_In[7];
                Zigbee_Sensor_Update(Tag_Id, Light_Sensor, Acc_Sensor, Mute_Mode);
                MESSAGE("Configured %d (A, L, M) = (%d, %d, %d)", Tag_Id, Acc_Sensor, Light_Sensor, Mute_Mode);
                Response_Payload[3] = 0x00; // errcode
                Response_Payload[4] = 0x01; // success (patch)
                Response_Payload[0] = 0x02;    // Message ID
                Response_Payload_Len = 5;
                break;
            }

            // 返回所有Readers
            case 0x03: {
                uint16_t len = 0;
                for (uint16_t i = 0; i < MAX_ALLOWED_READER_N; i++) {
                    if (Reader_Address_Mapping[i][2]) {
                        *(uint16_t*) (Response_Payload + 6 + len) = Reader_Address_Mapping[i][0];
                        len += 2;
                    }
                }
                Response_Payload[0] = 0x04;
                Response_Payload[3] = 0x00;
                Response_Payload[4] = len >> 8u;
                Response_Payload[5] = len & 0x00FFu;
                Response_Payload_Len = len + 6;
                break;
            }

            default:
                break;
        }

        MQTTString Response_Topic = MQTTString_initializer;
        Response_Topic.cstring = getTopicName(Response_Topic_Buf);
        int len = MQTTSerialize_publish(buf, MQTT_BUF_SIZE, 0, 0, 0, 1, Response_Topic, Response_Payload, Response_Payload_Len);
        transport_sendPacketBuffer(0, buf, len);
        OUTPUT("Response OK");
    }

    // 然后取消息队列中的内容后发送
    if (!PublishQueue_Empty()) {
        return STATE_PUBLISH;
    }

    return STATE_SUB_LOOP;
}

typedef uint8_t (*ACTION_T)();
static ACTION_T actions[STATE_CNT] = {
        Action_Connect,
        Action_Subscribe,
        Action_SubLoop,
        Action_Stuck,
        Action_Publish
};

void Mqtt_Connection_Config(char* _Device_ID, char* _Client_ID, char* _UserName, char* _EncodedPassword, uint16_t _KeepAlive_Interval) {
    Device_ID = _Device_ID;
    Client_ID = _Client_ID;
    UserName = _UserName;
    EncodedPassword = _EncodedPassword;
    KeepAlive_Interval = _KeepAlive_Interval;
}

void Mqtt_Client_Start() {
    OUTPUT("Hi!");

    uint8_t Current_State = START_STATE;
    while (Current_State != STATE_TERMINATED) {
        // MESSAGE("Enter state %d", Current_State);
        Current_State = actions[Current_State]();
    }
}
