#ifndef __MPU6050_H
#define __MPU6050_H

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

/* MPU6050 I2C 地址 (7-bit) */
#define MPU6050_ADDR        0x68

/* 寄存器地址 */
#define MPU6050_PWR_MGMT_1  0x6B
#define MPU6050_SMPLRT_DIV  0x19
#define MPU6050_CONFIG      0x1A
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_TEMP_OUT_H  0x41
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_WHO_AM_I    0x75

/* 数据结构 */
typedef struct {
    float accel_x, accel_y, accel_z;   /* 加速度 (g) */
    float gyro_x,  gyro_y,  gyro_z;    /* 角速度 (°/s) */
    float temp;                         /* 温度 (°C) */
    float pitch, roll, yaw;            /* 姿态角 (°) */
    uint32_t step_count;                /* 步数 */
} MPU6050_Data;

/* 函数声明 */
HAL_StatusTypeDef MPU6050_Init(void);
void MPU6050_ReadAll(MPU6050_Data *data);
void MPU6050_StepProcess(MPU6050_Data *data);

/* 诊断用: 最后一次 WHO_AM_I 读数值 */
extern uint8_t mpu6050_who_am_i;

#endif
