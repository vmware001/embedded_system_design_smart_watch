/*
 * bluetooth.c
 *
 *  Created on: 2026年7月3日
 *      Author: Whisper
 */
#include "bluetooth.h"
#include "mpu6050.h"
#include "stm32f1xx_hal.h"
#include <string.h>


// 外部串口句柄，与CubeMX生成的保持一致
extern UART_HandleTypeDef huart1;

// 全局接收缓冲区（对应头文件extern声明）
uint8_t bt_rx_buf[BT_RX_BUF_SIZE];

// ------------------- 内部静态变量 -------------------
static uint16_t rx_read_ptr = 0;   // 环形缓冲区读指针

// 帧解析状态机状态
typedef enum {
    RX_WAIT_STX,
    RX_GET_CMD,
    RX_GET_LEN,
    RX_GET_DATA,
    RX_GET_CHK,
    RX_GET_ETX
} RxState;

static RxState rx_state = RX_WAIT_STX;
static uint8_t rx_cmd = 0;
static uint8_t rx_len = 0;
static uint8_t rx_data_buf[64];
static uint8_t rx_data_idx = 0;

// ------------------- 内部工具函数 -------------------
// 计算校验和：累加所有输入字节，取低8位
static uint8_t calc_checksum(uint8_t *data, uint8_t len)
{
    uint8_t sum = 0;
    for(uint8_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return sum;
}

// ------------------- 对外接口函数 -------------------
/**
 * @brief  蓝牙初始化：启动USART1 DMA环形接收
 * @note   对应你配置的DMA1通道5 USART1_RX
 */
void Bluetooth_Init(void)
{
    // 启动DMA环形接收，数据自动持续写入bt_rx_buf
    HAL_UART_Receive_DMA(&huart1, bt_rx_buf, BT_RX_BUF_SIZE);
}

/**
 * @brief  发送MPU6050传感器数据，按自定义帧格式打包
 */
void Bluetooth_SendSensorData(MPU6050_Data *data)
{
    uint8_t tx_buf[64];
    uint8_t pos = 0;

    // 帧头
    tx_buf[pos++] = FRAME_STX;
    // 指令码
    tx_buf[pos++] = CMD_SENSOR_DATA;
    // 数据长度：10个float × 4字节 = 40
    tx_buf[pos++] = 40;

    // 数据段：按顺序打包
    memcpy(&tx_buf[pos], &data->accel_x, 4); pos += 4;
    memcpy(&tx_buf[pos], &data->accel_y, 4); pos += 4;
    memcpy(&tx_buf[pos], &data->accel_z, 4); pos += 4;
    memcpy(&tx_buf[pos], &data->gyro_x, 4);  pos += 4;
    memcpy(&tx_buf[pos], &data->gyro_y, 4);  pos += 4;
    memcpy(&tx_buf[pos], &data->gyro_z, 4);  pos += 4;
    memcpy(&tx_buf[pos], &data->temp, 4);    pos += 4;
    memcpy(&tx_buf[pos], &data->pitch, 4);   pos += 4;
    memcpy(&tx_buf[pos], &data->roll, 4);    pos += 4;
    memcpy(&tx_buf[pos], &data->yaw, 4);     pos += 4;

    // 校验和：对 CMD + LEN + 所有数据 累加
    uint8_t chk = calc_checksum(&tx_buf[1], pos - 1);
    tx_buf[pos++] = chk;
    // 帧尾
    tx_buf[pos++] = FRAME_ETX;

    // 串口阻塞发送
    HAL_UART_Transmit(&huart1, tx_buf, pos, HAL_MAX_DELAY);
}

/**
 * @brief  发送计步数据
 */
void Bluetooth_SendStepData(uint32_t steps)
{
    uint8_t tx_buf[16];
    uint8_t pos = 0;

    tx_buf[pos++] = FRAME_STX;
    tx_buf[pos++] = CMD_STEP_DATA;
    tx_buf[pos++] = 4;  // uint32_t占4字节
    memcpy(&tx_buf[pos], &steps, 4);
    pos += 4;

    uint8_t chk = calc_checksum(&tx_buf[1], pos - 1);
    tx_buf[pos++] = chk;
    tx_buf[pos++] = FRAME_ETX;

    HAL_UART_Transmit(&huart1, tx_buf, pos, HAL_MAX_DELAY);
}

/**
 * @brief  发送设备状态应答帧
 * @param  status: 设备状态码
 */
void Bluetooth_SendDevStatus(uint8_t status)
{
    uint8_t tx_buf[8];
    uint8_t pos = 0;

    tx_buf[pos++] = FRAME_STX;
    tx_buf[pos++] = CMD_DEV_STATUS;
    tx_buf[pos++] = 1;          // 数据长度：1字节状态值
    tx_buf[pos++] = status;     // 状态数据

    // 计算校验和
    uint8_t chk = calc_checksum(&tx_buf[1], pos - 1);
    tx_buf[pos++] = chk;
    tx_buf[pos++] = FRAME_ETX;

    HAL_UART_Transmit(&huart1, tx_buf, pos, HAL_MAX_DELAY);
}

/**
 * @brief  蓝牙接收帧解析，在主循环轮询调用
 * @note   适配DMA1环形缓冲区，通过DMA剩余计数器计算写指针
 */
void Bluetooth_ProcessReceived(void)
{
    // 通过DMA剩余传输数，计算当前DMA写指针位置
    uint16_t write_ptr = BT_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);

    // 读指针 != 写指针，说明有新数据未处理
    while(rx_read_ptr != write_ptr)
    {
        // 取出1字节数据
        uint8_t byte = bt_rx_buf[rx_read_ptr];
        // 读指针后移，环形回绕
        rx_read_ptr = (rx_read_ptr + 1) % BT_RX_BUF_SIZE;

        // 状态机逐字节解析帧（全阶段帧头重同步，防止卡死）
        switch(rx_state)
        {
            case RX_WAIT_STX:
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                }
                break;

            case RX_GET_CMD:
                // 中途遇到帧头：强制重同步，以新帧头为准
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                    break;
                }
                rx_cmd = byte;
                rx_state = RX_GET_LEN;
                break;

            case RX_GET_LEN:
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                    break;
                }
                rx_len = byte;
                if(rx_len > sizeof(rx_data_buf))
                {
                    rx_state = RX_WAIT_STX; // 长度异常，重置
                }
                else
                {
                    rx_state = RX_GET_DATA;
                }
                break;

            case RX_GET_DATA:
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                    break;
                }
                rx_data_buf[rx_data_idx++] = byte;
                if(rx_data_idx >= rx_len)
                {
                    rx_state = RX_GET_CHK;
                }
                break;

            case RX_GET_CHK:
            {
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                    break;
                }
                // 本地计算校验和，与接收值对比
                uint8_t local_chk = rx_cmd + rx_len;
                for(uint8_t i = 0; i < rx_len; i++)
                {
                    local_chk += rx_data_buf[i];
                }
                if(local_chk == byte)
                {
                    rx_state = RX_GET_ETX;
                }
                else
                {
                    rx_state = RX_WAIT_STX; // 校验失败，重置
                }
                break;
            }

            case RX_GET_ETX:
                if(byte == FRAME_STX)
                {
                    rx_state = RX_GET_CMD;
                    rx_data_idx = 0;
                    break;
                }
                if(byte == FRAME_ETX)
                {
                    // 完整帧接收成功，按指令分发处理
                    switch(rx_cmd)
                    {
                        case CMD_SYNC_TIME:
                            // 校验数据长度，解析4字节时间戳
                            if(rx_len == 4)
                            {
                                uint32_t timestamp = 0;
                                memcpy(&timestamp, rx_data_buf, 4);
                                // 此处可接入RTC设置函数，同步系统时间
                            }
                            // 回复设备正常状态，告知上位机指令已接收
                            Bluetooth_SendDevStatus(0x00);
                            break;

                        case CMD_SET_ALARM:
                            // 收到闹钟设置指令，回复确认
                            Bluetooth_SendDevStatus(0x00);
                            break;

                        case CMD_DEV_STATUS:
                            // 上位机查询设备状态，直接回复正常
                            Bluetooth_SendDevStatus(0x00);
                            break;

                        default:
                            // 未知指令不回复
                            break;
                    }
                }
                rx_state = RX_WAIT_STX; // 重置等待下一帧
                break;

            default:
                rx_state = RX_WAIT_STX;
                break;
        }
    }
}

/**
 * @brief  串口错误回调：DMA接收异常停止时自动重启
 * @note   解决“一开始能用，跑一会儿就收不到指令”的核心问题
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        // 清除所有串口错误标志
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE | UART_FLAG_PE);

        // 重新启动DMA环形接收
        HAL_UART_Receive_DMA(huart, bt_rx_buf, BT_RX_BUF_SIZE);

        // 重置接收状态机，避免残留异常状态
        rx_state = RX_WAIT_STX;
        rx_data_idx = 0;
    }
}
