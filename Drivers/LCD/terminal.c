/**
 * LCD终端驱动
 * (1) Cursor 字符光标，具有x、y属性，描述当前控制的字符在屏幕中的位置
 * (2) Buffer 终端环形缓冲区，包含Head、Tail、CurrentLinea属性，表示缓冲区的头尾，以及当前屏幕中显示的区域的起始位置
 * @author SJR
 */

#include "terminal.h"
#include "lcd.h"

static uint16_t Cursor_X = HORIZONTAL_MARGIN, Cursor_Y = VERTICAL_MARGIN;
static uint16_t Character_FrontColor = 0xFFFF, Character_BackColor = 0x0000;
static char Terminal_Buf[MAX_BUFFERED_LINE_CNT][LINE_BUF_SIZE + 1];
static int16_t Terminal_Buf_Head, Terminal_Buf_Tail, Terminal_Buf_CurrentLine, Terminal_Buf_Length;

void Terminal_Initialize(void) {
    LCD_Initialize();
    LCD_Clear(TERMINAL_BACK_COLOR);
    Terminal_Buf_Head = 0;
    Terminal_Buf_Tail = 0;
    Terminal_Buf_CurrentLine = 0;
    Terminal_Buf_Length = 1;
}

static void Terminal_Buf_Push(char ch) {
    if (ch == '\n') {
        if (Terminal_Buf_Length >= MAX_BUFFERED_LINE_CNT) {
            Terminal_Buf_Head = (Terminal_Buf_Head + 1) % MAX_BUFFERED_LINE_CNT;
            Terminal_Buf_Length--;
        }
        Terminal_Buf_Length++;
        Terminal_Buf_Tail = (Terminal_Buf_Tail + 1) % MAX_BUFFERED_LINE_CNT;
        uint16_t current = (Terminal_Buf_Tail + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT;
        Terminal_Buf[current][0] = 0;
        return;
    }

    uint16_t current = (Terminal_Buf_Tail + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT;
    if (Terminal_Buf[current][0] < LINE_BUF_SIZE) {
        Terminal_Buf[current][++Terminal_Buf[current][0]] = ch;
    }
}

static void Terminal_Cursor_Forward(void) {
    Cursor_X += CHAR_WIDTH + HORIZONTAL_MARGIN;
}

static void Terminal_Cursor_Next(void) {
    Cursor_Y += CHAR_HEIGHT + VERTICAL_MARGIN;
}

static void Terminal_Cursor_ResetX(void) {
    Cursor_X = HORIZONTAL_MARGIN;
}

static void Terminal_Cursor_ResetY(void) {
    Cursor_Y = VERTICAL_MARGIN;
}

static void Terminal_Draw(char ch) {
    LCD_Draw_Terminal_Char(Cursor_X, Cursor_Y, ch, Character_FrontColor, Character_BackColor);
}

void Terminal_ScrollUp() {
    if ((Terminal_Buf_CurrentLine - Terminal_Buf_Head + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT == 0) {
        return;
    }
    Terminal_Buf_CurrentLine = (Terminal_Buf_CurrentLine - 1 + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT;
}

void Terminal_ScrollDown() {
    if ((Terminal_Buf_CurrentLine - Terminal_Buf_Tail + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT == 0) {
        return;
    }
    Terminal_Buf_CurrentLine = (Terminal_Buf_CurrentLine + 1) % MAX_BUFFERED_LINE_CNT;
}

void Terminal_Buf_Render() {
    LCD_Clear(TERMINAL_BACK_COLOR);
    Terminal_Cursor_ResetY();
    for (uint16_t p = Terminal_Buf_CurrentLine, i = 0;
         i < MAX_LINE_PER_SCREEN;
         p = (p + 1) % MAX_BUFFERED_LINE_CNT, i++) {
        Terminal_Cursor_ResetX();

        for (uint16_t offset = 1; offset <= Terminal_Buf[p][0]; offset++) {
            Terminal_Draw(Terminal_Buf[p][offset]);
            Terminal_Cursor_Forward();
        }
        if ((p - Terminal_Buf_Tail + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT == 0) {
            break;
        }
        Terminal_Cursor_Next();
    }
    Terminal_Cursor_ResetX();
}

static uint8_t Terminal_Buf_AutoScrollDown() {
    if ((Terminal_Buf_CurrentLine + MAX_LINE_PER_SCREEN - Terminal_Buf_Tail + MAX_BUFFERED_LINE_CNT) % MAX_BUFFERED_LINE_CNT == 0) {
        Terminal_Buf_CurrentLine = (Terminal_Buf_CurrentLine + 1) % MAX_BUFFERED_LINE_CNT;
        Terminal_Buf_Render();
        return 1;
    }
    return 0;
}

void Terminal_PutChar(char chr) {
    // overflow x
    if (chr != '\n' && Cursor_X > LCD_WIDTH - CHAR_WIDTH - HORIZONTAL_MARGIN) {
        // auto break line
        Terminal_PutChar('\n');
    }

    Terminal_Buf_Push(chr);
    if (chr == '\n') {
        if (!Terminal_Buf_AutoScrollDown()) {
            Terminal_Cursor_Next();
            Terminal_Cursor_ResetX();
        }
        return;
    }

    LCD_Draw_Terminal_Char(Cursor_X, Cursor_Y, chr, Character_FrontColor, Character_BackColor);
    Cursor_X += HORIZONTAL_MARGIN + CHAR_WIDTH;
}

void Terminal_Write(const char* string) {
    const char* p = string;
    while (*p != '\0') {
        Terminal_PutChar(*p);
        p++;
    }
}
