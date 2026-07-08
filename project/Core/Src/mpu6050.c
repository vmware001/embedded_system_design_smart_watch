#include "mpu6050.h"
#include "bluetooth.h"
#include <math.h>
#include <stdio.h>

static I2C_HandleTypeDef *mpu6050_i2c;
float g_mpu_temp = 0.0f;

static volatile uint32_t step_count = 0;
static volatile uint32_t step_last_time = 0;

// Debug exports for UI display
volatile int32_t g_step_mag = 0;      // 显示用：当前 sqrt(mag_sq) 的近似值
volatile int32_t g_step_x_hat = 0;    // 显示用：当前平均值的 sqrt 近似
volatile int8_t  g_step_ped_flag = 0;

static uint32_t i2c_error_count = 0;

// ============================================
// Pedometer Algorithm v34: Optimized
//
// Optimizations:
//   1. Eliminate sqrtf() — all comparisons use squared magnitude
//   2. O(1) moving average — maintain running sum, no loop
//   3. Auto-calibration — 2-second baseline calibration on startup
//   4. Periodic motion detection retained from v33
//
// All thresholds converted to squared units:
//   Original PEAK_DIFF = 1500  →  PEAK_DIFF_SQ ≈ 2*17000*1500 = 51,000,000
//   Original VAR_THRESHOLD = 500000 → VAR_THRESHOLD_SQ scaled accordingly
// ============================================
#define AVG_WINDOW          20
#define VAR_WINDOW          10
#define DEBOUNCE_MS         250

// Squared thresholds (eliminates sqrtf)
// PEAK_DIFF = 1500 (mag units) → PEAK_DIFF_SQ = 2 * BASE * PEAK_DIFF
// BASE ≈ 17000, so 2 * 17000 * 1500 = 51,000,000
#define PEAK_DIFF_SQ        51000000LL

// VAR_THRESHOLD = 500000 (mag units) → VAR_THRESHOLD_SQ
// Variance of mag_sq is approximately (2*BASE)^2 * variance_of_mag
// Approximation: VAR_THRESHOLD_SQ = 500000 * 500000 = 250e9... too big.
// Better: use relative variance on mag_sq directly.
// Empirical: typical walking mag_sq variance ≈ 1e9 ~ 5e9
#define VAR_THRESHOLD_SQ    2000000000LL  // 2e9, tuned empirically

// Periodicity detection
#define INTERVAL_HISTORY    5
#define MIN_INTERVAL        300
#define MAX_INTERVAL        1500
#define STABLE_COUNT        1

// Calibration
#define CALIB_SAMPLES       40  // 2 seconds at 20Hz

// Buffers store squared magnitude (int64_t)
static int64_t avg_buf[AVG_WINDOW];
static int64_t var_buf[VAR_WINDOW];
static uint8_t avg_idx = 0;
static uint8_t var_idx = 0;

// O(1) moving average: maintain running sum
static int64_t avg_sum = 0;

// Step detection state (all in squared units)
static int64_t last_mag_sq = 0;
static int64_t last_avg_sq = 0;
static int64_t last_peak_sq = 0;
static uint8_t has_peaked = 0;

// Interval history for periodicity check
static uint32_t intervals[INTERVAL_HISTORY];
static uint8_t interval_idx = 0;
static uint32_t interval_count = 0;

// Auto-calibration
static uint8_t calibrating = 0;
static uint8_t calib_count = 0;
static int64_t calib_sum = 0;

// Integer square root approximation (fast inverse sqrt style or simple)
// For display only — not used in algorithm
static int32_t isqrt(int64_t x) {
    if (x <= 0) return 0;
    int64_t r = x;
    int64_t tmp = 0;
    // Newton-Raphson: guess = x/2, iterate
    int64_t guess = x >> 1;
    if (guess == 0) guess = 1;
    for (int i = 0; i < 8; i++) {
        guess = (guess + x / guess) >> 1;
    }
    return (int32_t)guess;
}

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
// Step Counter (v34): Optimized
// ============================================
void StepCounter_Init(void) {
    step_count = 0;
    step_last_time = 0;
    last_mag_sq = 0; last_avg_sq = 0; last_peak_sq = 0; has_peaked = 0;
    avg_idx = 0; var_idx = 0;
    interval_idx = 0; interval_count = 0;
    avg_sum = 0;
    
    // Initialize with approximate base squared (17000^2 = 289,000,000)
    int64_t base_sq = 289000000LL;
    for (int i = 0; i < AVG_WINDOW; i++) {
        avg_buf[i] = base_sq;
        avg_sum += base_sq;
    }
    for (int i = 0; i < VAR_WINDOW; i++) var_buf[i] = base_sq;
    for (int i = 0; i < INTERVAL_HISTORY; i++) intervals[i] = 0;
    
    // Start calibration
    calibrating = 1;
    calib_count = 0;
    calib_sum = 0;
}

static uint8_t is_periodic_walking(void) {
    if (interval_count < STABLE_COUNT) return 0;
    
    for (int i = 0; i < STABLE_COUNT; i++) {
        uint8_t idx = (interval_idx - 1 - i + INTERVAL_HISTORY) % INTERVAL_HISTORY;
        uint32_t iv = intervals[idx];
        if (iv < MIN_INTERVAL || iv > MAX_INTERVAL) {
            return 0;
        }
    }
    return 1;
}

