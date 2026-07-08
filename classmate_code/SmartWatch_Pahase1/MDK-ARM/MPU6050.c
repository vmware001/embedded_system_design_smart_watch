#define MPU6050_ADDR    0x68

// ?????
#define MPU6050_PWR_MGMT_1     0x6B
#define MPU6050_ACCEL_XOUT_H   0x3B
#define MPU6050_TEMP_OUT_H     0x41
#define MPU6050_GYRO_XOUT_H    0x43
#define MPU6050_WHO_AM_I       0x75

typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x,  gyro_y,  gyro_z;
    float temp;
    float pitch, roll, yaw;
} MPU6050_Data;

uint8_t MPU6050_Init(void);


void MPU6050_ReadAll(MPU6050_Data *data) 
    uint8_t buf[14];
    int16_t raw_accel_x, raw_accel_y, raw_accel_z;
    int16_t raw_gyro_x, raw_gyro_y, raw_gyro_z;
    int16_t raw_temp;

    /* ? ACCEL_XOUT_H (0x3B) ?????? 14 ?? */
    MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, buf, 14);

    raw_accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    raw_accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    raw_accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    raw_temp    = (int16_t)((buf[6] << 8) | buf[7]);
    raw_gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    raw_gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    raw_gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

    /* ?????? */
    data->accel_x = raw_accel_x / 16384.0f;
    data->accel_y = raw_accel_y / 16384.0f;
    data->accel_z = raw_accel_z / 16384.0f;
    data->gyro_x  = raw_gyro_x / 131.0f - gyro_bias_x;
    data->gyro_y  = raw_gyro_y / 131.0f - gyro_bias_y;
    data->gyro_z  = raw_gyro_z / 131.0f - gyro_bias_z;
    data->temp    = raw_temp / 340.0f + 36.53f;

    /* ================= ???????? ================= */
    
    /* 1. ?????????????(?????? Pitch/Roll ???????) */
    float accel_pitch = atan2f(-data->accel_x, sqrtf(data->accel_y * data->accel_y + data->accel_z * data->accel_z)) * 57.2958f;
    float accel_roll  = atan2f(data->accel_y, data->accel_z) * 57.2958f;

    /* 2. ?????? */
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();
    float dt = (now - last_tick) / 1000.0f;
    last_tick = now;
    if (dt > 0.1f || dt <= 0.0f) dt = 0.01f;  /* ???????? */

    /* 3. ???????? */
    float accel_mag = sqrtf(data->accel_x * data->accel_x + 
                            data->accel_y * data->accel_y + 
                            data->accel_z * data->accel_z);

    /* 4. ?????????? alpha(????) 
       ??? alpha=0.98(???????);????? alpha ???? 0.995(?????????) */
    float alpha = 0.98f;
    float deviation = fabsf(accel_mag - 1.0f);
    if (deviation > 0.2f) {
        alpha = 0.995f;   // ????,???????,???? 0.5% ??????
    } else {
        // ????,????
        alpha = 0.98f + (deviation / 0.2f) * 0.015f;
    }
    if (alpha > 0.995f) alpha = 0.995f;
    if (alpha < 0.98f)  alpha = 0.98f;

    /* 5. ?????? */
    static float pitch_angle = 0.0f;
    static float roll_angle = 0.0f;
    pitch_angle = alpha * (pitch_angle + data->gyro_x * dt) + (1.0f - alpha) * accel_pitch;
    roll_angle  = alpha * (roll_angle  + data->gyro_y * dt) + (1.0f - alpha) * accel_roll;

    data->pitch = pitch_angle;
    data->roll  = roll_angle;
    
    /* Yaw ?????????(?????),?????,????? */
    data->yaw += data->gyro_z * dt;
}