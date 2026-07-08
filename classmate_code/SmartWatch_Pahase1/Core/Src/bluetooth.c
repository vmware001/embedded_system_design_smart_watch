#include "main.h"
#include "bluetooth.h"
#include "ssd1306.h"
#include "menu.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

/* DMA 循环接收缓冲 */
static uint8_t bt_rx_buf[BT_RX_BUF_SIZE];

/* DMA 读取位置追踪 */
static uint16_t bt_rx_read_pos = 0;
static uint8_t  bt_line_buf[20];  /* 行缓冲 (最大 T+14数字+null) */
static uint8_t  bt_line_idx = 0;

/* AT 指令接收缓冲 */
static uint8_t at_rx_buf[16];

/* ======== 发送 AT 指令并等待 OK 回复 ======== */
/* 逐字节轮询接收，收到 \n 时检查是否已有 "OK"，超时 500ms */
static int AT_SendWait(const char *cmd) {
    /* 发送指令 */
    HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 100);

    /* 逐字节接收 */
    memset(at_rx_buf, 0, sizeof(at_rx_buf));
    int idx = 0;
    uint32_t start = HAL_GetTick();

    while (HAL_GetTick() - start < 500) {
        uint8_t ch;
        if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK) {
            at_rx_buf[idx++] = ch;
            if (idx >= (int)(sizeof(at_rx_buf) - 1)) break;
            if (ch == '\n' && strstr((char *)at_rx_buf, "OK") != NULL) {
                return 1;
            }
        }
    }
    return (strstr((char *)at_rx_buf, "OK") != NULL) ? 1 : 0;
}

/* ======== 蓝牙 AT 配置 (首次使用，一次性) ======== */
/*
 * 使用方法:
 *   1. 按住 HC-05 的按键不放
 *   2. 给 STM32 + HC-05 上电
 *   3. OLED 显示配置进度
 *   4. 看到 "OK! Release BTN" 后松开按键
 *   5. HC-05 自动重启进入透传模式 (波特率 115200，名称 SmartWatch)
 */
void Bluetooth_ATConfig(void) {
    int ok;

    /* 1. 切换 USART1 到 38400 (HC-05 AT 模式固定波特率) */
    HAL_UART_DeInit(&huart1);
    huart1.Init.BaudRate = 38400;
    HAL_UART_Init(&huart1);

    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("BT AT Config...");
    SSD1306_Flush();
    HAL_Delay(2000);  /* 等 HC-05 完全进入 AT 模式 */

    /* 2. 测试 AT 连接 */
    ok = AT_SendWait("AT\r\n");
    SSD1306_SetCursor(0, 2);
    if (ok) {
        SSD1306_WriteString("AT: OK");
    } else {
        SSD1306_WriteString("AT: NO RESP");
        SSD1306_SetCursor(0, 4);
        SSD1306_WriteString("HoldKey+PwrOn");
        SSD1306_SetCursor(0, 6);
        SSD1306_WriteString("then retry");
        SSD1306_Flush();
        goto restore;
    }
    SSD1306_Flush();

    /* 3. 设置波特率 115200 */
    ok = AT_SendWait("AT+UART=115200,0,0\r\n");
    SSD1306_SetCursor(0, 3);
    SSD1306_WriteString(ok ? "Baud: OK" : "Baud: FAIL");
    if (!ok) { SSD1306_Flush(); goto restore; }
    SSD1306_Flush();

    /* 4. 设置蓝牙名称 */
    ok = AT_SendWait("AT+NAME=SmartWatch\r\n");
    SSD1306_SetCursor(0, 4);
    SSD1306_WriteString(ok ? "Name: OK" : "Name: FAIL");
    if (!ok) { SSD1306_Flush(); goto restore; }
    SSD1306_Flush();

    SSD1306_SetCursor(0, 6);
    SSD1306_WriteString("OK! Release BTN");
    SSD1306_Flush();

restore:
    /* 5. 恢复 USART1 到 115200 */
    HAL_UART_DeInit(&huart1);
    huart1.Init.BaudRate = 115200;
    HAL_UART_Init(&huart1);

    HAL_Delay(2000);
}

/* ======== 蓝牙初始化 (正常模式 DMA 接收) ======== */
void Bluetooth_Init(void) {
    /* 启动 DMA 循环接收 */
    HAL_UART_Receive_DMA(&huart1, bt_rx_buf, BT_RX_BUF_SIZE);
}

/* ======== 发送传感器数据 (文本格式) ======== */
/*
 * 输出格式 (每行约 80 字节):
 *   T:25.3 P:45.2 R:-10.1 AX:0.12 AY:0.34 AZ:0.98 GX:1.2 GY:-0.5 GZ:3.1 ST:1234
 */
