#ifndef __MENU_H
#define __MENU_H

#include <stdint.h>

/* ======== 页面类型 ======== */
typedef enum {
    PAGE_CLOCK = 0,
    PAGE_MENU,
    PAGE_CLOCK_MENU,   /* Clock 子菜单: Timer / StopWatch / Back */
    PAGE_STEPS,
    PAGE_SENSOR,
    PAGE_BLUETOOTH,
    PAGE_STOPWATCH,
    PAGE_TIMER,        /* 计时器 */
    PAGE_ABOUT,
    PAGE_COUNT
} DisplayPage_t;

/* ======== 菜单项数量 ======== */
#define MENU_ITEMS      5
#define CLOCK_MENU_ITEMS 3
#define MENU_LABEL_LEN  16

/* ======== 菜单消息类型 (通过 queueMenuMsg 传递) ======== */
#define MENU_MSG_BUTTON  0x01
#define MENU_MSG_ROTATE  0x02

/* ======== 时间结构 ======== */
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} TimeData_t;

/* ======== 传感器消息 (通过 queueSensorData 传递) ======== */
typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float pitch, roll;
    float temp;
    uint32_t step_count;
} SensorMsg_t;

/* ======== 计时器 UI 状态 ======== */
typedef enum {
    TIMER_SET_HOURS,
    TIMER_SET_MINUTES,
    TIMER_SET_SECONDS,
    TIMER_RUNNING,
    TIMER_PAUSED,
    TIMER_TIMEOUT
} TimerUIState_t;

/* ======== 全局状态 ======== */
extern DisplayPage_t g_current_page;
extern uint8_t g_menu_index;
extern uint8_t g_clock_menu_index;
extern TimeData_t g_time;

/* ======== 计时器 UI 变量 ======== */
extern TimerUIState_t g_timer_state;
extern uint8_t g_timer_h, g_timer_m, g_timer_s;
extern uint8_t g_timer_btn_idx;

/* ======== 蓝牙时间同步标记 (0=未同步, 1=已同步) ======== */
extern uint8_t g_time_synced;

/* ======== 蓝牙开关 (1=开启, 0=关闭) ======== */
extern uint8_t g_bluetooth_enabled;
extern uint8_t g_bt_btn_idx;

#endif
