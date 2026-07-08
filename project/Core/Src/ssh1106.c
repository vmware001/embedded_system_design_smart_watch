#include "ssh1106.h"
#include <string.h>
#include <stdlib.h>

// Frame buffer: 128x64 / 8 = 128x8 bytes
static uint8_t SSH1106_Buffer[SSH1106_HEIGHT / 8][SSH1106_WIDTH];
static I2C_HandleTypeDef *ssh1106_i2c;

static void SSH1106_WriteCommand(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  // Control byte: 0x00 = command
    HAL_I2C_Master_Transmit(ssh1106_i2c, SSH1106_I2C_ADDR, data, 2, 100);
}

static void SSH1106_WriteData(uint8_t *data, uint16_t len) {
    static uint8_t buf[SSH1106_WIDTH + 1];
    if (len > SSH1106_WIDTH) return;
    buf[0] = 0x40;  // Control byte: 0x40 = data
    memcpy(buf + 1, data, len);
    HAL_I2C_Master_Transmit(ssh1106_i2c, SSH1106_I2C_ADDR, buf, len + 1, 100);
}

void SSH1106_Init(I2C_HandleTypeDef *hi2c) {
    ssh1106_i2c = hi2c;
    
    HAL_Delay(100);
    
    SSH1106_WriteCommand(SSH1106_DISPLAY_OFF);
    SSH1106_WriteCommand(SSH1106_SET_DISPLAY_CLK_DIV);
    SSH1106_WriteCommand(0x80);
    SSH1106_WriteCommand(SSH1106_SET_MUX_RATIO);
    SSH1106_WriteCommand(0x3F);  // 64-1
    SSH1106_WriteCommand(SSH1106_SET_DISPLAY_OFFSET);
    SSH1106_WriteCommand(0x00);
    SSH1106_WriteCommand(SSH1106_SET_START_LINE | 0x00);
    SSH1106_WriteCommand(SSH1106_SET_CHARGE_PUMP);
    SSH1106_WriteCommand(0x14);  // Enable charge pump
    SSH1106_WriteCommand(SSH1106_SET_MEMORY_ADDR_MODE);
    SSH1106_WriteCommand(0x00);   // Horizontal addressing mode
    SSH1106_WriteCommand(SSH1106_SET_SEGMENT_REMAP);
    SSH1106_WriteCommand(SSH1106_SET_COM_SCAN_DIR);
    SSH1106_WriteCommand(SSH1106_SET_COM_PINS);
    SSH1106_WriteCommand(0x12);
    SSH1106_WriteCommand(SSH1106_SET_CONTRAST);
    SSH1106_WriteCommand(0xCF);
    SSH1106_WriteCommand(SSH1106_SET_PRECHARGE);
    SSH1106_WriteCommand(0xF1);
    SSH1106_WriteCommand(SSH1106_SET_VCOM_DETECT);
    SSH1106_WriteCommand(0x40);
    SSH1106_WriteCommand(SSH1106_DISPLAY_ALL_ON_RESUME);
    SSH1106_WriteCommand(SSH1106_NORMAL_DISPLAY);
    SSH1106_WriteCommand(SSH1106_DISPLAY_ON);
    
    SSH1106_Clear();
    SSH1106_Refresh();
}

void SSH1106_Clear(void) {
    memset(SSH1106_Buffer, 0x00, sizeof(SSH1106_Buffer));
}

void SSH1106_DrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= SSH1106_WIDTH || y >= SSH1106_HEIGHT) return;
    if (color) {
        SSH1106_Buffer[y / 8][x] |= (1 << (y % 8));
    } else {
        SSH1106_Buffer[y / 8][x] &= ~(1 << (y % 8));
    }
}

