#ifndef __SSD1306_H
#define __SSD1306_H

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#define SSD1306_I2C_ADDR 0x78

void SSD1306_Init(void);
void SSD1306_FillAll(void);
void SSD1306_Clear(void);
void SSD1306_SetCursor(uint8_t x, uint8_t page);
void SSD1306_WriteChar(char ch);
void SSD1306_WriteString(char *str);
void SSD1306_Flush(void);

/* 大字体: 将 6x8 源字库缩放 sx×sy 倍绘制, x/y 为像素坐标 */
void SSD1306_DrawBigChar(char ch, uint8_t x, uint8_t y, uint8_t sx, uint8_t sy);

/* 矩形: 像素坐标 (x,y) 为左上角, w宽 h高, 空心/实心 */
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/* 中文字符串: UTF-8, 像素坐标, spacing=水平步进(推荐14) */
void SSD1306_DrawCNStr(const char *str, uint8_t x, uint8_t y, uint8_t spacing);

#endif
