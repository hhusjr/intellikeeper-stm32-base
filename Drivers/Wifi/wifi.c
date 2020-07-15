//
// Created by 沈俊儒 on 2020/6/25.
//

#include "stm32l4xx_hal.h"
#include "wifi.h"
#include "../LCD/terminal.h"
#include "../StreamIO/stream_io.h"
#include <stdio.h>
#include <usart.h>
#include <tim.h>
#include "string.h"
#include "stdarg.h"

#define OUTPUT(w)         Lcd_Printf("WF: "); Lcd_Printf(w);                Lcd_Printf("\n")
#define MESSAGE(fmt, ...) Lcd_Printf("WF: "); Lcd_Printf(fmt, __VA_ARGS__); Lcd_Printf("\n")

static volatile uint8_t Wifi_Rx[WIFI_BUF_SIZE];
static uint8_t Wifi_Tx[WIFI_BUF_SIZE];
static volatile uint16_t Rx_Length = 0;

static volatile uint16_t RingBuffer[WIFI_BUF_SIZE];
static volatile int16_t RingBuffer_Tail = 0, RingBuffer_Length = 0;
static int16_t RingBuffer_Head = 0;

static volatile uint8_t Is_Received = 0;

static volatile uint16_t Received_Timer = 0;

static uint16_t Pending_Time = 20;

static uint8_t Stream_Mode = 0;

#define RING_BUFFER_GET(pos) RingBuffer[((RingBuffer_Head + pos) % WIFI_BUF_SIZE + WIFI_BUF_SIZE) % WIFI_BUF_SIZE]

static void RingBuffer_Push(uint16_t data) {
    RingBuffer[RingBuffer_Tail] = data;
    RingBuffer_Tail = (RingBuffer_Tail + 1) % WIFI_BUF_SIZE;
    RingBuffer_Length++;
}

static uint8_t RingBuffer_Empty(void) {
    return RingBuffer_Tail % WIFI_BUF_SIZE == RingBuffer_Head % WIFI_BUF_SIZE;
}

static uint16_t RingBuffer_Pop(void) {
    uint16_t result = RingBuffer[RingBuffer_Head];
    RingBuffer_Head = (RingBuffer_Head + 1) % WIFI_BUF_SIZE;
    RingBuffer_Length--;
    return result;
}

static uint8_t RingBuffer_Clear(void) {
    uint16_t cnt = RingBuffer[RingBuffer_Head];
    for (uint16_t i = 0; i < cnt + 1; i++) {
        RingBuffer_Pop();
    }
    return 0;
}

static uint8_t RingBuffer_Print(void) {
    for (uint16_t i = 0; i < RingBuffer[RingBuffer_Head]; i++) {
        Terminal_PutChar(RING_BUFFER_GET(i));
    }
    return 0;
}

void Wifi_Receive_Time(void) {
    Received_Timer++;

    if (Received_Timer > Pending_Time) {
        if (!Stream_Mode) {
            RingBuffer_Push(Rx_Length);
            for (uint16_t i = 0; i < Rx_Length; i++) {
                RingBuffer_Push(Wifi_Rx[i]);
            }
            Rx_Length = 0;
        }
        Received_Timer = 0;
        Is_Received = 1;
        HAL_TIM_Base_Stop_IT(&htim6);
    }
}

void Wifi_UART_IRQ(void) {
    if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE) != RESET) {
        Received_Timer = 0;
        if (Rx_Length == 0) {
            Is_Received = 0;
            HAL_TIM_Base_Start_IT(&htim6);
        }
        if (!Stream_Mode) {
            Wifi_Rx[Rx_Length++] = hlpuart1.Instance->RDR & 0x00FFu;
        } else {
            RingBuffer_Push(hlpuart1.Instance->RDR & 0x00FFu);
        }
    } else {
        __HAL_UART_CLEAR_PEFLAG(&hlpuart1);
        __HAL_UART_CLEAR_FEFLAG(&hlpuart1);
        __HAL_UART_CLEAR_NEFLAG(&hlpuart1);
        __HAL_UART_CLEAR_OREFLAG(&hlpuart1);
    }
}

