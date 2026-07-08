#include "mpu6050.h"
#include <math.h>

/* ---- 内部辅助函数 ---- */

static HAL_StatusTypeDef MPU6050_WriteReg(uint8_t reg, uint8_t val) {
    HAL_StatusTypeDef status;
    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);
    status = HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, HAL_MAX_DELAY);
    return status;
}

static HAL_StatusTypeDef MPU6050_ReadReg(uint8_t reg, uint8_t *val) {
    HAL_StatusTypeDef status;
    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);
    status = HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, val, 1, HAL_MAX_DELAY);
    return status;  /* 返回 I2C 通信状态, 寄存器值通过 *val 传出 */
}

static HAL_StatusTypeDef MPU6050_ReadRegs(uint8_t reg, uint8_t *buf, uint16_t len) {
    HAL_StatusTypeDef status;
    while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY);
    status = HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR << 1, reg, I2C_MEMADD_SIZE_8BIT, buf, len, HAL_MAX_DELAY);
    return status;
}

/* ---- 互补滤波状态 ---- */
static float gyro_angle_x = 0.0f;
static float gyro_angle_y = 0.0f;

/* 陀螺仪零偏 (校准后减去) */
static float gyro_bias_x = 0.0f;
static float gyro_bias_y = 0.0f;
static float gyro_bias_z = 0.0f;

/* 诊断用: 最后一次读到的 WHO_AM_I 值 */
uint8_t mpu6050_who_am_i = 0;

/* ======== 初始化 ======== */
/* 返回 HAL_OK = 成功, HAL_ERROR = 失败 */

HAL_StatusTypeDef MPU6050_Init(void) {
    uint8_t who_am_i;
    HAL_StatusTypeDef status;

    /* 不复位设备，直接唤醒 (避免复位后 I2C 总线异常) */

    /* 1. 清除 SLEEP 位，唤醒设备 (时钟源=内部8MHz) */
    status = MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
    if (status != HAL_OK) {
        /* ---- I2C 总线恢复 ---- */
        HAL_I2C_DeInit(&hi2c1);
        HAL_Delay(50);
        HAL_I2C_Init(&hi2c1);
        HAL_Delay(100);
        /* 重试唤醒 */
        status = MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
        if (status != HAL_OK) return HAL_ERROR;
    }
    HAL_Delay(100);  /* 等待内部振荡器稳定 */

    /* 2. 重试读取 WHO_AM_I (最多 5 次, 每次间隔 100ms) */
    /* 注意: 部分国产/克隆模块 WHO_AM_I 返回 0x70 而非 0x68 */
    for (uint8_t retry = 0; retry < 5; retry++) {
        status = MPU6050_ReadReg(MPU6050_WHO_AM_I, &who_am_i);
        mpu6050_who_am_i = who_am_i;  /* 记录诊断值 */
        if (status == HAL_OK && (who_am_i == 0x68 || who_am_i == 0x70)) {
            break;
        }
        HAL_Delay(100);
    }

    if (status != HAL_OK || (who_am_i != 0x68 && who_am_i != 0x70)) {
        /* 最后一次尝试: 复位 I2C 总线后重试 */
        HAL_I2C_DeInit(&hi2c1);
        HAL_Delay(50);
        HAL_I2C_Init(&hi2c1);
        HAL_Delay(100);
        MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
        HAL_Delay(100);
        for (uint8_t retry = 0; retry < 5; retry++) {
            status = MPU6050_ReadReg(MPU6050_WHO_AM_I, &who_am_i);
            mpu6050_who_am_i = who_am_i;
            if (status == HAL_OK && (who_am_i == 0x68 || who_am_i == 0x70)) {
                break;
            }
            HAL_Delay(100);
        }
        if (status != HAL_OK || (who_am_i != 0x68 && who_am_i != 0x70)) {
            return HAL_ERROR;
        }
    }

    /* 3. 采样率分频: 200Hz (1kHz / (1+4) = 200Hz) */
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 4);

    /* 4. 数字低通滤波器: DLPF 42Hz */
    MPU6050_WriteReg(MPU6050_CONFIG, 0x03);

    /* 5. 加速度计量程: ±2g */
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);

    /* 6. 陀螺仪量程: ±250°/s */
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x08);

    /* 重置滤波状态 */
    gyro_angle_x = 0.0f;
    gyro_angle_y = 0.0f;

    /* ---- 陀螺仪零偏校准 (需保持板子静止) ---- */
    gyro_bias_x = 0.0f;
    gyro_bias_y = 0.0f;
    gyro_bias_z = 0.0f;
    {
        uint8_t cal_buf[6];
        float sum_x = 0, sum_y = 0, sum_z = 0;
        for (uint16_t i = 0; i < 200; i++) {
            MPU6050_ReadRegs(MPU6050_GYRO_XOUT_H, cal_buf, 6);
            sum_x += (int16_t)((cal_buf[0] << 8) | cal_buf[1]) / 65.5f;
            sum_y += (int16_t)((cal_buf[2] << 8) | cal_buf[3]) / 65.5f;
            sum_z += (int16_t)((cal_buf[4] << 8) | cal_buf[5]) / 65.5f;
            HAL_Delay(5);
        }
        gyro_bias_x = sum_x / 200.0f;
        gyro_bias_y = sum_y / 200.0f;
        gyro_bias_z = sum_z / 200.0f;
    }

    return HAL_OK;  /* 初始化成功 */
}

