#include "ui.h"
#include "bluetooth.h"
#include <stdio.h>
#include <string.h>

static PageID_t current_page = PAGE_HOME;
static uint32_t ui_steps = 0;
static float ui_temp = 0.0f;
static int16_t ui_ax = 0, ui_ay = 0, ui_az = 0;

// Stopwatch state
static uint8_t sw_running = 0;
static uint32_t sw_start_tick = 0;
static uint32_t sw_paused_tick = 0;

// !!! VERSION MARKER - Update this when code changes !!!
#define FIRMWARE_VERSION "v35"

void UI_Init(void) {
    current_page = PAGE_HOME;
    SSH1106_Clear();
    SSH1106_Refresh();
}

void UI_SetPage(PageID_t page) {
    if (page < PAGE_MAX) {
        current_page = page;
    }
}

PageID_t UI_GetPage(void) {
    return current_page;
}

void UI_GoHome(void) {
    current_page = PAGE_HOME;
    UI_StopwatchReset();  // 回主页时清零秒表
}

void UI_NextPage(void) {
    current_page = (current_page + 1) % PAGE_MAX;
}

void UI_PrevPage(void) {
    current_page = (current_page == 0) ? (PAGE_MAX - 1) : (current_page - 1);
}

void UI_UpdateData(uint32_t steps, float temp, int16_t ax, int16_t ay, int16_t az) {
    ui_steps = steps;
    ui_temp = temp;
    ui_ax = ax;
    ui_ay = ay;
    ui_az = az;
}

// Stopwatch: based on HAL_GetTick, no accumulation error
void UI_StopwatchToggle(void) {
    if (sw_running) {
        sw_paused_tick = HAL_GetTick() - sw_start_tick;
        sw_running = 0;
    } else {
        sw_start_tick = HAL_GetTick() - sw_paused_tick;
        sw_paused_tick = 0;
        sw_running = 1;
    }
}

void UI_StopwatchReset(void) {
    sw_running = 0;
    sw_start_tick = 0;
    sw_paused_tick = 0;
}

uint8_t UI_IsStopwatchRunning(void) {
    return sw_running;
}

void UI_GetStopwatch(uint8_t* min, uint8_t* sec, uint8_t* ms_100) {
    uint32_t total_ms = sw_paused_tick;
    if (sw_running) {
        total_ms = HAL_GetTick() - sw_start_tick;
    }
    *min = (total_ms / 60000) % 60;
    *sec = (total_ms / 1000) % 60;
    *ms_100 = (total_ms / 100) % 10;
}

void UI_DrawPage(PageID_t page) {
    SSH1106_Clear();
    switch (page) {
        case PAGE_HOME:      UI_DrawHome(); break;
        case PAGE_STEPS:     UI_DrawSteps(); break;
        case PAGE_INFO:      UI_DrawInfo(); break;
        case PAGE_BLUETOOTH: UI_DrawBluetooth(); break;
        case PAGE_STOPWATCH: UI_DrawStopwatch(); break;
        default: break;
    }
    SSH1106_Refresh();
}

void UI_DrawHome(void) {
    SSH1106_FillRect(0, 0, 128, 10, 1);
    SSH1106_DrawString(30, 1, "Smart Watch", 0);
    SSH1106_DrawString(100, 1, FIRMWARE_VERSION, 0);

    char buf[32];
    sprintf(buf, "%02d:%02d:%02d", g_watch_time.hour, g_watch_time.min, g_watch_time.sec);
    SSH1106_DrawString(35, 15, buf, 1);

    sprintf(buf, "Steps: %lu", ui_steps);
    SSH1106_DrawString(5, 30, buf, 1);

    int temp_int = (int)(ui_temp + 0.5f);
    sprintf(buf, "Temp: %d C", temp_int);
    SSH1106_DrawString(5, 45, buf, 1);

    // Debug: show PA1 raw state
    GPIO_PinState pa1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    sprintf(buf, "P:%d", pa1 == GPIO_PIN_RESET ? 0 : 1);
    SSH1106_DrawString(95, 45, buf, 1);

    SSH1106_DrawRect(0, 55, 128, 9, 1);
    SSH1106_DrawString(50, 56, "[1/5]", 1);
}

// Steps page: compact layout to avoid overlap with bottom bar
void UI_DrawSteps(void) {
    SSH1106_FillRect(0, 0, 128, 10, 1);
    SSH1106_DrawString(35, 1, "Step Counter", 0);

    char buf[32];
    sprintf(buf, "Steps: %lu", ui_steps);
    SSH1106_DrawString(10, 16, buf, 1);

    uint8_t bar_width = (ui_steps > 10000) ? 100 : (ui_steps / 100);
    SSH1106_DrawRect(10, 28, 100, 6, 1);
    SSH1106_FillRect(11, 29, bar_width, 4, 1);

    sprintf(buf, "Goal: 10000");
    SSH1106_DrawString(10, 40, buf, 1);

    SSH1106_DrawRect(0, 55, 128, 9, 1);
    SSH1106_DrawString(50, 56, "[2/5]", 1);
}