static void Wifi_AT_Async(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf((char*) Wifi_Tx, WIFI_BUF_SIZE, fmt, args);
    va_end(args);

    Is_Received = 0;
    HAL_UART_Transmit(&hlpuart1, Wifi_Tx, strlen((char*) Wifi_Tx), 0x1000);

    while (!Is_Received);
}

static uint16_t Expect_From(const char* data, int32_t from) {
    for (int32_t i = from; i < ((int32_t) RingBuffer[RingBuffer_Head]) - (int32_t) strlen(data) + 1; i++) {
        const char* ch = data;
        uint8_t HasDifference = 0, delta = 0;
        while (*ch != '\0') {
            if (RING_BUFFER_GET(1 + i + delta) != *ch) {
                HasDifference = 1;
                break;
            }
            delta++;
            ch++;
        }
        if (!HasDifference) {
            return i + 1;
        }
    }
    return 0;
}

static uint16_t Expect(const char* data) {
    return Expect_From(data, 0);
}

typedef uint8_t (*ACTION_T)();

#define STATE_CNT       12

#define STATE_SAY_HELLO         0
#define STATE_CONNECT           1
#define STATE_ENTER_AP          2
#define STATE_RESET             3
#define STATE_AP_CONFIG         4
#define STATE_CREATE_SERVER     5
#define STATE_REQUEST_LOOP      6
#define STATE_MAKE_RESPONSE     7
#define STATE_MAKE_CONNECTION   8
#define STATE_LEAVE_AP          9
#define STATE_LEAVE_AP_RESET    10
#define STATE_TERMINATED        11

#define START_STATE     STATE_SAY_HELLO

#define MUST_OK(handler, FAILED_STATE) \
do { \
    handler(); \
    if (!Expect("OK")) { \
        RingBuffer_Print(); \
        RingBuffer_Clear(); \
        return FAILED_STATE; \
    } \
} while (0)

#define MUST_OK_WARGS(handler, FAILED_STATE, ...) \
do { \
    handler(__VA_ARGS__); \
    if (!Expect("OK")) { \
        RingBuffer_Print(); \
        RingBuffer_Clear(); \
        return FAILED_STATE; \
    } \
} while (0)

uint8_t Action_SayHello() {
    Pending_Time = 20;

    MUST_OK(WIFI_SAY_HELLO, STATE_SAY_HELLO);
    RingBuffer_Clear();
    return STATE_CONNECT;
}

uint8_t Action_EnterAP() {
    MUST_OK_WARGS(WIFI_CHANGE_MODE, STATE_ENTER_AP, WIFI_MODE_MIXED);
    RingBuffer_Clear();
    return STATE_RESET;
}

uint8_t Action_Connect() {
    OUTPUT("Testing WIFI Connection...");
    HAL_Delay(10000);

    MUST_OK(WIFI_GET_CONNECT_INFO, STATE_CONNECT);
    if (Expect("No AP")) {
        RingBuffer_Clear();
        return STATE_ENTER_AP;
    }

    OUTPUT("Wifi OK");
    RingBuffer_Clear();

    return STATE_TERMINATED;
}

uint8_t Action_Reset() {
    Pending_Time = 1000;
    MUST_OK(WIFI_RESET, STATE_RESET);
    RingBuffer_Clear();

    HAL_Delay(3000);

    return STATE_AP_CONFIG;
}

uint8_t Action_AP_Config() {
    Pending_Time = 500;

    const static char *SSID = "IntelliKeeper", *PASS = "intellikeeper";
    const static uint8_t channel = 2;

    MUST_OK_WARGS(WIFI_AP_CONFIG, STATE_AP_CONFIG, SSID, PASS, channel, WIFI_ENCRYPT_WPA2_PSK);
    RingBuffer_Clear();

    Lcd_Printf("AP Ready:\n");
    Lcd_Printf("SSID: %s\n", SSID);
    Lcd_Printf("PASS: %s\n", PASS);
    Lcd_Printf("CHAN: %d\n", channel);

    return STATE_CREATE_SERVER;
}