// 5x7 font - basic ASCII
static const uint8_t Font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00,  // Space
    0x00, 0x00, 0x5F, 0x00, 0x00,  // !
    // ... simplified, using a minimal font approach
    0x3E, 0x51, 0x49, 0x45, 0x3E,  // 0
    0x00, 0x42, 0x7F, 0x40, 0x00,  // 1
    0x42, 0x61, 0x51, 0x49, 0x46,  // 2
    0x21, 0x41, 0x45, 0x4B, 0x31,  // 3
    0x18, 0x14, 0x12, 0x7F, 0x10,  // 4
    0x27, 0x45, 0x45, 0x45, 0x39,  // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30,  // 6
    0x01, 0x71, 0x09, 0x05, 0x03,  // 7
    0x36, 0x49, 0x49, 0x49, 0x36,  // 8
    0x06, 0x49, 0x49, 0x29, 0x1E,  // 9
    // Uppercase A-Z
    0x7E, 0x11, 0x11, 0x11, 0x7E,  // A
    0x7F, 0x49, 0x49, 0x49, 0x36,  // B
    0x3E, 0x41, 0x41, 0x41, 0x22,  // C
    0x7F, 0x41, 0x41, 0x22, 0x1C,  // D
    0x7F, 0x49, 0x49, 0x49, 0x41,  // E
    0x7F, 0x09, 0x09, 0x09, 0x01,  // F
    0x3E, 0x41, 0x49, 0x49, 0x7A,  // G
    0x7F, 0x08, 0x08, 0x08, 0x7F,  // H
    0x00, 0x41, 0x7F, 0x41, 0x00,  // I
    0x20, 0x40, 0x41, 0x3F, 0x01,  // J
    0x7F, 0x08, 0x14, 0x22, 0x41,  // K
    0x7F, 0x40, 0x40, 0x40, 0x40,  // L
    0x7F, 0x02, 0x0C, 0x02, 0x7F,  // M
    0x7F, 0x04, 0x08, 0x10, 0x7F,  // N
    0x3E, 0x41, 0x41, 0x41, 0x3E,  // O
    0x7F, 0x09, 0x09, 0x09, 0x06,  // P
    0x3E, 0x41, 0x51, 0x21, 0x5E,  // Q
    0x7F, 0x09, 0x19, 0x29, 0x46,  // R
    0x46, 0x49, 0x49, 0x49, 0x31,  // S
    0x01, 0x01, 0x7F, 0x01, 0x01,  // T
    0x3F, 0x40, 0x40, 0x40, 0x3F,  // U
    0x1F, 0x20, 0x40, 0x20, 0x1F,  // V
    0x3F, 0x40, 0x38, 0x40, 0x3F,  // W
    0x63, 0x14, 0x08, 0x14, 0x63,  // X
    0x07, 0x08, 0x70, 0x08, 0x07,  // Y
    0x61, 0x51, 0x49, 0x45, 0x43,  // Z
    // lowercase a-z
    0x20, 0x54, 0x54, 0x54, 0x78,  // a
    0x7F, 0x48, 0x44, 0x44, 0x38,  // b
    0x38, 0x44, 0x44, 0x44, 0x20,  // c
    0x38, 0x44, 0x44, 0x48, 0x7F,  // d
    0x38, 0x54, 0x54, 0x54, 0x18,  // e
    0x08, 0x7E, 0x09, 0x01, 0x02,  // f
    0x0C, 0x52, 0x52, 0x52, 0x3E,  // g
    0x7F, 0x08, 0x04, 0x04, 0x78,  // h
    0x00, 0x44, 0x7D, 0x40, 0x00,  // i
    0x20, 0x40, 0x44, 0x3D, 0x00,  // j
    0x7F, 0x10, 0x28, 0x44, 0x00,  // k
    0x00, 0x41, 0x7F, 0x40, 0x00,  // l
    0x7C, 0x04, 0x18, 0x04, 0x78,  // m
    0x7C, 0x08, 0x04, 0x04, 0x78,  // n
    0x38, 0x44, 0x44, 0x44, 0x38,  // o
    0x7C, 0x14, 0x14, 0x14, 0x08,  // p
    0x08, 0x14, 0x14, 0x18, 0x7C,  // q
    0x7C, 0x08, 0x04, 0x04, 0x08,  // r
    0x48, 0x54, 0x54, 0x54, 0x20,  // s
    0x04, 0x3F, 0x44, 0x40, 0x20,  // t
    0x3C, 0x40, 0x40, 0x20, 0x7C,  // u
    0x1C, 0x20, 0x40, 0x20, 0x1C,  // v
    0x3C, 0x40, 0x30, 0x40, 0x3C,  // w
    0x44, 0x28, 0x10, 0x28, 0x44,  // x
    0x0C, 0x50, 0x50, 0x50, 0x3C,  // y
    0x44, 0x64, 0x54, 0x4C, 0x44,  // z
    // Special
    0x00, 0x00, 0x00, 0x00, 0x00,  // space
    0x00, 0x60, 0x60, 0x00, 0x00,  // .
    0x00, 0x00, 0x52, 0x24, 0x00,  // :
    0x00, 0x08, 0x08, 0x00, 0x00,  // -
    0x00, 0x00, 0x00, 0x00, 0x00,  // placeholder
    0x00, 0x00, 0x00, 0x00, 0x00,  // placeholder
    0x00, 0x00, 0x00, 0x00, 0x00,  // placeholder
    0x3E, 0x51, 0x49, 0x45, 0x3E,  // 0 (duplicate)
};