/* ======== 读取全部数据 + 互补滤波 ======== */

void MPU6050_ReadAll(MPU6050_Data *data) {
    uint8_t buf[14];
    int16_t raw_accel_x, raw_accel_y, raw_accel_z;
    int16_t raw_gyro_x, raw_gyro_y, raw_gyro_z;
    int16_t raw_temp;

    /* 从 ACCEL_XOUT_H (0x3B) 开始连续读取 14 字节 */
    MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, buf, 14);

    /* 合并高/低字节为 16 位有符号数 */
    raw_accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    raw_accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    raw_accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    raw_temp    = (int16_t)((buf[6] << 8) | buf[7]);
    raw_gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    raw_gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    raw_gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

    /* 转换为物理量 */
    /* 加速度: ±2g 量程, 灵敏度 16384 LSB/g */
    data->accel_x = raw_accel_x / 16384.0f;
    data->accel_y = raw_accel_y / 16384.0f;
    data->accel_z = raw_accel_z / 16384.0f;

    /* 陀螺仪: ±500°/s 量程, 灵敏度 65.5 LSB/(°/s), 减去零偏 */
    data->gyro_x = raw_gyro_x / 65.5f - gyro_bias_x;
    data->gyro_y = raw_gyro_y / 65.5f - gyro_bias_y;
    data->gyro_z = raw_gyro_z / 65.5f - gyro_bias_z;
		
    /* 温度: 公式 T = raw/340.0 + 36.53 */
    data->temp = raw_temp / 340.0f + 36.53f;

    /* ================= 互补滤波 (误差比例修正) ================= */

    /* 1. 计算时间步长 */
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();
    float dt = (now - last_tick) / 1000.0f;
    last_tick = now;
    if (dt > 0.1f || dt <= 0.0f) dt = 0.01f;

    /* 2. 加速度计合力量: 静止时应 ≈ 1g */
    float accel_mag = sqrtf(data->accel_x * data->accel_x +
                            data->accel_y * data->accel_y +
                            data->accel_z * data->accel_z);

    /* 3. 仅在加速度计可靠时修正 (静止/缓慢运动时 mag ≈ 1g) */
    /*    运动/甩动时 mag 偏离 1g, 加速度计角度不可信, 纯用陀螺仪积分 */
    if (accel_mag > 0.9f && accel_mag < 1.1f) {
        /* 加速度计计算绝对角度 */
        float accel_pitch = atan2f(-data->accel_x,
            sqrtf(data->accel_y * data->accel_y + data->accel_z * data->accel_z)) * 57.2958f;
        float accel_roll  = atan2f(data->accel_y, data->accel_z) * 57.2958f;

        /* 误差 = 陀螺仪积分角 - 加速度计角 */
        float error_pitch = gyro_angle_x - accel_pitch;
        float error_roll  = gyro_angle_y - accel_roll;

        /* 比例修正: 每周期修正误差的 3%, 拉向加速度计角度 */
        /* pitch 对应 Y 轴旋转 → gyro_y;  roll 对应 X 轴旋转 → gyro_x */
        gyro_angle_x += (data->gyro_y - error_pitch * 0.03f) * dt;
        gyro_angle_y += (data->gyro_x - error_roll  * 0.03f) * dt;
    } else {
        /* 运动期间: 仅陀螺仪积分, 不修正 */
        gyro_angle_x += data->gyro_y * dt;
        gyro_angle_y += data->gyro_x * dt;
    }

    data->pitch = gyro_angle_x;
    data->roll  = gyro_angle_y;
    data->yaw  += data->gyro_z * dt;
}