uint8_t Action_AP_CreateServer() {
    const static uint16_t port = 8080;

    MUST_OK(WIFI_ALLOW_MULTI_CONN, STATE_CREATE_SERVER);
    RingBuffer_Clear();

    MUST_OK_WARGS(WIFI_CREATE_SERVER, STATE_CREATE_SERVER, port);
    RingBuffer_Clear();

    Lcd_Printf("Server Ready:\n");
    Lcd_Printf("PORT: %d\n", port);

    while (!RingBuffer_Empty()) {
        RingBuffer_Clear();
    }

    return STATE_REQUEST_LOOP;
}

uint8_t Action_AP_EnterRequestLoop() {
    Pending_Time = 100;
    if (!RingBuffer_Empty()) {
        if (Expect("+IPD")) {
            if (Expect("GET / HTTP/1.1")) {
                RingBuffer_Clear();
                return STATE_MAKE_RESPONSE;
            } else if (Expect("POST / HTTP/1.1")) {
                // RingBuffer_Clear(); 需要手动处理rb中的内容
                return STATE_MAKE_CONNECTION;
            } else {
                WIFI_CLOSE_CIP(0);
                RingBuffer_Clear();
            }
        }
        RingBuffer_Clear();
    }
    return STATE_REQUEST_LOOP;
}

