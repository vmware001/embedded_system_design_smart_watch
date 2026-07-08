#ifndef __UI_H
#define __UI_H

#include "ssh1106.h"
#include "mpu6050.h"

// Page IDs
typedef enum {
    PAGE_HOME = 0,
    PAGE_STEPS,
    PAGE_INFO,
    PAGE_BLUETOOTH,    PAGE_STOPWATCH,
    PAGE_MAX
} PageID_t;

void UI_Init(void);
void UI_DrawPage(PageID_t page);
void UI_SetPage(PageID_t page);
PageID_t UI_GetPage(void);
void UI_GoHome(void);
void UI_NextPage(void);
void UI_PrevPage(void);

// Individual page draw functions
void UI_DrawHome(void);
void UI_DrawSteps(void);
void UI_DrawInfo(void);
void UI_DrawBluetooth(void);    void UI_DrawStopwatch(void);

// Update dynamic data on current page
void UI_UpdateData(uint32_t steps, float temp, int16_t ax, int16_t ay, int16_t az);

// Stopwatch control (based on HAL_GetTick, no accumulation error)
void UI_StopwatchToggle(void);
void UI_StopwatchReset(void);
void UI_GetStopwatch(uint8_t *min, uint8_t *sec, uint8_t *ms_100);
uint8_t UI_IsStopwatchRunning(void);

#endif
