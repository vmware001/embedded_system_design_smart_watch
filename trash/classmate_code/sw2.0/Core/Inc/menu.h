/*
 * menu.h
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */

#ifndef INC_MENU_H_
#define INC_MENU_H_

#include <stdint.h>

// 按键事件定义
#define BTN_NONE    0x00
#define BTN_SHORT   0x01
#define BTN_LONG    0x02

// 菜单消息结构体
typedef struct {
    int16_t delta;      // 编码器旋转增量
    uint8_t button;     // 按键事件
} MenuMsg;

uint8_t ReadEncoderButton(void);

#endif /* INC_MENU_H_ */
