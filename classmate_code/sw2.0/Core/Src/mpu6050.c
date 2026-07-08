/*
 * mpu6050.c
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */


#include "mpu6050.h"
#include "stm32f1xx_hal.h"  // 根据你的芯片型号修改（如F103C8T6对应f1，F4系列对应f4）
#include <math.h>
#include "cmsis_os.h"


// 声明你的I2C外设句柄，根据实际使用的I2C修改（比如hi2c1、hi2c2）
extern I2C_HandleTypeDef hi2c1;
extern osMutexId mutexI2CHandle;  // 引用 freertos.c 创建的互斥锁

uint8_t MPU6050_Init(void)
{
    uint8_t who_am_i;
    uint8_t reg_val;

    /* Note: mutex not used here — called before scheduler starts, no concurrency */

    // 1. 复位设备：向PWR_MGMT_1写入0x80
    reg_val = 0x80;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_PWR_MGMT_1, 1, &reg_val, 1, 100);

    // 2. 延时100ms等待复位完成
    HAL_Delay(100);

    // 3. 唤醒设备：向PWR_MGMT_1写入0x00，关闭睡眠模式
    reg_val = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_PWR_MGMT_1, 1, &reg_val, 1, 100);

    // 4. 设置采样率分频：SMPLRT_DIV = 4，采样率 = 1kHz/(4+1) = 200Hz
    reg_val = 4;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_SMPLRT_DIV, 1, &reg_val, 1, 100);

    // 5. 配置数字低通滤波DLPF：CONFIG = 0x03，带宽42Hz
    reg_val = 0x03;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_CONFIG, 1, &reg_val, 1, 100);

    // 6. 配置加速度计量程：ACCEL_CONFIG = 0x00，量程±2g
    reg_val = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_ACCEL_CONFIG, 1, &reg_val, 1, 100);

    // 7. 配置陀螺仪量程：GYRO_CONFIG = 0x00，量程±250°/s
    reg_val = 0x00;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, MPU6050_GYRO_CONFIG, 1, &reg_val, 1, 100);

    // 8. 读取WHO_AM_I寄存器，验证设备ID
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR << 1, MPU6050_WHO_AM_I, 1, &who_am_i, 1, 100);

    if(who_am_i == 0x68)
    {
        return 0;  // 验证通过，初始化成功
    }
    return 1;      // 验证失败，设备异常
}

void MPU6050_ReadAll(MPU6050_Data *data)
{
    uint8_t recv_buf[14];

    osMutexWait(mutexI2CHandle, osWaitForever);

    HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR << 1, MPU6050_ACCEL_XOUT_H, 1, recv_buf, 14, 100);

    osMutexRelease(mutexI2CHandle);

    // 原始数据转换（原有逻辑保留）
    int16_t accel_x_raw = (int16_t)((recv_buf[0] << 8) | recv_buf[1]);
    int16_t accel_y_raw = (int16_t)((recv_buf[2] << 8) | recv_buf[3]);
    int16_t accel_z_raw = (int16_t)((recv_buf[4] << 8) | recv_buf[5]);
    int16_t temp_raw    = (int16_t)((recv_buf[6] << 8) | recv_buf[7]);
    int16_t gyro_x_raw  = (int16_t)((recv_buf[8] << 8) | recv_buf[9]);
    int16_t gyro_y_raw  = (int16_t)((recv_buf[10] << 8) | recv_buf[11]);
    int16_t gyro_z_raw  = (int16_t)((recv_buf[12] << 8) | recv_buf[13]);

    data->accel_x = accel_x_raw / 16384.0f;
    data->accel_y = accel_y_raw / 16384.0f;
    data->accel_z = accel_z_raw / 16384.0f;
    data->gyro_x  = gyro_x_raw / 131.0f;
    data->gyro_y  = gyro_y_raw / 131.0f;
    data->gyro_z  = gyro_z_raw / 131.0f;
    data->temp    = temp_raw / 340.0f + 36.53f;

    // ===== 补充Pitch计算（核心修复）=====
    // 1. Roll（横滚角）：基于Y轴加速度、Z轴加速度 + X轴陀螺仪
    float roll_accel = atan2f(data->accel_y, data->accel_z) * 57.2958f;
    static float roll_gyro = 0;
    roll_gyro += data->gyro_x * 0.01f; // 0.01s = 1/200Hz采样周期
    data->roll = 0.98f * roll_gyro + 0.02f * roll_accel;

    // 2. Pitch（俯仰角）：基于X轴加速度、Z轴加速度 + Y轴陀螺仪
    float pitch_accel = atan2f(-data->accel_x, data->accel_z) * 57.2958f;
    static float pitch_gyro = 0;
    pitch_gyro += data->gyro_y * 0.01f;
    data->pitch = 0.98f * pitch_gyro + 0.02f * pitch_accel;

    // Yaw（偏航角）需磁力计辅助，此处暂赋值0（无磁力计则无法计算）
    data->yaw = 0.0f;
}