#define HTML_BUFFER_SIZE 4096
uint8_t Response_Buffer[HTML_BUFFER_SIZE], HTML_Buffer[HTML_BUFFER_SIZE];
uint8_t CWLAP_Buffer[HTML_BUFFER_SIZE];
uint8_t Action_AP_MakeResponse() {
    OUTPUT("Scanning AP, Please wait for about 10 seconds...");
    Pending_Time = 5000;
    MUST_OK(WIFI_CWLAP, STATE_REQUEST_LOOP);
    OUTPUT("Scan OK");

    uint16_t len = RingBuffer_Pop();
    uint16_t CWLAP_Buffer_Ptr = 0;
    uint8_t Write_Flag = 0;
    while (len--) {
        uint8_t ch = RingBuffer_Pop();
        if (ch == '(') {
            CWLAP_Buffer[CWLAP_Buffer_Ptr++] = '[';
            Write_Flag = 1;
            continue;
        } else if (ch == ')') {
            Write_Flag = 0;
            CWLAP_Buffer[CWLAP_Buffer_Ptr++] = ']';
            CWLAP_Buffer[CWLAP_Buffer_Ptr++] = ',';
            continue;
        }
        if (Write_Flag) {
            CWLAP_Buffer[CWLAP_Buffer_Ptr++] = ch;
        }
    }
    CWLAP_Buffer[CWLAP_Buffer_Ptr] = '\0';

    snprintf((char*) HTML_Buffer, HTML_BUFFER_SIZE, "<!DOCTYPE html><html><head><title>IntelliKeeper</title><meta charset=\"utf-8\"><style>html,body{padding:0;margin:0}table{margin:20px auto;width:720px;border-collapse:collapse}tr{height:80px;cursor:pointer}"
                                        "td{font-size:24px;border-bottom:solid 1px #fff;text-align:center}tr:hover{background:#fff;color:#1BA1E2;text-shadow:1px 1px 1px #000}</style></head><body style=\"background: #1BA1E2; color: #fff;\"><h1 sty"
                                        "le=\"margin:30px 100px;font-size:48px;text-shadow:1px 1px 8px #000;\">设备配网</h1><div style=\"color: #1BA1E2; margin: 0; background: #fff; padding: 20px;\"><h3 style=\"margin: 0;\" id=\"a\">将智慧管家基站"
                                        "连接到您的路由器。</h3><form style=\"margin-top: 20px; display: none;\" id=\"form\" action=\"\" method=\"post\"> <input type=\"hidden\" name=\"s\" id=\"s\" /> <label id=\"pw\"> <span id=\"enc\"></span>密码："
                                        " <input type=\"password\" name=\"p\" placeholder=\"密码\" value=\"\"/> </label> <input type=\"submit\" value=\"连接\"/></form></div><table id=\"routers\"></table><p style=\"text-align: center; margin-top: 30px;"
                                        " font-size: 12px;\">IntelliKeeper Base</p> <script>var aps=[%s];aps.sort(function(x,y){return y[2]-x[2]});var routers=document.getElementById('routers');var v={0:'很好',1:'很好',2:'很好',3:'很好',4:'很好',5:'很好',"
                                        "6:'很好',7:'好',8:'中等',9:'差',10:'很差'};var e=['开放','WEP','WPA_PSK','WPA2_PSK','WPA_WPA2_PSK'];for(var ri in aps){var router=aps[ri];routers.innerHTML+=' <tr class=\"r\" onclick=\"i('+ri+');\"><td>'+router[1]+"
                                        "'</td><td>信号'+(v[parseInt((-router[2]-5)/10)])+'</td><td style=\"font-size: 12px;\">'+(router[0]?'加密':'未加密')+'</td> </tr> '} function i(ri){document.body.scrollTop=document.documentElement.scrollTop=0;docume"
                                        "nt.getElementById('form').style.cssText='margin-top:20px';document.getElementById('a').innerText='连接到'+aps[ri][1];document.getElementById('enc').innerText=e[aps[ri][0]];document.getElementById('s').value=aps[ri][1];"
                                        "if(aps[ri][0]){document.getElementById('pw').style.cssText='display:inline';}else{document.getElementById('pw').style.cssText='display:none';}}</script></body></html>", CWLAP_Buffer);

    snprintf((char*) Response_Buffer, HTML_BUFFER_SIZE, "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: close\r\n"
                               "Content-Length: %d\r\n\r\n%s",
                               strlen((char*) HTML_Buffer),
                               HTML_Buffer);

    Pending_Time = 500;
    // 分成多批传送
    uint8_t* p = Response_Buffer;
    uint16_t Total_Length = strlen((char*) Response_Buffer);
    uint16_t TrunkSize = 512;
    uint16_t Remain_Length = Total_Length;

    for (uint16_t i = 0; i < Total_Length / TrunkSize; i++) {
        uint8_t tmp = p[TrunkSize];
        p[TrunkSize] = '\0';

        WIFI_SEND(0, TrunkSize);
        if (!Expect(">")) {
            RingBuffer_Clear();
            return STATE_REQUEST_LOOP;
        }
        RingBuffer_Clear();

        MUST_OK_WARGS(WIFI_CONTENT_SEND, STATE_REQUEST_LOOP, p);
        RingBuffer_Clear();

        p[TrunkSize] = tmp;
        Remain_Length -= TrunkSize;
        p += TrunkSize;
    }

    WIFI_SEND(0, Remain_Length);
    if (!Expect(">")) {
        RingBuffer_Clear();
        return STATE_REQUEST_LOOP;
    }
    RingBuffer_Clear();

    MUST_OK_WARGS(WIFI_CONTENT_SEND, STATE_REQUEST_LOOP, p);
    RingBuffer_Clear();

    WIFI_CLOSE_CIP(0);
    RingBuffer_Clear();

    return STATE_REQUEST_LOOP;
}

#define MAKE_RESULT_RESPONSE(body) \
    do { \
        RingBuffer_Clear(); \
        snprintf((char*) Response_Buffer, HTML_BUFFER_SIZE, "HTTP/1.1 200 OK\r\n" \
                                                    "Content-Type: text/html\r\n" \
                                                    "Connection: close\r\n" \
                                                    "Content-Length: %d\r\n\r\n%s", \
         strlen(body), \
        body); \
        WIFI_SEND(0, strlen((char*) Response_Buffer)); \
        if (!Expect(">")) { \
            RingBuffer_Clear(); \
            return STATE_REQUEST_LOOP; \
        } \
        RingBuffer_Clear(); \
        MUST_OK_WARGS(WIFI_CONTENT_SEND, STATE_REQUEST_LOOP, Response_Buffer); \
        RingBuffer_Clear(); \
        WIFI_CLOSE_CIP(0); \
        RingBuffer_Clear(); \
    } while (0)

