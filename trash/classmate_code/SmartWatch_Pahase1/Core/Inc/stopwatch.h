#ifndef __STOPWATCH_H
#define __STOPWATCH_H

#include <stdint.h>

/* ======== 状态 ======== */
typedef enum {
    SW_STOPPED,
    SW_RUNNING,
    SW_PAUSED
} StopWatchState_t;

/* ======== 秒表按钮索引 (用于 encoder 选择) ======== */
/* STOPPED: BTN_START, BTN_BACK                */
/* RUNNING: BTN_PAUSE, BTN_BACK                */
/* PAUSED:  BTN_RESET, BTN_RESUME, BTN_BACK    */
#define SW_BTN_START    0
#define SW_BTN_PAUSE    0   /* 同 START, 标签不同 */
#define SW_BTN_RESET    0
#define SW_BTN_RESUME   1
#define SW_BTN_BACK     1   /* STOPPED/RUNNING 时的 BACK 索引 */
#define SW_BTN_BACK2    2   /* PAUSED 时的 BACK 索引 */

void StopWatch_Init(void);
void StopWatch_Start(void);            /* 开始 / 继续 */
void StopWatch_Pause(void);            /* 暂停 */
void StopWatch_Reset(void);            /* 归零 (仅在暂停时有效) */
StopWatchState_t StopWatch_GetState(void);
void StopWatch_GetTime(uint8_t *min, uint8_t *sec, uint8_t *cs);  /* cs=百分秒 */

/* 当前选中按钮索引 (0-based) */
extern uint8_t g_sw_btn_idx;

#endif
