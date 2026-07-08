#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f1xx_hal.h"

#define MPU6050_ADDR        0xD0  // 0x68 << 1

#define MPU6050_REG_WHO_AM_I    0x75
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H  0x43
#define MPU6050_REG_TEMP_OUT_H   0x41
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C

void MPU6050_Init(I2C_HandleTypeDef *hi2c);
uint8_t MPU6050_ReadWhoAmI(void);
void MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az);
void MPU6050_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz);
float MPU6050_ReadTemp(void);

// Global sensor data (updated by SensorTask)
extern float g_mpu_temp;

// Step counting (v27: fixed consecutive confirm)
void StepCounter_Init(void);
void StepCounter_Update(int16_t ax, int16_t ay, int16_t az);
uint32_t StepCounter_GetCount(void);
void StepCounter_Reset(void);

// Debug variables for UI display
extern volatile int32_t g_step_mag;      // Acceleration magnitude (for debug)
extern volatile int32_t g_step_x_hat;    // Confirm counter (for debug)
extern volatile int8_t  g_step_ped_flag; // Step detection flag (for debug)

#endif
