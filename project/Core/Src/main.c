/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
  /* USER CODE END Header */
  /* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "ssh1106.h"
#include "mpu6050.h"
#include "bluetooth.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

osThreadId defaultTaskHandle;
osThreadId ScreenTaskHandle;
osThreadId SensorTaskHandle;
osThreadId BleTaskHandle;
osThreadId IdleTaskHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const* argument);
void StartScreenTask(void const* argument);
void StartSensorTask(void const* argument);
void StartBleTask(void const* argument);
void StartIdleTask(void const* argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    /* USER CODE BEGIN 2 */
    SSH1106_Init(&hi2c1);
    MPU6050_Init(&hi2c1);

    Bluetooth_Init(&huart1);
    Bluetooth_SendString("BOOT\r\n");  // 上电广播
    UI_Init();
    /* USER CODE END 2 */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* definition and creation of defaultTask */
    osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    /* definition and creation of ScreenTask */
    osThreadDef(ScreenTask, StartScreenTask, osPriorityNormal, 0, 512);
    ScreenTaskHandle = osThreadCreate(osThread(ScreenTask), NULL);

    /* definition and creation of SensorTask */
    osThreadDef(SensorTask, StartSensorTask, osPriorityNormal, 0, 256);
    SensorTaskHandle = osThreadCreate(osThread(SensorTask), NULL);

    /* definition and creation of BleTask */
    osThreadDef(BleTask, StartBleTask, osPriorityNormal, 0, 512);
    BleTaskHandle = osThreadCreate(osThread(BleTask), NULL);

    /* definition and creation of IdleTask */
    osThreadDef(IdleTask, StartIdleTask, osPriorityLow, 0, 128);
    IdleTaskHandle = osThreadCreate(osThread(IdleTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void) {

    /* USER CODE BEGIN I2C1_Init 0 */

    /* USER CODE END I2C1_Init 0 */

    /* USER CODE BEGIN I2C1_Init 1 */

    /* USER CODE END I2C1_Init 1 */
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C1_Init 2 */

    /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void) {

    /* USER CODE BEGIN USART1_Init 0 */

    /* USER CODE END USART1_Init 0 */

    /* USER CODE BEGIN USART1_Init 1 */

    /* USER CODE END USART1_Init 1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) {

    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pins : PA0 PA1 PA2 PA3 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
  /* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const* argument) {
    /* USER CODE BEGIN 5 */
    /* Infinite loop */
    for (;;) {
        osDelay(1);
    }
    /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the ScreenTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartScreenTask(void const* argument) {
    /* USER CODE BEGIN StartScreenTask */
    PageID_t currentPage = PAGE_HOME;
    uint32_t last_sec_tick = 0;
    /* Infinite loop */
    for (;;) {
        UI_DrawPage(currentPage);
        currentPage = UI_GetPage();
        // 每1秒更新时间
        uint32_t now = HAL_GetTick();
        if (now - last_sec_tick >= 1000) {
            last_sec_tick = now;
            WatchTime_Tick();
        }
        // 秒表由 UI_GetStopwatch 基于 HAL_GetTick 实时计算，
        // 不需要手动 tick，显示时自动获取当前时间
        osDelay(100);  // 10 Hz refresh rate
    }
    /* USER CODE END StartScreenTask */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartSensorTask(void const* argument) {
    /* USER CODE BEGIN StartSensorTask */
    int16_t ax, ay, az;
    float temp;
    /* Infinite loop */
    for (;;) {
        MPU6050_ReadAccel(&ax, &ay, &az);
        temp = MPU6050_ReadTemp();
        StepCounter_Update(ax, ay, az);
        uint32_t steps = StepCounter_GetCount();
        UI_UpdateData(steps, temp, ax, ay, az);
        osDelay(50);  // 20 Hz sensor sampling
    }
    /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartTask04 */
/**
* @brief Function implementing the BleTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask04 */
void StartBleTask(void const* argument) {
    /* USER CODE BEGIN StartBleTask */
    uint8_t rx_buf[BT_BUFFER_SIZE];
    uint8_t rx_len = 0;
    uint32_t last_tx_tick = 0;
    /* Infinite loop */
    for (;;) {
        if (Bluetooth_DataAvailable()) {
            Bluetooth_GetData(rx_buf, &rx_len);
            Bluetooth_ProcessCommand(rx_buf, rx_len);
        }
        // 每 2 秒自动回传步数和温度
        uint32_t now = HAL_GetTick();
        if (now - last_tx_tick >= 2000) {
            last_tx_tick = now;
            char buf[64];
            uint32_t steps = StepCounter_GetCount();
            int temp_int = (int)(g_mpu_temp + 0.5f);
            sprintf(buf, "STEPS:%lu TEMP:%d\r\n", steps, temp_int);
            Bluetooth_SendString(buf);
        }
        osDelay(100);
    }
    /* USER CODE END StartBleTask */
}

/* USER CODE BEGIN Header_StartTask05 */
/**
* @brief Function implementing the IdleTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask05 */
void StartIdleTask(void const* argument) {
    /* USER CODE BEGIN StartIdleTask */

    // 按键状态机
#define BTN_IDLE      0
#define BTN_PRESSED   1
#define BTN_HELD      2
#define BTN_RELEASED  3

    uint8_t btn_state = BTN_IDLE;
    uint32_t btn_press_time = 0;
    const uint32_t SHORT_PRESS_MS = 500;   // 短按阈值
    const uint32_t LONG_PRESS_MS = 1000;  // 长按阈值

    /* Infinite loop */
    for (;;) {
        // 读取 PA1（上拉输入，按下=低电平）
        GPIO_PinState btn_now = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);

        switch (btn_state) {
            case BTN_IDLE:
                if (btn_now == GPIO_PIN_RESET) {  // 按下
                    btn_state = BTN_PRESSED;
                    btn_press_time = HAL_GetTick();
                }
                break;

            case BTN_PRESSED:
                if (btn_now == GPIO_PIN_RESET) {  // 仍然按住
                    uint32_t held = HAL_GetTick() - btn_press_time;
                    if (held >= LONG_PRESS_MS) {
                        // 长按：返回主页（同时清零秒表）
                        UI_GoHome();
                        btn_state = BTN_HELD;  // 进入长按状态，等待释放
                    }
                } else {  // 释放了
                    uint32_t held = HAL_GetTick() - btn_press_time;
                    if (held < SHORT_PRESS_MS) {
                        // 短按：Stopwatch 页控制秒表，其他页切下一页
                        PageID_t current = UI_GetPage();
                        if (current == PAGE_STOPWATCH) {
                            UI_StopwatchToggle();  // 开始/暂停，不离开 Stopwatch 页
                        } else {
                            UI_NextPage();  // 其他页：正常切换
                        }
                    }
                    btn_state = BTN_IDLE;
                }
                break;

            case BTN_HELD:
                if (btn_now == GPIO_PIN_SET) {  // 长按后释放
                    btn_state = BTN_IDLE;
                }
                break;

            default:
                btn_state = BTN_IDLE;
                break;
        }

        osDelay(20);  // 50 Hz 按键扫描
    }
    /* USER CODE END StartIdleTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM4) {
        HAL_IncTick();
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
       /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