#define STEP_THRESHOLD   0.25f   /* 计步阈值 (g), 超过即视为一步 */
#define STEP_RESET       0.10f   /* 回退阈值, 信号低于此值才能再次计步 */
#define STEP_MIN_INTERVAL 250    /* 最小步间隔 (ms), 对应最高 4步/秒 */

/* 硬性底线：低于此值的晃动直接忽略，不计步，也不参与自适应学习 */
#define MIN_PEAK_THRESHOLD 0.20f  // 单位 g，建议 0.18 ~ 0.25，越大约抗干扰

void MPU6050_StepProcess(MPU6050_Data *data) {
    static float s_filtered = 0.0f;
    static float last_sig = 0.0f;
    static uint8_t rising = 0;          // 当前是否处于上升期
    static uint32_t last_step_time = 0;
    
    // 动态自适应阈值（记录前几次真实步数的波峰幅度）
    static float peak_history[5] = {0};
    static uint8_t history_idx = 0;

    /* 1. 去除重力后的动态加速度 */
    float a_mag = sqrtf(data->accel_x * data->accel_x + 
                        data->accel_y * data->accel_y + 
                        data->accel_z * data->accel_z) - 1.0f;

    /* 2. 一阶低通滤波 (α=0.2)，平滑噪声 */
    s_filtered = 0.8f * s_filtered + 0.2f * a_mag;

    float sig = fabsf(s_filtered);
    uint32_t now = HAL_GetTick();

    /* 3. 动态计算自适应阈值（取最近 5 次波峰平均值的 55%） */
    float dynamic_threshold = 0.25f; // 默认值
    float sum_peaks = 0;
    uint8_t valid_peaks = 0          ;
    for (int i = 0; i < 5; i++) {
        if (peak_history[i] > 0.1f) {
            sum_peaks += peak_history[i];
            valid_peaks++;
        }
    }
    if (valid_peaks >= 3) {
        dynamic_threshold = (sum_peaks / valid_peaks) * 0.522f;
        if (dynamic_threshold < 0.15f) dynamic_threshold = 0.15f;
        if (dynamic_threshold > 0.6f)  dynamic_threshold = 0.6f;
    }

    /* 4. 核心：检测加速度波峰的“峰值”（由升转降） */
    if (sig > last_sig) {
        rising = 1;  // 信号正在爬升
    } 
    else if (rising && sig < last_sig) {
        // 信号达到局部最高点，开始下降 → 检测到一个峰值
        rising = 0;
        
        /* --- 关键修改：增加硬性底线，过滤微小晃动 --- */
        if (last_sig > dynamic_threshold && 
            last_sig > MIN_PEAK_THRESHOLD &&          // 必须大于硬性底线
            (now - last_step_time) >= 250) {
            
            data->step_count++;
            last_step_time = now;

            // 将有效步数的波峰加入历史记录
            peak_history[history_idx] = last_sig;
            history_idx = (history_idx + 1) % 5;
        }
    }

    /* 5. 防误触：当信号特别小（完全静止）时，强制重置状态 */
    if (sig < 0.05f) {
        rising = 0;
    }

    last_sig = sig;
}