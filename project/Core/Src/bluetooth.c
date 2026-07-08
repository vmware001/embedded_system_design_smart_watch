#include "bluetooth.h"
#include "mpu6050.h"
#include "ssh1106.h"
#include <string.h>
#include <stdio.h>

static UART_HandleTypeDef *bt_uart;
static uint8_t rx_buffer[BT_BUFFER_SIZE];
static uint8_t rx_head = 0;
static uint8_t rx_tail = 0;
static uint8_t rx_data[1];
static BT_Status_t bt_status = BT_STATUS_INIT;
static uint32_t bt_last_rx_time = 0;
static uint32_t bt_rx_count = 0;

// Debug: store last received bytes for display
char last_rx_debug[32] = {0};
uint8_t last_rx_debug_len = 0;

volatile WatchTime_t g_watch_time = {0, 0, 0};

void Bluetooth_Init(UART_HandleTypeDef *huart) {
    bt_uart = huart;
    rx_head = 0;
    rx_tail = 0;
    bt_status = BT_STATUS_READY;
    bt_last_rx_time = 0;
    bt_rx_count = 0;
    memset(last_rx_debug, 0, sizeof(last_rx_debug));
    last_rx_debug_len = 0;
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(bt_uart, rx_data, 1);
    if (status != HAL_OK) {
        bt_status = BT_STATUS_ERROR;
    }
}

BT_Status_t Bluetooth_GetStatus(void) {
    return bt_status;
}

uint32_t Bluetooth_GetLastRxTime(void) {
    return bt_last_rx_time;
}

uint32_t Bluetooth_GetRxCount(void) {
    return bt_rx_count;
}

void Bluetooth_Process(void) {
    __disable_irq();
    uint8_t available = (rx_head >= rx_tail) ? (rx_head - rx_tail) : (BT_BUFFER_SIZE - rx_tail + rx_head);
    __enable_irq();
    
    if (available > 0 && bt_status != BT_STATUS_ERROR) {
        bt_status = BT_STATUS_RX_ACTIVE;
    }
    (void)available;
}

uint8_t Bluetooth_DataAvailable(void) {
    __disable_irq();
    uint8_t available = (rx_head >= rx_tail) ? (rx_head - rx_tail) : (BT_BUFFER_SIZE - rx_tail + rx_head);
    __enable_irq();
    return available;
}

void Bluetooth_GetData(uint8_t *data, uint8_t *len) {
    *len = 0;
    __disable_irq();
    while (rx_tail != rx_head && *len < BT_BUFFER_SIZE - 1) {
        data[(*len)++] = rx_buffer[rx_tail++];
        if (rx_tail >= BT_BUFFER_SIZE) rx_tail = 0;
    }
    __enable_irq();
}

void Bluetooth_Send(const uint8_t *data, uint8_t len) {
    HAL_UART_Transmit(bt_uart, (uint8_t*)data, len, 100);
}

void Bluetooth_SendString(const char *str) {
    // HC-05 v3.0/v4.0 firmware bug: buffers up to 230 bytes before sending
    // Workaround: pad every transmission to 230 bytes with nulls
    uint8_t buf[230];
    uint16_t len = strlen(str);
    
    if (len > 230) len = 230;
    memcpy(buf, str, len);
    // Pad with zeros to 230 bytes (module ignores trailing nulls)
    for (uint16_t i = len; i < 230; i++) {
        buf[i] = 0x00;
    }
    HAL_UART_Transmit(bt_uart, buf, 230, 500);
}

void WatchTime_Sync(uint8_t h, uint8_t m, uint8_t s) {
    g_watch_time.hour = h;
    g_watch_time.min = m;
    g_watch_time.sec = s;
}

void WatchTime_Tick(void) {
    if (++g_watch_time.sec >= 60) {
        g_watch_time.sec = 0;
        if (++g_watch_time.min >= 60) {
            g_watch_time.min = 0;
            if (++g_watch_time.hour >= 24) {
                g_watch_time.hour = 0;
            }
        }
    }
}