static uint8_t get_font_index(char c) {
    if (c >= '0' && c <= '9') return c - '0' + 2;  // 0-9 starts at index 2
    if (c >= 'A' && c <= 'Z') return c - 'A' + 12; // A-Z starts at index 12
    if (c >= 'a' && c <= 'z') return c - 'a' + 38; // a-z starts at index 38
    if (c == ' ') return 0;
    if (c == '.') return 64;
    if (c == ':') return 65;
    if (c == '-') return 66;
    return 0;
}

void SSH1106_DrawChar(uint8_t x, uint8_t y, char c, uint8_t color) {
    uint8_t idx = get_font_index(c);
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t line = Font5x7[idx * 5 + i];
        for (uint8_t j = 0; j < 7; j++) {
            if (line & (1 << j)) {
                SSH1106_DrawPixel(x + i, y + j, color);
            } else {
                SSH1106_DrawPixel(x + i, y + j, !color);
            }
        }
    }
}

void SSH1106_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t color) {
    while (*str) {
        SSH1106_DrawChar(x, y, *str, color);
        x += 6;
        str++;
    }
}

void SSH1106_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        SSH1106_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void SSH1106_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    SSH1106_DrawLine(x, y, x + w - 1, y, color);
    SSH1106_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    SSH1106_DrawLine(x, y, x, y + h - 1, color);
    SSH1106_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void SSH1106_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t i = x; i < x + w; i++) {
        for (uint8_t j = y; j < y + h; j++) {
            SSH1106_DrawPixel(i, j, color);
        }
    }
}

void SSH1106_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color) {
    for (uint8_t j = 0; j < h; j++) {
        for (uint8_t i = 0; i < w; i++) {
            if (bitmap[j * ((w + 7) / 8) + i / 8] & (1 << (i % 8))) {
                SSH1106_DrawPixel(x + i, y + j, color);
            }
        }
    }
}

void SSH1106_Refresh(void) {
    static uint8_t data[SSH1106_WIDTH + 1];
    
    for (uint8_t page = 0; page < 8; page++) {
        SSH1106_WriteCommand(SSH1106_SET_PAGE_ADDR + page);
        SSH1106_WriteCommand(SSH1106_SET_LOW_COL_ADDR | (SSH1106_COLUMN_OFFSET & 0x0F));
        SSH1106_WriteCommand(SSH1106_SET_HIGH_COL_ADDR | ((SSH1106_COLUMN_OFFSET >> 4) & 0x0F));
        
        data[0] = 0x40;  // Data control byte
        memcpy(data + 1, SSH1106_Buffer[page], SSH1106_WIDTH);
        HAL_I2C_Master_Transmit(ssh1106_i2c, SSH1106_I2C_ADDR, data, SSH1106_WIDTH + 1, 100);
    }
}
