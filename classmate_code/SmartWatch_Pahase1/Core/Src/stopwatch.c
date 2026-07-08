#include "stopwatch.h"
#include "cmsis_os.h"

/* ======== 内部状态 ======== */
static StopWatchState_t state = SW_STOPPED;
static uint32_t started_at_tick = 0;   /* 启动 / 恢复时的 tick */
static uint32_t elapsed_before = 0;    /* 暂停前已累计的毫秒 */

uint8_t g_sw_btn_idx = 0;

/* ======== 初始化 ======== */
void StopWatch_Init(void)
{
    state = SW_STOPPED;
    started_at_tick = 0;
    elapsed_before = 0;
    g_sw_btn_idx = 0;
}

/* ======== 开始 / 继续 ======== */
void StopWatch_Start(void)
{
    if (state == SW_STOPPED) {
        elapsed_before = 0;
    }
    started_at_tick = xTaskGetTickCount();
    state = SW_RUNNING;
}

/* ======== 暂停 ======== */
void StopWatch_Pause(void)
{
    if (state != SW_RUNNING) return;
    uint32_t now = xTaskGetTickCount();
    elapsed_before += (now - started_at_tick);
    state = SW_PAUSED;
}

/* ======== 归零 (仅在暂停/停止时有效) ======== */
void StopWatch_Reset(void)
{
    elapsed_before = 0;
    started_at_tick = 0;
    state = SW_STOPPED;
    g_sw_btn_idx = 0;
}

/* ======== 获取当前状态 ======== */
StopWatchState_t StopWatch_GetState(void)
{
    return state;
}

/* ======== 获取当前时间 (分, 秒, 百分秒) ======== */
void StopWatch_GetTime(uint8_t *min, uint8_t *sec, uint8_t *cs)
{
    uint32_t elapsed;
    if (state == SW_RUNNING) {
        uint32_t now = xTaskGetTickCount();
        elapsed = elapsed_before + (now - started_at_tick);
    } else {
        elapsed = elapsed_before;
    }

    /* tick 频率通常 1000Hz → 1tick=1ms, 这里转换为 cs (10ms) */
    uint32_t total_cs = elapsed / 10;

    *cs  = (uint8_t)(total_cs % 100);
    uint32_t total_sec = total_cs / 100;
    *sec = (uint8_t)(total_sec % 60);
    *min = (uint8_t)(total_sec / 60);
}
