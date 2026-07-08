#include "main.h"
#include "encoder.h"

extern TIM_HandleTypeDef htim3;

static uint8_t btn_stable = 1;   /* 上拉, 默认高电平 */
static uint8_t btn_counter = 0;  /* 消抖计数器 */

/* ======== 初始化编码器 ======== */
void Encoder_Init(void)
{
    /*
     * STM32F1 上拉配置: MODE=00(input), CNF=10(input+w/pull), ODR=1(pull-up)
     * PB10 在 CRH[11:8], shift = (10-8)*4 = 8
     */
    uint32_t tmp = GPIOB->CRH;
    tmp &= ~(0xFUL << 8);     /* 清除 PB10 的 MODE+CNF */
    tmp |=  (0x8UL << 8);     /* CNF=10, MODE=00 → 输入带上拉/下拉 */
    GPIOB->CRH = tmp;
    GPIOB->ODR |= (1UL << 10); /* ODR=1 → 上拉 */

    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    btn_stable  = (uint8_t)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);
    btn_counter = 0;
}

/* ======== 读取编码器增量, ±4 阈值消抖 ======== */
int8_t Encoder_GetDelta(void)
{
    int16_t cnt = __HAL_TIM_GET_COUNTER(&htim3);
    if (cnt >= 4 || cnt <= -4) {
        __HAL_TIM_SET_COUNTER(&htim3, 0);
        return (cnt > 0) ? 1 : -1;
    }
    return 0;
}

/* ======== 按键检测: 消抖下降沿(上拉→低电平=按下) ======== */

/* 诊断用: 暴露当前按键原始电平 */
uint8_t g_btn_raw = 1;

uint8_t Encoder_ButtonPressed(void)
{
    uint8_t raw = (uint8_t)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);
    g_btn_raw = raw;  /* 诊断 */

    if (raw == btn_stable) {
        btn_counter = 0;
        return 0;
    }

    btn_counter++;
    if (btn_counter >= 3) {
        btn_stable  = raw;
        btn_counter = 0;
        if (btn_stable == 0) {
            return 1;  /* 下降沿: 按下 */
        }
    }
    return 0;
}
