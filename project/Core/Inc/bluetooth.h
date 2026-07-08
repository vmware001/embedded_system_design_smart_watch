#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f1xx_hal.h"

#define BT_BUFFER_SIZE  64

typedef enum {
    BT_STATUS_INIT = 0,
    BT_STATUS_READY,
    BT_STATUS_RX_ACTIVE,
    BT_STATUS_ERROR
} BT_Status_t;

typedef struct {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} WatchTime_t;

extern volatile WatchTime_t g_watch_time;

void Bluetooth_Init(UART_HandleTypeDef *huart);
void Bluetooth_Process(void);
uint8_t Bluetooth_DataAvailable(void);
void Bluetooth_GetData(uint8_t *data, uint8_t *len);
void Bluetooth_Send(const uint8_t *data, uint8_t len);
void Bluetooth_SendString(const char *str);
BT_Status_t Bluetooth_GetStatus(void);
uint32_t Bluetooth_GetLastRxTime(void);
uint32_t Bluetooth_GetRxCount(void);

// Time sync
void WatchTime_Sync(uint8_t h, uint8_t m, uint8_t s);
void WatchTime_Tick(void);

// HC-05 one-time AT configuration: hold module KEY pin during power-on
void Bluetooth_ATConfig(void);

// Command processor
void Bluetooth_ProcessCommand(uint8_t *data, uint8_t len);

// Debug: store last received data for display
extern char last_rx_debug[32];
extern uint8_t last_rx_debug_len;

// Protocol commands (simple text-based)
#define BT_CMD_SYNC_TIME    "SYNC"
#define BT_CMD_GET_STEPS    "STEP"
#define BT_CMD_GET_TEMP     "TEMP"
#define BT_CMD_ACK_OK       "OK"
#define BT_CMD_ACK_ERR      "ERR"

#endif
