#include "mpu6050.h"
#include "bluetooth.h"
#include <math.h>
#include <stdio.h>

static I2C_HandleTypeDef *mpu6050_i2c;
float g_mpu_temp = 0.0f;

static volatile uint32_t step_count = 0;      // confirmed steps (for display)
static volatile uint32_t step_last_time = 0;  // last confirmed step time

// Debug exports for UI display
volatile int32_t g_step_mag = 0;
volatile int32_t g_step_x_hat = 0;
volatile int8_t  g_step_ped_flag = 0;

// I2C error tracking
static uint32_t i2c_error_count = 0;

// ============================================
// Pedometer Algorithm v31: State Machine + Continuous Confirm
//
// Solves the "1-step false trigger" problem by using a state machine:
//   - IDLE: no activity
//   - PENDING: 1 step detected, waiting for confirmation
//   - WALKING: confirmed walking, all steps count
//
// Confirmation: 2 steps within 3 seconds = walking
// Timeout: 3 seconds without 2nd step = discard (minor shake)
//
// This means:
//   - Single accidental shake (1 step) → discarded after 3s
//   - Normal walking (2+ steps) → confirmed, all steps counted
//
// Combined with v30's fall-back + variance detection for robustness.
// ============================================
#define AVG_WINDOW          20
#define VAR_WINDOW          10
#define PEAK_DIFF           2000
#define VAR_THRESHOLD       800000
#define DEBOUNCE_MS         300

// State machine parameters
#define CONFIRM_WINDOW_MS   3000  // 3 seconds to confirm
#define MIN_CONFIRM_STEPS   2     // 2 steps = walking

static int32_t avg_buf[AVG_WINDOW];
static uint8_t avg_idx = 0;
static int32_t var_buf[VAR_WINDOW];
static uint8_t var_idx = 0;

// Step detection state
static int32_t last_mag = 0;
static int32_t last_avg = 0;
static int32_t last_peak = 0;
static uint8_t has_peaked = 0;

// State machine
static uint8_t walk_state = 0;     // 0=IDLE, 1=PENDING, 2=WALKING
static uint32_t pending_count = 0;  // steps in pending state
static uint32_t pending_start = 0; // time when pending started
static uint32_t last_detected = 0; // last time any step was detected

// ============================================
// I2C helpers
// ============================================
static uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        mpu6050_i2c, MPU6050_ADDR, buf, 2, 100);
    if (status != HAL_OK) { i2c_error_count++; return 0; }
    return 1;
}

static uint8_t MPU6050_ReadReg(uint8_t reg, uint8_t *data) {
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        mpu6050_i2c, MPU6050_ADDR, &reg, 1, 100);
    if (status != HAL_OK) { i2c_error_count++; return 0; }
    status = HAL_I2C_Master_Receive(mpu6050_i2c, MPU6050_ADDR, data, 1, 100);
    if (status != HAL_OK) { i2c_error_count++; return 0; }
    return 1;
}

static uint8_t MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len) {
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        mpu6050_i2c, MPU6050_ADDR, &reg, 1, 100);
    if (status != HAL_OK) { i2c_error_count++; return 0; }
    status = HAL_I2C_Master_Receive(mpu6050_i2c, MPU6050_ADDR, data, len, 100);
    if (status != HAL_OK) { i2c_error_count++; return 0; }
    return 1;
}

void MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    mpu6050_i2c = hi2c;
    HAL_Delay(100);
    MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x80);
    HAL_Delay(100);
    MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x00);
    HAL_Delay(10);
    MPU6050_WriteReg(MPU6050_REG_SMPLRT_DIV, 0x00);
    MPU6050_WriteReg(MPU6050_REG_CONFIG, 0x03);
    MPU6050_WriteReg(MPU6050_REG_GYRO_CONFIG, 0x00);
    MPU6050_WriteReg(MPU6050_REG_ACCEL_CONFIG, 0x00);
    uint8_t whoami = 0;
    MPU6050_ReadReg(MPU6050_REG_WHO_AM_I, &whoami);
    StepCounter_Init();
}

uint8_t MPU6050_ReadWhoAmI(void) {
    uint8_t data = 0;
    MPU6050_ReadReg(MPU6050_REG_WHO_AM_I, &data);
    return data;
}

void MPU6050_ReadAccel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t buf[6];
    if (!MPU6050_ReadRegs(MPU6050_REG_ACCEL_XOUT_H, buf, 6)) return;
    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
    *az = (int16_t)((buf[4] << 8) | buf[5]);
}

void MPU6050_ReadGyro(int16_t *gx, int16_t *gy, int16_t *gz) {
    uint8_t buf[6];
    if (!MPU6050_ReadRegs(MPU6050_REG_GYRO_XOUT_H, buf, 6)) return;
    *gx = (int16_t)((buf[0] << 8) | buf[1]);
    *gy = (int16_t)((buf[2] << 8) | buf[3]);
    *gz = (int16_t)((buf[4] << 8) | buf[5]);
}

