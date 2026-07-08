#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "main.h"
#include "mpu6050.h"

/* 帧协议定义 */
#define FRAME_STX          0xAA
#define FRAME_ETX          0x55

/* 命令字 */
#define CMD_SENSOR_DATA    0x01
#define CMD_STEP_COUNT     0x02
#define CMD_SYNC_TIME      0x81

/* DMA 接收缓冲区大小 */
#define BT_RX_BUF_SIZE     128
#define BT_TX_FRAME_SIZE   28

/* 帧结构: STX(1) + CMD(1) + LEN(1) + DATA[n] + CHK(1) + ETX(1) */

void Bluetooth_ATConfig(void);
void Bluetooth_Init(void);
void Bluetooth_SendSensorData(MPU6050_Data *data);

/* 检查 DMA 环形缓冲区是否有时间同步指令, 返回 1=已同步 */
uint8_t Bluetooth_CheckRxCommand(void);

#endif
