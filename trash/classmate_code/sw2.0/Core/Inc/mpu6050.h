/*
 * mpu6050.h
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include <stdint.h>  // 提供uint8_t等标准类型定义

// 设备地址
#define MPU6050_ADDR        0x68

// 寄存器地址
#define MPU6050_SMPLRT_DIV  0x19
#define MPU6050_CONFIG      0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_PWR_MGMT_1  0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_TEMP_OUT_H  0x41
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_WHO_AM_I    0x75

// 数据结构体
typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x,  gyro_y,  gyro_z;
    float temp;
    float pitch, roll, yaw;
} MPU6050_Data;

// 函数声明
uint8_t MPU6050_Init(void);
void MPU6050_ReadAll(MPU6050_Data *data);


#endif /* INC_MPU6050_H_ */