void Bluetooth_ProcessCommand(uint8_t *data, uint8_t len) {
    data[len] = '\0';
    char *str = (char*)data;
    
    // 忽略太短的命令（模块上电噪声）
    if (len < 3) {
        return;
    }
    
    // 查找 "SYNC," 并解析时间
    char *sync_pos = strstr(str, "SYNC,");
    if (sync_pos != NULL) {
        int h = 0, m = 0, s = 0;
        if (sscanf(sync_pos + 5, "%d:%d:%d", &h, &m, &s) == 3) {
            if (h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
                WatchTime_Sync((uint8_t)h, (uint8_t)m, (uint8_t)s);
                return;
            }
        }
        return;  // SYNC 解析失败，忽略
    }
    
    if (strstr(str, "STEP") != NULL) {
        char buf[32];
        sprintf(buf, "STEP:%lu\r\n", StepCounter_GetCount());
        Bluetooth_SendString(buf);
    }
    else if (strstr(str, "TEMP") != NULL) {
        char buf[32];
        sprintf(buf, "TEMP:%.1f\r\n", g_mpu_temp);
        Bluetooth_SendString(buf);
    }
    else if (strstr(str, "TIME") != NULL) {
        char buf[32];
        sprintf(buf, "TIME:%02d:%02d:%02d\r\n", g_watch_time.hour, g_watch_time.min, g_watch_time.sec);
        Bluetooth_SendString(buf);
    }
    else if (strstr(str, "SWSTART") != NULL) {
        extern void UI_StopwatchToggle(void);
        UI_StopwatchToggle();
    }
    else if (strstr(str, "SWSTOP") != NULL) {
        extern void UI_StopwatchToggle(void);
        UI_StopwatchToggle();
    }
    else if (strstr(str, "SWRESET") != NULL) {
        extern void UI_StopwatchReset(void);
        UI_StopwatchReset();
    }
    else if (strstr(str, "DEBUG") != NULL) {
        // 调试命令：返回最近接收到的原始数据（十六进制）
        char buf[128];
        int idx = 0;
        idx += sprintf(buf + idx, "DEBUG:cnt=%lu status=%d\r\n", (unsigned long)bt_rx_count, (int)bt_status);
        idx += sprintf(buf + idx, "RAW HEX:");
        __disable_irq();
        uint8_t t = rx_tail;
        uint8_t h = rx_head;
        __enable_irq();
        for (int i = 0; i < 16 && t != h; i++) {
            uint8_t ch = rx_buffer[t];
            t = (t + 1) % BT_BUFFER_SIZE;
            idx += sprintf(buf + idx, " %02X", ch);
        }
        idx += sprintf(buf + idx, "\r\n");
        Bluetooth_SendString(buf);
    }
    // 未知命令直接忽略，不修改时间
}

// ======== HC-05 一次性 AT 配置 ========
// 使用方法：上电前按住 HC-05 模块的 KEY/EN 引脚（接 3.3V），上电后自动进入 AT 模式
// 配置成功后会设置波特率为 115200、名称为 SmartWatch，然后恢复 UART 到 115200
static int AT_SendWait(const char *cmd) {
    uint8_t rx_buf[32];
    uint32_t start = HAL_GetTick();
    uint8_t idx = 0;

    HAL_UART_Transmit(bt_uart, (uint8_t*)cmd, strlen(cmd), 100);

    while (HAL_GetTick() - start < 800) {
        uint8_t ch;
        if (HAL_UART_Receive(bt_uart, &ch, 1, 10) == HAL_OK) {
            if (idx < sizeof(rx_buf) - 1) {
                rx_buf[idx++] = ch;
                rx_buf[idx] = '\0';
            }
            if (ch == '\n' && strstr((char*)rx_buf, "OK") != NULL) {
                return 1;
            }
        }
    }
    return (strstr((char*)rx_buf, "OK") != NULL) ? 1 : 0;
}

