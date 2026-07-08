/*
 * menu.c
 * 旋转编码器 + 按键菜单交互模块
 */
#include "menu.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "tim.h"

// ========== 硬件配置 ==========
#define BTN_GPIO_PORT   GPIOB
#define BTN_GPIO_PIN    GPIO_PIN_10
#define BTN_DEBOUNCE_MS     20
#define BTN_LONG_PRESS_MS   800

// 全局RTOS对象（在 main.c 中定义）
extern QueueHandle_t queueMenuMsg;
extern SemaphoreHandle_t semDisplayUpdate;

/**
 * @brief 按键状态机检测
 */
uint8_t ReadEncoderButton(void)
{
    static uint8_t state = 0;
    static uint32_t press_tick = 0;
    uint8_t key_level = HAL_GPIO_ReadPin(BTN_GPIO_PORT, BTN_GPIO_PIN);

    switch (state)
    {
        case 0: // 等待按下
            if (key_level == GPIO_PIN_RESET)
            {
                state = 1;
                press_tick = HAL_GetTick();
            }
            break;

        case 1: // 消抖
            if (key_level == GPIO_PIN_RESET)
            {
                if (HAL_GetTick() - press_tick >= BTN_DEBOUNCE_MS)
                    state = 2;
            }
            else
                state = 0;
            break;

        case 2: // 长短按判定
            if (key_level == GPIO_PIN_SET) // 释放
            {
                state = 0;
                if (HAL_GetTick() - press_tick < BTN_LONG_PRESS_MS)
                    return BTN_SHORT;
            }
            else if (HAL_GetTick() - press_tick >= BTN_LONG_PRESS_MS)
            {
                state = 3;
                return BTN_LONG;
            }
            break;

        case 3: // 长按触发后等待释放
            if (key_level == GPIO_PIN_SET)
                state = 0;
            break;

        default:
            state = 0;
            break;
    }
    return BTN_NONE;
}