uint8_t SSID_Buf[64], Password_Buf[64];
uint8_t Action_AP_MakeConnection() {
    uint16_t pos = Expect("Content-Length");
    if (pos == 0) {
        MAKE_RESULT_RESPONSE("<!DOCTYPE html><html><head>"
                             "<title>IntelliKeeper</title><meta charset=\"utf-8\">"
                             "</head><body style=\"background:#ff0000;\">"
                             "<h1 style=\"padding:80px;color:#fff\">连接失败，Content-Length缺失。<a href=\"/\">回去</a></h1>"
                             "</body></html>");
        return STATE_REQUEST_LOOP;
    }
    pos--;
    uint16_t len = 0;
    while (RING_BUFFER_GET(pos) != '\r') {
        uint8_t ch = RING_BUFFER_GET(pos);
        if (ch >= '0' && ch <= '9') {
            len = len * 10 + ch - '0';
        }
        pos++;
    }

    uint16_t Body_Pos = Expect_From("\r\n\r\n", pos);
    if (Body_Pos == 0) {
        MAKE_RESULT_RESPONSE("<!DOCTYPE html><html><head>"
                             "<title>IntelliKeeper</title><meta charset=\"utf-8\">"
                             "</head><body style=\"background:#ff0000;\">"
                             "<h1 style=\"padding:80px;color:#fff\">连接失败，body缺失。<a href=\"/\">回去</a></h1>"
                             "</body></html>");
        return STATE_REQUEST_LOOP;
    }
    Body_Pos += 4; // 移动到第一个参数的位置

    uint8_t state = 0;
    uint8_t SSID_Buf_Pointer = 0, Password_Buf_Pointer = 0;

    while (len--) {
        volatile uint16_t ch = RING_BUFFER_GET(Body_Pos);
        if (ch == '&') {
            state = 1;
            Body_Pos++;
            continue;
        }
        if (state == 0) {
            SSID_Buf[SSID_Buf_Pointer++] = ch;
        } else {
            Password_Buf[Password_Buf_Pointer++] = ch;
        }
        Body_Pos++;
    }
    SSID_Buf[SSID_Buf_Pointer] = '\0';
    Password_Buf[Password_Buf_Pointer] = '\0';

    if (strlen((char*) SSID_Buf) < 2 || strlen((char*) Password_Buf) < 2) {
        MAKE_RESULT_RESPONSE("<!DOCTYPE html><html><head>"
                             "<title>IntelliKeeper</title><meta charset=\"utf-8\">"
                             "</head><body style=\"background:#ff0000;\">"
                             "<h1 style=\"padding:80px;color:#fff\">连接失败，ssid或密码缺失。<a href=\"/\">回去</a></h1>"
                             "</body></html>");
        return STATE_REQUEST_LOOP;
    }

    char* ssid = (char*) SSID_Buf + 2;
    char* password = (char*) Password_Buf + 2;

    MAKE_RESULT_RESPONSE("<!DOCTYPE html><html><head>"
                         "<title>IntelliKeeper</title><meta charset=\"utf-8\">"
                         "</head><body style=\"background:#1BA1E2;\">"
                         "<h1 style=\"padding:80px;color:#fff\">连接中，请看基站LCD屏显提示。如果连接失败，您可以<a href=\"/\">重新选择</a></h1>"
                         "</body></html>");

    OUTPUT("Connecting to WIFI, Please wait...");

    WIFI_CONNECT_TO_AP(ssid, password);
    RingBuffer_Clear();

    while (RingBuffer_Empty());

    if (Expect("WIFI CONNECTED")) {
        OUTPUT("Connect OK! Waiting for IP...");
        RingBuffer_Clear();
    } else {
        RingBuffer_Clear();
        OUTPUT("Connect failed, please check SSID and password.");
        return STATE_REQUEST_LOOP;
    }

    while (RingBuffer_Empty());

    if (Expect("WIFI GOT IP")) {
        OUTPUT("Got IP! Reconfiguring device...");
    } else {
        OUTPUT("No IP! Reconfiguring device...");
    }
    RingBuffer_Clear();

    MUST_OK(WIFI_REJECT_MULTI_CONN, STATE_CONNECT);
    RingBuffer_Clear();

    MUST_OK(WIFI_SHUTDOWN_SERVER, STATE_CONNECT);
    RingBuffer_Clear();

    return STATE_LEAVE_AP;
}