void UI_DrawInfo(void) {
    SSH1106_FillRect(0, 0, 128, 10, 1);
    SSH1106_DrawString(40, 1, "Sensor Info", 0);

    char buf[32];
    sprintf(buf, "AX: %d", ui_ax);
    SSH1106_DrawString(5, 12, buf, 1);
    sprintf(buf, "AY: %d", ui_ay);
    SSH1106_DrawString(5, 22, buf, 1);
    sprintf(buf, "AZ: %d", ui_az);
    SSH1106_DrawString(5, 32, buf, 1);

    sprintf(buf, "MAX: %ld", g_step_mag);
    SSH1106_DrawString(5, 42, buf, 1);
    sprintf(buf, "CNT: %ld", g_step_x_hat);
    SSH1106_DrawString(65, 42, buf, 1);

    SSH1106_DrawRect(0, 55, 128, 9, 1);
    SSH1106_DrawString(50, 56, "[3/5]", 1);
}

void UI_DrawBluetooth(void) {
    SSH1106_FillRect(0, 0, 128, 10, 1);
    SSH1106_DrawString(30, 1, "BT Debug", 0);

    BT_Status_t status = Bluetooth_GetStatus();
    const char* status_str;
    switch (status) {
        case BT_STATUS_INIT: status_str = "Init"; break;
        case BT_STATUS_READY: status_str = "Rdy"; break;
        case BT_STATUS_RX_ACTIVE: status_str = "ACT"; break;
        case BT_STATUS_ERROR: status_str = "Err"; break;
        default: status_str = "???";
    }

    uint32_t rx_count = Bluetooth_GetRxCount();
    char buf[32];

    sprintf(buf, "RX:%lu ST:%s", rx_count, status_str);
    SSH1106_DrawString(0, 12, buf, 1);

    uint32_t last_rx = Bluetooth_GetLastRxTime();
    if (rx_count > 0) {
        uint32_t elapsed = (HAL_GetTick() - last_rx) / 1000;
        sprintf(buf, "T+%lus ago", elapsed);
    } else {
        sprintf(buf, "No RX yet");
    }
    SSH1106_DrawString(0, 22, buf, 1);

    extern char last_rx_debug[];
    extern uint8_t last_rx_debug_len;

    SSH1106_DrawString(0, 32, "HEX:", 1);

    if (last_rx_debug_len > 0) {
        int n = (last_rx_debug_len > 6) ? 6 : last_rx_debug_len;
        int idx = 0;
        for (int i = 0; i < n; i++) {
            uint8_t b = (uint8_t)last_rx_debug[i];
            idx += sprintf(buf + idx, "%02X", b);
        }
        buf[idx] = '\0';
        SSH1106_DrawString(25, 32, buf, 1);
    }

    if (last_rx_debug_len > 6) {
        int n = (last_rx_debug_len > 12) ? 12 : last_rx_debug_len;
        int idx = 0;
        for (int i = 6; i < n; i++) {
            uint8_t b = (uint8_t)last_rx_debug[i];
            idx += sprintf(buf + idx, "%02X", b);
        }
        buf[idx] = '\0';
        SSH1106_DrawString(0, 42, buf, 1);
    }

    if (last_rx_debug_len > 0) {
        int n = (last_rx_debug_len > 10) ? 10 : last_rx_debug_len;
        int idx = 0;
        for (int i = 0; i < n; i++) {
            uint8_t b = (uint8_t)last_rx_debug[i];
            if (b >= 32 && b <= 126) {
                buf[idx++] = (char)b;
            } else {
                buf[idx++] = '.';
            }
        }
        buf[idx] = '\0';
        SSH1106_DrawString(0, 52, buf, 1);
    }

    SSH1106_DrawRect(0, 62, 128, 2, 1);
    SSH1106_DrawString(50, 56, "[4/5]", 1);
}

// Stopwatch page: hint merged with status to avoid bottom bar overlap
void UI_DrawStopwatch(void) {
    SSH1106_FillRect(0, 0, 128, 10, 1);
    SSH1106_DrawString(35, 1, "Stopwatch", 0);

    uint8_t min, sec, ms_100;
    UI_GetStopwatch(&min, &sec, &ms_100);

    char buf[32];
    // Big time display: MM:SS.d
    sprintf(buf, "%02d:%02d.%d", min, sec, ms_100);
    SSH1106_DrawString(25, 22, buf, 1);

    // Status line with hint (single line, y=40)
    if (UI_IsStopwatchRunning()) {
        SSH1106_DrawString(5, 40, "RUN Press=Pause", 1);
    } else if (min > 0 || sec > 0) {
        SSH1106_DrawString(5, 40, "PAU Press=Resume", 1);
    } else {
        SSH1106_DrawString(5, 40, "RDY Press=Start", 1);
    }

    SSH1106_DrawRect(0, 55, 128, 9, 1);
    SSH1106_DrawString(50, 56, "[5/5]", 1);
}