void Bluetooth_ATConfig(void) {
    // 切换到 AT 模式固定波特率 38400
    HAL_UART_DeInit(bt_uart);
    bt_uart->Init.BaudRate = 38400;
    bt_uart->Init.WordLength = UART_WORDLENGTH_8B;
    bt_uart->Init.StopBits = UART_STOPBITS_1;
    bt_uart->Init.Parity = UART_PARITY_NONE;
    bt_uart->Init.Mode = UART_MODE_TX_RX;
    bt_uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    bt_uart->Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(bt_uart);

    SSH1106_Clear();
    SSH1106_DrawString(0, 0, "BT AT Config...", 1);
    SSH1106_Refresh();
    HAL_Delay(2000);  // 等 HC-05 完全进入 AT 模式

    // 测试 AT 连接
    int ok = AT_SendWait("AT\r\n");
    SSH1106_DrawString(0, 12, ok ? "AT: OK" : "AT: NO RESP", 1);
    SSH1106_Refresh();
    if (!ok) {
        SSH1106_DrawString(0, 24, "Hold KEY + PwrOn", 1);
        SSH1106_DrawString(0, 36, "then retry", 1);
        SSH1106_Refresh();
        HAL_Delay(3000);
        goto restore;
    }
    HAL_Delay(200);

    // 设置波特率 115200
    ok = AT_SendWait("AT+UART=115200,0,0\r\n");
    SSH1106_DrawString(0, 22, ok ? "Baud: OK" : "Baud: FAIL", 1);
    SSH1106_Refresh();
    if (!ok) { HAL_Delay(1000); goto restore; }
    HAL_Delay(200);

    // 设置蓝牙名称
    ok = AT_SendWait("AT+NAME=SmartWatch\r\n");
    SSH1106_DrawString(0, 32, ok ? "Name: OK" : "Name: FAIL", 1);
    SSH1106_Refresh();
    if (!ok) { HAL_Delay(1000); goto restore; }
    HAL_Delay(200);

    SSH1106_DrawString(0, 44, "OK! Release KEY", 1);
    SSH1106_DrawString(0, 54, "Rebooting BT...", 1);
    SSH1106_Refresh();
    HAL_Delay(2000);

restore:
    // 恢复 UART 到 115200
    HAL_UART_DeInit(bt_uart);
    bt_uart->Init.BaudRate = 115200;
    bt_uart->Init.WordLength = UART_WORDLENGTH_8B;
    bt_uart->Init.StopBits = UART_STOPBITS_1;
    bt_uart->Init.Parity = UART_PARITY_NONE;
    bt_uart->Init.Mode = UART_MODE_TX_RX;
    bt_uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    bt_uart->Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(bt_uart);
    HAL_Delay(1000);
}

// UART Rx Complete Callback - called from ISR
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == bt_uart->Instance) {
        uint8_t ch = rx_data[0];
        rx_buffer[rx_head++] = ch;
        if (rx_head >= BT_BUFFER_SIZE) rx_head = 0;
        bt_last_rx_time = HAL_GetTick();
        bt_rx_count++;
        bt_status = BT_STATUS_RX_ACTIVE;
        
        // Record last received bytes for OLED debug display
        if (last_rx_debug_len < sizeof(last_rx_debug) - 1) {
            last_rx_debug[last_rx_debug_len++] = ch;
        } else {
            // Shift buffer left, drop oldest byte
            for (int i = 0; i < (int)sizeof(last_rx_debug) - 2; i++) {
                last_rx_debug[i] = last_rx_debug[i + 1];
            }
            last_rx_debug[sizeof(last_rx_debug) - 2] = ch;
        }
        
        HAL_UART_Receive_IT(bt_uart, rx_data, 1);
    }
}
