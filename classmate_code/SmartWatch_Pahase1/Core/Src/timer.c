#include "timer.h"
#include "cmsis_os.h"

static uint32_t timer_total_ms = 0;
static uint32_t timer_start_tick = 0;
static uint8_t  timer_running  = 0;
static uint8_t  timer_paused   = 0;
static uint8_t  timer_timeout  = 0;

void Timer_Init(void)
{
    timer_total_ms   = 0;
    timer_start_tick = 0;
    timer_running    = 0;
    timer_paused     = 0;
    timer_timeout    = 0;
}

void Timer_SetTotal(uint8_t h, uint8_t m, uint8_t s)
{
    timer_total_ms = ((uint32_t)h * 3600UL + (uint32_t)m * 60UL + s) * 1000UL;
}

void Timer_Start(void)
{
    timer_start_tick = xTaskGetTickCount();
    timer_running    = 1;
    timer_paused     = 0;
    timer_timeout    = 0;
}

void Timer_Pause(void)
{
    if (!timer_running || timer_paused) return;
    uint32_t elapsed = xTaskGetTickCount() - timer_start_tick;
    if (elapsed >= timer_total_ms) {
        timer_total_ms = 0;
        timer_running  = 0;
        timer_timeout  = 1;
    } else {
        timer_total_ms -= elapsed;
        timer_running   = 0;
        timer_paused    = 1;
    }
}

void Timer_Resume(void)
{
    if (!timer_paused) return;
    timer_start_tick = xTaskGetTickCount();
    timer_running    = 1;
    timer_paused     = 0;
}

uint8_t Timer_IsRunning(void) { return timer_running; }
uint8_t Timer_IsPaused(void)  { return timer_paused; }
uint8_t Timer_IsTimeout(void) { return timer_timeout; }

void Timer_GetRemaining(uint8_t *h, uint8_t *m, uint8_t *s)
{
    if (!timer_running) {
        uint32_t sec = timer_total_ms / 1000UL;
        *h = (uint8_t)(sec / 3600UL);
        *m = (uint8_t)((sec % 3600UL) / 60UL);
        *s = (uint8_t)(sec % 60UL);
        return;
    }

    uint32_t elapsed = xTaskGetTickCount() - timer_start_tick;
    if (elapsed >= timer_total_ms) {
        timer_running = 0;
        timer_timeout = 1;
        *h = 0; *m = 0; *s = 0;
        return;
    }

    uint32_t remain_ms = timer_total_ms - elapsed;
    uint32_t sec = remain_ms / 1000UL;
    *h = (uint8_t)(sec / 3600UL);
    *m = (uint8_t)((sec % 3600UL) / 60UL);
    *s = (uint8_t)(sec % 60UL);
}