uint8_t Action_AP_Leave() {
    MUST_OK_WARGS(WIFI_CHANGE_MODE, STATE_LEAVE_AP, WIFI_MODE_STATION);
    RingBuffer_Clear();
    return STATE_LEAVE_AP_RESET;
}

uint8_t Action_AP_Reset() {
    Pending_Time = 1000;
    MUST_OK(WIFI_RESET, STATE_RESET);
    RingBuffer_Clear();

    HAL_Delay(3000);

    OUTPUT("Restarting...");
    return STATE_SAY_HELLO;
}

static ACTION_T actions[STATE_CNT] = {
        Action_SayHello,
        Action_Connect,
        Action_EnterAP,
        Action_Reset,
        Action_AP_Config,
        Action_AP_CreateServer,
        Action_AP_EnterRequestLoop,
        Action_AP_MakeResponse,
        Action_AP_MakeConnection,
        Action_AP_Leave,
        Action_AP_Reset
};

void Wifi_Start() {
    //WIFI_RESTORE();
    __HAL_UART_CLEAR_PEFLAG(&hlpuart1);
    __HAL_UART_CLEAR_FEFLAG(&hlpuart1);
    __HAL_UART_CLEAR_NEFLAG(&hlpuart1);
    __HAL_UART_CLEAR_OREFLAG(&hlpuart1);

    __HAL_UART_ENABLE_IT(&hlpuart1, UART_IT_RXNE);

    OUTPUT("Setting up WIFI Module...");
    HAL_Delay(3000);

    while (!RingBuffer_Empty()) {
        RingBuffer_Clear();
    }

    uint8_t Current_State = START_STATE;
    while (Current_State != STATE_TERMINATED) {
       // MESSAGE("Enter state %d", Current_State);
        Current_State = actions[Current_State]();
    }

    while (!RingBuffer_Empty()) {
        RingBuffer_Clear();
    }
}

void Wifi_TCP_Connect(const char* host, uint16_t port) {
    ready:
    Pending_Time = 500;
    WIFI_SET_CIP();
    if (!Expect("OK")) {
        RingBuffer_Clear();
        goto ready;
    }
    OUTPUT("CIP Ready!");
    RingBuffer_Clear();

    conn:
    Pending_Time = 2000;
    MESSAGE("Making TCP Connection... %s:%d", host, port);
    WIFI_CONNECT("TCP", host, port);
    if (!Expect("OK")) {
        RingBuffer_Print();
        RingBuffer_Clear();
        HAL_Delay(2000);
        goto conn;
    }
    RingBuffer_Clear();
    MESSAGE("TCP Connect OK! %s:%d", host, port);

    enable:
    Pending_Time = 500;
    WIFI_ENTER_CIP();
    if (!Expect("OK")) {
        RingBuffer_Clear();
        goto enable;
    }
    OUTPUT("CIP Enabled!");

    while (!Expect(">"));
    RingBuffer_Clear();

    Pending_Time = 100;
    Stream_Mode = 1;

    while (!RingBuffer_Empty()) {
        RingBuffer_Clear();
    }
}

void Wifi_TCP_Send(uint8_t* buffer, uint16_t len) {
    Is_Received = 0;
    Pending_Time = 200;
    HAL_UART_Transmit(&hlpuart1, buffer, len, 10000);
}

uint16_t Wifi_TCP_Receive(uint8_t* buffer, uint16_t len, uint16_t timeout) {
    if (RingBuffer_Length < len) {
        uint16_t counter = 0;
        while (!Is_Received) {
            HAL_Delay(1);
            if (++counter > timeout) {
                return 0;
            }
        }
    }
    for (uint16_t i = 0; i < len; i++) {
        if (RingBuffer_Empty()) {
            return i;
        }
        buffer[i] = RingBuffer_Pop();
    }
    return len;
}
