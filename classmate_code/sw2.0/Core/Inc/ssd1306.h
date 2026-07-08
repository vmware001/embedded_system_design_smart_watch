/*
 * ssd1306.h
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */

#ifndef INC_SSD1306_H_
#define INC_SSD1306_H_

// 引入HAL库头文件，自带uint8_t等所有标准类型定义
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

// I2C 从机地址
#define SSD1306_ADDR        0x3C
// 左移一位得到I2C写地址，供.c文件调用
#define SSD1306_WR_ADDR     (SSD1306_ADDR << 1)

#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_PAGE_CNT  8

// 声明互斥锁（供外部创建）
extern osMutexId mutexI2C;

// 基本函数
void SSD1306_Init(void);
void SSD1306_Clear(void);
void SSD1306_SetCursor(uint8_t x, uint8_t page);
void SSD1306_WriteChar(char ch);
void SSD1306_WriteString(char *str);


#endif /* INC_SSD1306_H_ */
