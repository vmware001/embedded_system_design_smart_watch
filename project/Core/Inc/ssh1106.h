#ifndef __SSH1106_H
#define __SSH1106_H

#include "stm32f1xx_hal.h"

#define SSH1106_WIDTH   128
#define SSH1106_HEIGHT  64

#define SSH1106_I2C_ADDR    0x78  // 0x3C << 1

// Basic commands
#define SSH1106_DISPLAY_OFF         0xAE
#define SSH1106_DISPLAY_ON            0xAF
#define SSH1106_SET_DISPLAY_CLK_DIV   0xD5
#define SSH1106_SET_MUX_RATIO         0xA8
#define SSH1106_SET_DISPLAY_OFFSET    0xD3
#define SSH1106_SET_START_LINE        0x40
#define SSH1106_SET_CHARGE_PUMP       0x8D
#define SSH1106_SET_MEMORY_ADDR_MODE  0x20
#define SSH1106_SET_SEGMENT_REMAP     0xA1
#define SSH1106_SET_COM_SCAN_DIR      0xC8
#define SSH1106_SET_COM_PINS          0xDA
#define SSH1106_SET_CONTRAST          0x81
#define SSH1106_SET_PRECHARGE         0xD9
#define SSH1106_SET_VCOM_DETECT       0xDB
#define SSH1106_DISPLAY_ALL_ON_RESUME 0xA4
#define SSH1106_NORMAL_DISPLAY        0xA6
#define SSH1106_SET_PAGE_ADDR         0xB0
#define SSH1106_SET_HIGH_COL_ADDR     0x10
#define SSH1106_SET_LOW_COL_ADDR      0x00

// SSH1106 has 2-pixel horizontal offset compared to SSD1306
#define SSH1106_COLUMN_OFFSET  2

void SSH1106_Init(I2C_HandleTypeDef *hi2c);
void SSH1106_Clear(void);
void SSH1106_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void SSH1106_DrawChar(uint8_t x, uint8_t y, char c, uint8_t color);
void SSH1106_DrawString(uint8_t x, uint8_t y, const char *str, uint8_t color);
void SSH1106_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void SSH1106_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void SSH1106_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void SSH1106_DrawBitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color);
void SSH1106_Refresh(void);

#endif
