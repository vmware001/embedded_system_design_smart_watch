/*
 * bluetooth.h
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */

#ifndef INC_BLUETOOTH_H_
#define INC_BLUETOOTH_H_

#include <stdint.h>      // 提供uint8_t、uint32_t等标准类型
#include "mpu6050.h"    // 引入MPU6050_Data结构体定义

// 自定义数据帧
#define FRAME_STX   0xAA
#define FRAME_ETX   0x55

// 指令码
#define CMD_SYNC_TIME   0x01
#define CMD_STEP_DATA   0x02
#define CMD_SENSOR_DATA 0x03
#define CMD_SET_ALARM   0x04
#define CMD_DEV_STATUS  0x05

// 设备状态码（补充）
#define DEV_STATUS_OK       0x00    // 设备运行正常
#define DEV_STATUS_MPU_ERR  0x01    // MPU6050异常
#define DEV_STATUS_BUSY     0x02    // 设备忙碌

// 环形接收缓冲区
#define BT_RX_BUF_SIZE  256
extern uint8_t bt_rx_buf[BT_RX_BUF_SIZE];

void Bluetooth_Init(void);
void Bluetooth_SendSensorData(MPU6050_Data *data);
void Bluetooth_SendStepData(uint32_t steps);
void Bluetooth_SendDevStatus(uint8_t status);  // 补充：设备状态上报
void Bluetooth_ProcessReceived(void);


#endif /* INC_BLUETOOTH_H_ */