void StepCounter_Update(int16_t ax, int16_t ay, int16_t az) {
    uint32_t now = HAL_GetTick();
    
    // ========== OPTIMIZATION 1: Eliminate sqrtf ==========
    // Compute squared magnitude directly (int64_t prevents overflow)
    int64_t mag_sq = (int64_t)ax * ax + (int64_t)ay * ay + (int64_t)az * az;
    
    // ========== Auto-calibration ==========
    if (calibrating) {
        calib_sum += mag_sq;
        calib_count++;
        if (calib_count >= CALIB_SAMPLES) {
            int64_t calibrated_base = calib_sum / CALIB_SAMPLES;
            // Fill buffers with calibrated base
            avg_sum = 0;
            for (int i = 0; i < AVG_WINDOW; i++) {
                avg_buf[i] = calibrated_base;
                avg_sum += calibrated_base;
            }
            for (int i = 0; i < VAR_WINDOW; i++) var_buf[i] = calibrated_base;
            calibrating = 0;
        }
        // Display approximate mag for UI during calibration
        g_step_mag = isqrt(mag_sq);
        g_step_x_hat = isqrt(calib_sum / calib_count);
        return;  // Don't count steps during calibration
    }
    
    // ========== OPTIMIZATION 2: O(1) Moving Average ==========
    // Update variance buffer
    var_buf[var_idx] = mag_sq;
    var_idx = (var_idx + 1) % VAR_WINDOW;
    
    // Update average buffer with O(1) running sum
    int64_t old_val = avg_buf[avg_idx];
    avg_buf[avg_idx] = mag_sq;
    avg_sum = avg_sum - old_val + mag_sq;  // O(1)!
    avg_idx = (avg_idx + 1) % AVG_WINDOW;
    
    int64_t avg_sq = avg_sum / AVG_WINDOW;
    
    // Variance (still O(N) for VAR_WINDOW=10, acceptable)
    int64_t variance = 0;
    for (int i = 0; i < VAR_WINDOW; i++) {
        int64_t diff = var_buf[i] - avg_sq;
        variance += diff * diff;
    }
    variance /= VAR_WINDOW;
    
    // Display values (sqrt approximation for UI only, not used in algorithm)
    g_step_mag = isqrt(mag_sq);
    g_step_x_hat = isqrt(avg_sq);
    
    // Track peak (in squared units)
    if (mag_sq > last_peak_sq) {
        last_peak_sq = mag_sq;
        has_peaked = 1;
    }
    
    // Fall-back detection (all in squared units)
    int64_t peak_drop_sq = last_peak_sq - mag_sq;
    uint8_t crossed_below = (mag_sq < avg_sq) && (last_mag_sq >= last_avg_sq);
    uint8_t peak_big = (peak_drop_sq >= PEAK_DIFF_SQ) && has_peaked;
    uint8_t variance_ok = (variance > VAR_THRESHOLD_SQ);
    uint8_t debounce_ok = (now - step_last_time) > DEBOUNCE_MS;
    
    uint8_t detected = crossed_below && peak_big && variance_ok && debounce_ok;
    
    if (detected) {
        uint32_t interval = now - step_last_time;
        
        // Record valid intervals
        if (interval >= MIN_INTERVAL && interval <= MAX_INTERVAL) {
            intervals[interval_idx] = interval;
            interval_idx = (interval_idx + 1) % INTERVAL_HISTORY;
            if (interval_count < INTERVAL_HISTORY) interval_count++;
        }
        
        if (is_periodic_walking()) {
            if (interval_count == STABLE_COUNT) {
                step_count += (interval_count + 1);
            } else {
                step_count++;
            }
            step_last_time = now;
            
            char dbg[64];
            sprintf(dbg, "STEP! iv=%lu cnt=%lu\r\n", interval, step_count);
            Bluetooth_SendString(dbg);
        } else {
            step_last_time = now;
        }
        
        last_peak_sq = 0;
        has_peaked = 0;
    }
    
    // Safety: reset peak if stuck
    static uint32_t peak_start = 0;
    if (has_peaked && peak_start == 0) peak_start = now;
    if (has_peaked && (now - peak_start) > 2000) {
        last_peak_sq = 0; has_peaked = 0; peak_start = 0;
    }
    if (!has_peaked) peak_start = 0;
    
    last_mag_sq = mag_sq;
    last_avg_sq = avg_sq;
}

uint32_t StepCounter_GetCount(void) {
    return step_count;
}

void StepCounter_Reset(void) {
    step_count = 0;
    step_last_time = 0;
    last_mag_sq = 0; last_avg_sq = 0; last_peak_sq = 0; has_peaked = 0;
    avg_idx = 0; var_idx = 0;
    interval_idx = 0; interval_count = 0;
    avg_sum = 0;
    
    int64_t base_sq = 289000000LL;
    for (int i = 0; i < AVG_WINDOW; i++) {
        avg_buf[i] = base_sq;
        avg_sum += base_sq;
    }
    for (int i = 0; i < VAR_WINDOW; i++) var_buf[i] = base_sq;
    for (int i = 0; i < INTERVAL_HISTORY; i++) intervals[i] = 0;
    
    calibrating = 1;
    calib_count = 0;
    calib_sum = 0;
}