float MPU6050_ReadTemp(void) {
    uint8_t buf[2];
    if (!MPU6050_ReadRegs(MPU6050_REG_TEMP_OUT_H, buf, 2)) return g_mpu_temp;
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    g_mpu_temp = (raw / 340.0f) + 36.53f - 22.0f;
    return g_mpu_temp;
}

// ============================================
// Step Counter (v31): State Machine + Confirm
// ============================================
void StepCounter_Init(void) {
    step_count = 0;
    step_last_time = 0;
    last_mag = 0; last_avg = 0; last_peak = 0; has_peaked = 0;
    avg_idx = 0; var_idx = 0;
    for (int i = 0; i < AVG_WINDOW; i++) avg_buf[i] = 17000;
    for (int i = 0; i < VAR_WINDOW; i++) var_buf[i] = 17000;
    walk_state = 0;
    pending_count = 0;
    pending_start = 0;
    last_detected = 0;
}

void StepCounter_Update(int16_t ax, int16_t ay, int16_t az) {
    uint32_t now = HAL_GetTick();
    
    // Compute magnitude
    int64_t mag_sq = (int64_t)ax * ax + (int64_t)ay * ay + (int64_t)az * az;
    int32_t mag = (int32_t)sqrtf((float)mag_sq);
    
    // Update buffers
    var_buf[var_idx] = mag; var_idx = (var_idx + 1) % VAR_WINDOW;
    avg_buf[avg_idx] = mag; avg_idx = (avg_idx + 1) % AVG_WINDOW;
    
    int32_t avg = 0;
    for (int i = 0; i < AVG_WINDOW; i++) avg += avg_buf[i];
    avg /= AVG_WINDOW;
    
    int64_t variance = 0;
    for (int i = 0; i < VAR_WINDOW; i++) {
        int32_t diff = var_buf[i] - avg;
        variance += (int64_t)diff * diff;
    }
    variance /= VAR_WINDOW;
    
    g_step_mag = mag;
    g_step_x_hat = avg;
    
    // Track peak
    if (mag > last_peak) { last_peak = mag; has_peaked = 1; }
    
    // Fall-back detection (v30 logic)
    int32_t peak_drop = last_peak - mag;
    uint8_t crossed_below = (mag < avg) && (last_mag >= last_avg);
    uint8_t peak_big = (peak_drop >= PEAK_DIFF) && has_peaked;
    uint8_t variance_ok = (variance > VAR_THRESHOLD);
    uint8_t debounce_ok = (now - step_last_time) > DEBOUNCE_MS;
    
    uint8_t detected = crossed_below && peak_big && variance_ok && debounce_ok;
    
    // ============================================
    // STATE MACHINE: key innovation of v31
    // ============================================
    if (detected) {
        last_detected = now;
        
        if (walk_state == 0) {  // IDLE → PENDING
            walk_state = 1;
            pending_start = now;
            pending_count = 1;
        }
        else if (walk_state == 1) {  // PENDING
            pending_count++;
            if (pending_count >= MIN_CONFIRM_STEPS) {
                // CONFIRMED! Add pending steps to count
                step_count += pending_count;
                step_last_time = now;
                pending_count = 0;
                walk_state = 2;  // WALKING
                
                char dbg[64];
                sprintf(dbg, "CONFIRM! steps=%lu\r\n", step_count);
                Bluetooth_SendString(dbg);
            }
        }
        else if (walk_state == 2) {  // WALKING
            step_count++;
            step_last_time = now;
        }
    }
    
    // PENDING timeout: no 2nd step within 3 seconds → discard
    if (walk_state == 1 && (now - pending_start > CONFIRM_WINDOW_MS)) {
        walk_state = 0;
        pending_count = 0;
    }
    
    // WALKING timeout: no step for 2 seconds → back to IDLE
    if (walk_state == 2 && (now - last_detected > 2000)) {
        walk_state = 0;
    }
    
    // Safety: reset peak if stuck
    static uint32_t peak_start = 0;
    if (has_peaked && peak_start == 0) peak_start = now;
    if (has_peaked && (now - peak_start) > 2000) {
        last_peak = 0; has_peaked = 0; peak_start = 0;
    }
    if (!has_peaked) peak_start = 0;
    
    last_mag = mag;
    last_avg = avg;
}

uint32_t StepCounter_GetCount(void) {
    return step_count;  // only confirmed steps
}

void StepCounter_Reset(void) {
    step_count = 0;
    step_last_time = 0;
    last_mag = 0; last_avg = 0; last_peak = 0; has_peaked = 0;
    avg_idx = 0; var_idx = 0;
    for (int i = 0; i < AVG_WINDOW; i++) avg_buf[i] = 17000;
    for (int i = 0; i < VAR_WINDOW; i++) var_buf[i] = 17000;
    walk_state = 0;
    pending_count = 0;
    pending_start = 0;
    last_detected = 0;
}