void Bluetooth_SendSensorData(MPU6050_Data *data) {
    char buf[96];
    int len = snprintf(buf, sizeof(buf),
        "T:%.1f P:%.1f R:%.1f AX:%.2f AY:%.2f AZ:%.2f GX:%.1f GY:%.1f GZ:%.1f ST:%lu\r\n",
        data->temp, data->pitch, data->roll,
        data->accel_x, data->accel_y, data->accel_z,
        data->gyro_x, data->gyro_y, data->gyro_z,
        (unsigned long)data->step_count);
    if (len > 0 && len < (int)sizeof(buf)) {
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100);
    }
}

/* ======== 解析时间同步指令 (bt_line_buf 已填入整行) ======== */
/* 格式1: THHMMSS (7字节) / 格式2: TYYYYMMDDHHMMSS (15字节)      */
/* 返回 1=已同步, 0=无效                                         */
static uint8_t ParseTimeSyncCmd(void)
{
    if (bt_line_buf[0] != 'T') return 0;

    if (bt_line_idx == 7) {
        /* 旧格式: THHMMSS */
        uint8_t h, m, s;
        uint8_t i;
        for (i = 1; i <= 6; i++) {
            if (bt_line_buf[i] < '0' || bt_line_buf[i] > '9') return 0;
        }
        h = (bt_line_buf[1] - '0') * 10 + (bt_line_buf[2] - '0');
        m = (bt_line_buf[3] - '0') * 10 + (bt_line_buf[4] - '0');
        s = (bt_line_buf[5] - '0') * 10 + (bt_line_buf[6] - '0');
        if (h >= 24 || m >= 60 || s >= 60) return 0;
        g_time.hours   = h;
        g_time.minutes = m;
        g_time.seconds = s;
        g_time_synced  = 1;
        return 1;
    }

    if (bt_line_idx == 15) {
        /* 新格式: TYYYYMMDDHHMMSS */
        uint16_t yr;
        uint8_t mo, dy, h, m, s;
        uint8_t i;
        for (i = 1; i <= 14; i++) {
            if (bt_line_buf[i] < '0' || bt_line_buf[i] > '9') return 0;
        }
        yr = (uint16_t)(bt_line_buf[1] - '0') * 1000
           + (uint16_t)(bt_line_buf[2] - '0') * 100
           + (uint16_t)(bt_line_buf[3] - '0') * 10
           + (uint16_t)(bt_line_buf[4] - '0');
        mo = (bt_line_buf[5]  - '0') * 10 + (bt_line_buf[6]  - '0');
        dy = (bt_line_buf[7]  - '0') * 10 + (bt_line_buf[8]  - '0');
        h  = (bt_line_buf[9]  - '0') * 10 + (bt_line_buf[10] - '0');
        m  = (bt_line_buf[11] - '0') * 10 + (bt_line_buf[12] - '0');
        s  = (bt_line_buf[13] - '0') * 10 + (bt_line_buf[14] - '0');
        if (mo < 1 || mo > 12 || dy < 1 || dy > 31
            || h >= 24 || m >= 60 || s >= 60) return 0;
        g_time.year    = yr;
        g_time.month   = mo;
        g_time.day     = dy;
        g_time.hours   = h;
        g_time.minutes = m;
        g_time.seconds = s;
        g_time_synced  = 1;
        return 1;
    }

    return 0;
}

/* ======== 检查 DMA 缓冲区时间同步指令 ======== */
/* 手机蓝牙串口 App 发送 "T20260707143025" 同步年月日+时分秒         */
/* 每 100ms 由 taskBluetooth 调用一次                                */
__attribute__((optnone))
uint8_t Bluetooth_CheckRxCommand(void)
{
    uint16_t dma_cnt = __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    uint16_t dma_pos = BT_RX_BUF_SIZE - dma_cnt;
    if (dma_pos >= BT_RX_BUF_SIZE) dma_pos = 0;

    if (bt_rx_read_pos == dma_pos) return 0;

    uint8_t synced = 0;

    while (bt_rx_read_pos != dma_pos) {
        uint8_t ch = bt_rx_buf[bt_rx_read_pos];
        bt_rx_read_pos = (bt_rx_read_pos + 1) % BT_RX_BUF_SIZE;

        if (ch == '\n' || ch == '\r') {
            if (bt_line_idx > 0) {
                bt_line_buf[bt_line_idx] = '\0';
                if (ParseTimeSyncCmd()) synced = 1;
                bt_line_idx = 0;
            }
        } else if (bt_line_idx < sizeof(bt_line_buf) - 1) {
            bt_line_buf[bt_line_idx++] = ch;
        } else {
            bt_line_idx = 0;
        }
    }

    return synced;
}
