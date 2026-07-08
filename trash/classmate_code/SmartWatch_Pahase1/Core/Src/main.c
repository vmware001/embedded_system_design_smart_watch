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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "mpu6050.h"
#include "bluetooth.h"
#include "encoder.h"
#include "menu.h"
#include "stopwatch.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
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

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

osThreadId defaultTaskHandle;
osThreadId taskTimeKeepHandle;
osThreadId taskSensorHandle;
osThreadId taskDisplayHandle;
osThreadId taskMenuHandle;
osThreadId myTask06Handle;
osMutexId mutexI2CHandle;
osSemaphoreId semDisplayUpdateHandle;
/* USER CODE BEGIN PV */
MPU6050_Data mpu_data;
DisplayPage_t g_current_page = PAGE_CLOCK;
uint8_t g_menu_index = 0;
TimeData_t g_time = {2026, 7, 7, 12, 0, 0};
SensorMsg_t g_sensor;
uint8_t g_time_synced = 0;  /* 蓝牙时间同步标记 */

/* 蓝牙开关: 1=开启, 0=关闭 */
uint8_t g_bluetooth_enabled = 1;
uint8_t g_bt_btn_idx = 0;

/* Clock 子菜单 & 计时器 UI */
uint8_t g_clock_menu_index = 0;
TimerUIState_t g_timer_state = TIMER_SET_HOURS;
uint8_t g_timer_h = 0, g_timer_m = 0, g_timer_s = 0;
uint8_t g_timer_btn_idx = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void const * argument);
void vTask_TimeKeep(void const * argument);
void vTask_Sensor(void const * argument);
void vTask_Display(void const * argument);
void vTask_Menu(void const * argument);
void vTask_Bluetooth(void const * argument);

/* USER CODE BEGIN PFP */
void I2C_ScanAndDisplay(void);
static void Loading_Show(uint8_t step);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  SSD1306_Init();
  SSD1306_Clear();
  SSD1306_SetCursor(0, 3);
  SSD1306_WriteString("Booting...");
  SSD1306_Flush();

  Bluetooth_ATConfig();

  if (MPU6050_Init() != HAL_OK) {
    /* MPU6050 通信失败: 在加载界面显示错误 */
    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("MPU6050 FAIL");
    SSD1306_SetCursor(0, 2);
    {
      char diag[22];
      snprintf(diag, sizeof(diag), "WHO:0x%02X", mpu6050_who_am_i);
      SSD1306_WriteString(diag);
    }
    SSD1306_Flush();
    HAL_Delay(2000);
    I2C_ScanAndDisplay();
    while(1);
  }

  mpu_data.step_count = 0;

  /* 启动蓝牙 DMA 接收 */
  Bluetooth_Init();

  /* 初始化编码器 */
  Encoder_Init();

  /* 初始化秒表 & 计时器 */
  StopWatch_Init();
  Timer_Init();
  /* USER CODE END 2 */

  /* Create the mutex(es) */
  /* definition and creation of mutexI2C */
  osMutexDef(mutexI2C);
  mutexI2CHandle = osMutexCreate(osMutex(mutexI2C));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of semDisplayUpdate */
  osSemaphoreDef(semDisplayUpdate);
  semDisplayUpdateHandle = osSemaphoreCreate(osSemaphore(semDisplayUpdate), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* queues removed (unused) — using global variables directly */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 96);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of taskTimeKeep */
  osThreadDef(taskTimeKeep, vTask_TimeKeep, osPriorityHigh, 0, 128);
  taskTimeKeepHandle = osThreadCreate(osThread(taskTimeKeep), NULL);

  /* definition and creation of taskSensor */
  osThreadDef(taskSensor, vTask_Sensor, osPriorityAboveNormal, 0, 384);
  taskSensorHandle = osThreadCreate(osThread(taskSensor), NULL);

  /* definition and creation of taskDisplay */
  osThreadDef(taskDisplay, vTask_Display, osPriorityNormal, 0, 640);
  taskDisplayHandle = osThreadCreate(osThread(taskDisplay), NULL);

  /* definition and creation of taskMenu */
  osThreadDef(taskMenu, vTask_Menu, osPriorityBelowNormal, 0, 256);
  taskMenuHandle = osThreadCreate(osThread(taskMenu), NULL);

  /* definition and creation of myTask06 */
  osThreadDef(myTask06, vTask_Bluetooth, osPriorityBelowNormal, 0, 384);
  myTask06Handle = osThreadCreate(osThread(myTask06), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  /* 清屏后启动调度器 */
  SSD1306_Clear();
  SSD1306_Flush();
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* Phase 4: 所有功能已迁移到 RTOS 任务中, 此处不可达 */
  /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

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
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7199;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

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
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ======== 启动加载界面 ======== */
#define LOADING_STEPS 5

static void Loading_Show(uint8_t step)
{
    uint8_t i;
    uint8_t bar_x = 20, bar_y = 36, bar_w = 88, bar_h = 10;
    uint8_t fill_x = 22, fill_y = 38, fill_h = 6;
    uint8_t fill_max = 84;
    uint8_t fill_w;

    SSD1306_Clear();

    /* 标题 SmartWatch */
    {
        const char *title = "SmartWatch";
        uint8_t tx = 34;
        for (i = 0; title[i]; i++) {
            SSD1306_DrawBigChar(title[i], tx, 8, 1, 2);
            tx += 6;
        }
    }

    /* 进度条 */
    SSD1306_DrawRect(bar_x, bar_y, bar_w, bar_h);
    fill_w = (step >= LOADING_STEPS) ? fill_max
             : (uint8_t)((uint32_t)fill_max * step / LOADING_STEPS);
    if (fill_w > 0) {
        SSD1306_FillRect(fill_x, fill_y, fill_w, fill_h);
    }

    SSD1306_Flush();
}

/* I2C 地址扫描: 在 OLED 上显示总线上发现的设备地址 */
void I2C_ScanAndDisplay(void) {
    char buf[22];
    uint8_t count = 0;
    uint8_t found[8];  /* 最多记录 8 个设备 */

    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    SSD1306_WriteString("Scanning I2C...");
    SSD1306_Flush();

    /* 快速扫描: 1次重试, 10ms超时 → 总共约1.3秒 */
    for (uint8_t addr = 1; addr < 128; addr++) {
        /* 每扫16个地址刷新一次进度 */
        if ((addr & 0x0F) == 0) {
            SSD1306_SetCursor(0, 2);
            snprintf(buf, sizeof(buf), "Addr: %d/127", addr);
            SSD1306_WriteString(buf);
            SSD1306_Flush();
        }

        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10);
        if (status == HAL_OK) {
            found[count] = addr;
            count++;
            if (count >= 8) break;
        }
    }

    /* 显示结果 */
    SSD1306_Clear();
    SSD1306_SetCursor(0, 0);
    snprintf(buf, sizeof(buf), "Found: %d dev", count);
    SSD1306_WriteString(buf);

    for (uint8_t i = 0; i < count; i++) {
        SSD1306_SetCursor(0, 2 + i);
        snprintf(buf, sizeof(buf), "Dev%d: 0x%02X", i, found[i]);
        SSD1306_WriteString(buf);
    }

    if (count == 0) {
        SSD1306_SetCursor(0, 2);
        SSD1306_WriteString("No device!");
        SSD1306_SetCursor(0, 4);
        SSD1306_WriteString("Check wiring!");
    }
    SSD1306_Flush();
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_vTask_TimeKeep */
/**
* @brief Function implementing the taskTimeKeep thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_TimeKeep */
void vTask_TimeKeep(void const * argument)
{
  /* USER CODE BEGIN vTask_TimeKeep */
  /* 假时间: 从初始日期开始计时, 每秒递增, 含年月日进位 */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  static const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};

  for(;;)
  {
    g_time.seconds++;
    if (g_time.seconds >= 60) { g_time.seconds = 0; g_time.minutes++; }
    if (g_time.minutes >= 60) { g_time.minutes = 0; g_time.hours++; }
    if (g_time.hours >= 24)   {
      g_time.hours = 0;
      g_time.day++;
      uint8_t max_d = days_in_month[g_time.month - 1];
      /* 闰年二月 29 天 */
      if (g_time.month == 2 && (g_time.year % 4 == 0)) max_d = 29;
      if (g_time.day > max_d) { g_time.day = 1; g_time.month++; }
      if (g_time.month > 12)  { g_time.month = 1; g_time.year++; }
    }

    osSemaphoreRelease(semDisplayUpdateHandle);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
  }
  /* USER CODE END vTask_TimeKeep */
}

/* USER CODE BEGIN Header_vTask_Sensor */
/**
* @brief Function implementing the taskSensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_Sensor */
void vTask_Sensor(void const * argument)
{
  /* USER CODE BEGIN vTask_Sensor */
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    osMutexWait(mutexI2CHandle, osWaitForever);
    MPU6050_ReadAll(&mpu_data);
    MPU6050_StepProcess(&mpu_data);
    osMutexRelease(mutexI2CHandle);

    g_sensor.accel_x = mpu_data.accel_x;
    g_sensor.accel_y = mpu_data.accel_y;
    g_sensor.accel_z = mpu_data.accel_z;
    g_sensor.gyro_x  = mpu_data.gyro_x;
    g_sensor.gyro_y  = mpu_data.gyro_y;
    g_sensor.gyro_z  = mpu_data.gyro_z;
    g_sensor.pitch   = mpu_data.pitch;
    g_sensor.roll    = mpu_data.roll;
    g_sensor.temp    = mpu_data.temp;
    g_sensor.step_count = mpu_data.step_count;

    osSemaphoreRelease(semDisplayUpdateHandle);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
  }
  /* USER CODE END vTask_Sensor */
}

/* USER CODE BEGIN Header_vTask_Display */
/**
* @brief Function implementing the taskDisplay thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_Display */
void vTask_Display(void const * argument)
{
  /* USER CODE BEGIN vTask_Display */
  char buf[22];
  const char *menu_labels[] = {"Steps", "Sensor", "Bluetooth", "Clock", "About"};

  for(;;)
  {
    osSemaphoreWait(semDisplayUpdateHandle, osWaitForever);

    osMutexWait(mutexI2CHandle, osWaitForever);

    SSD1306_Clear();

    switch (g_current_page) {
    case PAGE_CLOCK: {
      /* 日期: YYYY-MM-DD 小字居中 */
      {
        char date_buf[11];
        snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                 g_time.year, g_time.month, g_time.day);
        SSD1306_SetCursor(34, 0);
        SSD1306_WriteString(date_buf);
      }

      /* 大字时间: 2x3 缩放, HH:MM:SS 居中 x=10, y=16 */
      {
        char time_buf[9];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
                 g_time.hours, g_time.minutes, g_time.seconds);
        uint8_t x_pos = 10;
        for (uint8_t i = 0; time_buf[i]; i++) {
          if (time_buf[i] == ':') {
            SSD1306_DrawBigChar(':', x_pos, 16, 1, 3);
            x_pos += 6;
          } else {
            SSD1306_DrawBigChar(time_buf[i], x_pos, 16, 2, 3);
            x_pos += 12;
          }
        }
      }

      /* 底部提示 + 蓝牙同步标记 */
      SSD1306_SetCursor(0, 7);
      SSD1306_WriteString("Press to menu");
      if (g_time_synced) {
        SSD1306_SetCursor(108, 0);
        SSD1306_WriteString("BT");
      }
      break;
    }

    case PAGE_MENU: {
      /* 大字体标题 MENU: 4字×12px=48px, 居中 x=40, 高16px */
      const char *title = "MENU";
      uint8_t tx = 40;
      for (uint8_t i = 0; title[i]; i++) {
        SSD1306_DrawBigChar(title[i], tx, 2, 2, 2);
        tx += 12;
      }
      /* 菜单项从第3页开始 */
      for (uint8_t i = 0; i < MENU_ITEMS; i++) {
        SSD1306_SetCursor(0, 3 + i);
        SSD1306_WriteString(i == g_menu_index ? ">" : " ");
        SSD1306_WriteString((char *)menu_labels[i]);
      }
      break;
    }

    case PAGE_STEPS:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("== Step Count ==");
      SSD1306_SetCursor(0, 2);
      snprintf(buf, sizeof(buf), "Steps: %lu",
               (unsigned long)g_sensor.step_count);
      SSD1306_WriteString(buf);
      SSD1306_SetCursor(0, 6);
      SSD1306_WriteString("Press to back");
      break;

    case PAGE_SENSOR:
      SSD1306_SetCursor(0, 0);
      snprintf(buf, sizeof(buf), "AX:%.2f AY:%.2f",
               g_sensor.accel_x, g_sensor.accel_y);
      SSD1306_WriteString(buf);
      SSD1306_SetCursor(0, 2);
      snprintf(buf, sizeof(buf), "P:%.1f R:%.1f",
               g_sensor.pitch, g_sensor.roll);
      SSD1306_WriteString(buf);
      SSD1306_SetCursor(0, 4);
      snprintf(buf, sizeof(buf), "Temp:%.1fC", g_sensor.temp);
      SSD1306_WriteString(buf);
      SSD1306_SetCursor(0, 6);
      SSD1306_WriteString("Press to back");
      break;

    case PAGE_BLUETOOTH:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("== Bluetooth ==");
      SSD1306_SetCursor(0, 2);
      SSD1306_WriteString("Name:SmartWatch");
      SSD1306_SetCursor(0, 3);
      snprintf(buf, sizeof(buf), "Status: %s",
               g_bluetooth_enabled ? "ON" : "OFF");
      SSD1306_WriteString(buf);
      SSD1306_SetCursor(0, 6);
      SSD1306_WriteString(g_bt_btn_idx == 0 ?
                          ">[Toggle] [Back]" :
                          " [Toggle] >[Back]");
      break;

    case PAGE_STOPWATCH: {
      /* 大字时间 MM:SS.CC */
      uint8_t m, s, c;
      StopWatch_GetTime(&m, &s, &c);
      char sw_buf[9];
      snprintf(sw_buf, sizeof(sw_buf), "%02d:%02d.%02d", m, s, c);
      uint8_t sx = 22;
      for (uint8_t i = 0; sw_buf[i]; i++) {
        if (sw_buf[i] == ':' || sw_buf[i] == '.') {
          SSD1306_DrawBigChar(sw_buf[i], sx, 6, 1, 3);
          sx += 6;
        } else {
          SSD1306_DrawBigChar(sw_buf[i], sx, 6, 2, 3);
          sx += 12;
        }
      }
      /* 按钮栏, 居中对齐 */
      SSD1306_SetCursor(0, 6);
      switch (StopWatch_GetState()) {
      case SW_STOPPED:
        SSD1306_WriteString(g_sw_btn_idx == 0 ? ">[Start] [Back]" : " [Start] >[Back]");
        break;
      case SW_RUNNING:
        SSD1306_WriteString(g_sw_btn_idx == 0 ? ">[Pause] [Back]" : " [Pause] >[Back]");
        break;
      case SW_PAUSED:
        if (g_sw_btn_idx == 0)
          SSD1306_WriteString(">[Reset][Resume][Back]");
        else if (g_sw_btn_idx == 1)
          SSD1306_WriteString(" [Reset]>[Resume][Back]");
        else
          SSD1306_WriteString(" [Reset][Resume]>[Back]");
        break;
      }
      break;
    }

    case PAGE_CLOCK_MENU: {
      /* Clock 子菜单 */
      const char *clock_items[] = {"Timer", "StopWatch", "Back"};
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("== Clock ==");
      for (uint8_t i = 0; i < CLOCK_MENU_ITEMS; i++) {
        SSD1306_SetCursor(0, 3 + i);
        SSD1306_WriteString(i == g_clock_menu_index ? ">" : " ");
        SSD1306_WriteString((char *)clock_items[i]);
      }
      break;
    }

    case PAGE_TIMER: {
      uint8_t h, m, s;
      if (g_timer_state <= TIMER_SET_SECONDS) {
        /* 设置阶段: 数字下移到第4页, 避开标题 */
        SSD1306_SetCursor(0, 0);
        SSD1306_WriteString("== Set Timer ==");
        SSD1306_SetCursor(26, 4);
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", g_timer_h, g_timer_m, g_timer_s);
        SSD1306_WriteString(buf);
        if (g_timer_state == TIMER_SET_HOURS)
          { SSD1306_SetCursor(26, 5); SSD1306_WriteString("^^"); }
        else if (g_timer_state == TIMER_SET_MINUTES)
          { SSD1306_SetCursor(44, 5); SSD1306_WriteString("^^"); }
        else
          { SSD1306_SetCursor(62, 5); SSD1306_WriteString("^^"); }
        SSD1306_SetCursor(0, 6);
        SSD1306_WriteString("Rotate + Press OK");
      } else if (g_timer_state == TIMER_RUNNING || g_timer_state == TIMER_PAUSED) {
        SSD1306_SetCursor(0, 0);
        SSD1306_WriteString("== Timer ==");
        Timer_GetRemaining(&h, &m, &s);
        /* 大字倒计时 */
        char tm_buf[9];
        snprintf(tm_buf, sizeof(tm_buf), "%02d:%02d:%02d", h, m, s);
        uint8_t tx = 22;
        for (uint8_t i = 0; tm_buf[i]; i++) {
          if (tm_buf[i] == ':') { SSD1306_DrawBigChar(':', tx, 12, 1, 3); tx += 6; }
          else { SSD1306_DrawBigChar(tm_buf[i], tx, 12, 2, 3); tx += 12; }
        }
        /* 按钮栏 */
        SSD1306_SetCursor(0, 6);
        if (g_timer_state == TIMER_RUNNING)
          SSD1306_WriteString(g_timer_btn_idx == 0 ? ">[Pause] [Exit]" : " [Pause] >[Exit]");
        else
          SSD1306_WriteString(g_timer_btn_idx == 0 ? ">[Resume][Exit]" : " [Resume]>[Exit]");
      } else { /* TIMER_TIMEOUT */
        SSD1306_SetCursor(24, 3);
        SSD1306_WriteString("Time Out!");
      }
      break;
    }

    case PAGE_ABOUT: {
      /* 小标题: "26" + 中文 */
      SSD1306_SetCursor(16, 0);
      SSD1306_WriteString("26");
      SSD1306_DrawCNStr("春嵌入式课设", 28, 0, 14);

      /* SmartWatch 大字 */
      {
        const char *title = "SmartWatch";
        uint8_t tx = 10;
        uint8_t i;
        for (i = 0; title[i]; i++) {
          SSD1306_DrawBigChar(title[i], tx, 16, 2, 2);
          tx += 12;
        }
      }

      /* designed by */
      SSD1306_SetCursor(34, 4);
      SSD1306_WriteString("designed by");

      /* 姓名 */
      SSD1306_DrawCNStr("刘缘园 王昊 柳政道", 2, 42, 14);

      /* 底部提示 */
      SSD1306_SetCursor(0, 7);
      SSD1306_WriteString("Press to back");
      break;
    }

    default:
      break;
    }

    SSD1306_Flush();
    osMutexRelease(mutexI2CHandle);
    osDelay(1);
  }
  /* USER CODE END vTask_Display */
}

/* USER CODE BEGIN Header_vTask_Menu */
/**
* @brief Function implementing the taskMenu thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_Menu */
void vTask_Menu(void const * argument)
{
  /* USER CODE BEGIN vTask_Menu */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  TickType_t menu_activity_tick = 0;  /* 菜单上次活动 tick */
  TickType_t timer_timeout_tick = 0;  /* 计时器超时时刻 */
  int8_t delta;

  for(;;)
  {
    delta = Encoder_GetDelta();

    /* ==== Clock 子菜单: 旋转选项目 ==== */
    if (delta != 0 && g_current_page == PAGE_CLOCK_MENU) {
      if (delta > 0 && g_clock_menu_index < CLOCK_MENU_ITEMS - 1) g_clock_menu_index++;
      else if (delta < 0 && g_clock_menu_index > 0)               g_clock_menu_index--;
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 秒表页面: 旋转选按钮 ==== */
    if (delta != 0 && g_current_page == PAGE_STOPWATCH) {
      int8_t max_btn;
      switch (StopWatch_GetState()) {
        case SW_PAUSED: max_btn = 2; break;
        default:        max_btn = 1; break;
      }
      if (delta > 0 && g_sw_btn_idx < max_btn) g_sw_btn_idx++;
      else if (delta < 0 && g_sw_btn_idx > 0)  g_sw_btn_idx--;
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 计时器设置: 旋钮调节小时/分钟/秒 ==== */
    if (delta != 0 && g_current_page == PAGE_TIMER && g_timer_state <= TIMER_SET_SECONDS) {
      switch (g_timer_state) {
      case TIMER_SET_HOURS:
        if (delta > 0 && g_timer_h < 23) g_timer_h++;
        else if (delta < 0 && g_timer_h > 0) g_timer_h--;
        if (g_timer_h > 23) g_timer_h = 0;
        break;
      case TIMER_SET_MINUTES:
        if (delta > 0 && g_timer_m < 59) g_timer_m++;
        else if (delta < 0 && g_timer_m > 0) g_timer_m--;
        if (g_timer_m > 59) g_timer_m = 0;
        break;
      case TIMER_SET_SECONDS:
        if (delta > 0 && g_timer_s < 59) g_timer_s++;
        else if (delta < 0 && g_timer_s > 0) g_timer_s--;
        if (g_timer_s > 59) g_timer_s = 0;
        break;
      default: break;
      }
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 计时器运行/暂停: 旋钮选按钮 [Pause]/[Resume] [Exit] ==== */
    if (delta != 0 && g_current_page == PAGE_TIMER &&
        (g_timer_state == TIMER_RUNNING || g_timer_state == TIMER_PAUSED)) {
      if (delta > 0 && g_timer_btn_idx < 1) g_timer_btn_idx++;
      else if (delta < 0 && g_timer_btn_idx > 0) g_timer_btn_idx--;
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 蓝牙页面: 旋钮选按钮 [Toggle] [Back] ==== */
    if (delta != 0 && g_current_page == PAGE_BLUETOOTH) {
      if (delta > 0 && g_bt_btn_idx < 1) g_bt_btn_idx++;
      else if (delta < 0 && g_bt_btn_idx > 0) g_bt_btn_idx--;
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 菜单页面: 旋转选菜单项 ==== */
    if (delta != 0 && g_current_page == PAGE_MENU) {
      if (delta > 0 && g_menu_index < MENU_ITEMS - 1) g_menu_index++;
      else if (delta < 0 && g_menu_index > 0)          g_menu_index--;
      menu_activity_tick = xTaskGetTickCount();
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    if (Encoder_ButtonPressed()) {
      switch (g_current_page) {
      case PAGE_CLOCK:
        g_current_page = PAGE_MENU;
        g_menu_index = 0;
        menu_activity_tick = xTaskGetTickCount();
        break;

      case PAGE_MENU:
        switch (g_menu_index) {
        case 0: g_current_page = PAGE_STEPS;       break;
        case 1: g_current_page = PAGE_SENSOR;      break;
        case 2: g_current_page = PAGE_BLUETOOTH;   break;
        case 3:
          g_current_page = PAGE_CLOCK_MENU;
          g_clock_menu_index = 0;
          break;
        case 4: g_current_page = PAGE_ABOUT;       break;
        }
        break;

      case PAGE_CLOCK_MENU:
        switch (g_clock_menu_index) {
        case 0: /* Timer */
          g_current_page = PAGE_TIMER;
          g_timer_state = TIMER_SET_HOURS;
          g_timer_h = 0; g_timer_m = 0; g_timer_s = 0;
          Timer_Init();
          break;
        case 1: /* StopWatch */
          g_current_page = PAGE_STOPWATCH;
          StopWatch_Init();
          break;
        case 2: /* Back */
          g_current_page = PAGE_MENU;
          menu_activity_tick = xTaskGetTickCount();
          break;
        }
        break;

      case PAGE_TIMER:
        if (g_timer_state == TIMER_SET_HOURS) {
          g_timer_state = TIMER_SET_MINUTES;
        } else if (g_timer_state == TIMER_SET_MINUTES) {
          g_timer_state = TIMER_SET_SECONDS;
        } else if (g_timer_state == TIMER_SET_SECONDS) {
          Timer_SetTotal(g_timer_h, g_timer_m, g_timer_s);
          Timer_Start();
          g_timer_state = TIMER_RUNNING;
          g_timer_btn_idx = 0;
        } else if (g_timer_state == TIMER_RUNNING) {
          if (g_timer_btn_idx == 0) {
            Timer_Pause();
            g_timer_state = TIMER_PAUSED;
          } else {
            g_current_page = PAGE_MENU;
            menu_activity_tick = xTaskGetTickCount();
          }
        } else if (g_timer_state == TIMER_PAUSED) {
          if (g_timer_btn_idx == 0) {
            Timer_Resume();
            g_timer_state = TIMER_RUNNING;
            g_timer_btn_idx = 0;
          } else {
            g_current_page = PAGE_MENU;
            menu_activity_tick = xTaskGetTickCount();
          }
        }
        /* TIMEOUT: 按键忽略 */
        break;

      case PAGE_STEPS:
      case PAGE_SENSOR:
      case PAGE_ABOUT:
        g_current_page = PAGE_MENU;  /* 返回菜单 */
        menu_activity_tick = xTaskGetTickCount();
        break;

      case PAGE_BLUETOOTH:
        if (g_bt_btn_idx == 0) {
          g_bluetooth_enabled = !g_bluetooth_enabled;
        } else {
          g_current_page = PAGE_MENU;
          menu_activity_tick = xTaskGetTickCount();
        }
        break;

      case PAGE_STOPWATCH: {
        StopWatchState_t sws = StopWatch_GetState();
        if (sws == SW_STOPPED) {
          if (g_sw_btn_idx == 0) StopWatch_Start();
          else {
            g_current_page = PAGE_CLOCK_MENU;
            g_clock_menu_index = 1;  /* 回到 StopWatch 位置 */
          }
        } else if (sws == SW_RUNNING) {
          if (g_sw_btn_idx == 0) StopWatch_Pause();
          else {
            g_current_page = PAGE_CLOCK_MENU;
            g_clock_menu_index = 1;
          }
        } else { /* SW_PAUSED */
          if      (g_sw_btn_idx == 0) StopWatch_Reset();
          else if (g_sw_btn_idx == 1) StopWatch_Start();
          else {
            g_current_page = PAGE_CLOCK_MENU;
            g_clock_menu_index = 1;
          }
        }
        break;
      }

      default:
        break;
      }
      osSemaphoreRelease(semDisplayUpdateHandle);
    }

    /* ==== 计时器超时检测 & 5 秒后返回菜单 ==== */
    if (g_current_page == PAGE_TIMER) {
      if (g_timer_state == TIMER_RUNNING || g_timer_state == TIMER_PAUSED) {
        uint8_t _h, _m, _s;
        Timer_GetRemaining(&_h, &_m, &_s);
        if (Timer_IsTimeout()) {
          g_timer_state = TIMER_TIMEOUT;
          timer_timeout_tick = xTaskGetTickCount();
          osSemaphoreRelease(semDisplayUpdateHandle);
        }
      } else if (g_timer_state == TIMER_TIMEOUT) {
        if ((xTaskGetTickCount() - timer_timeout_tick) >= pdMS_TO_TICKS(5000)) {
          g_current_page = PAGE_MENU;
          menu_activity_tick = xTaskGetTickCount();
          osSemaphoreRelease(semDisplayUpdateHandle);
        }
      }
    }

    /* ==== 菜单页 10 秒无操作自动返回时钟 ==== */
    if (g_current_page == PAGE_MENU) {
      if ((xTaskGetTickCount() - menu_activity_tick) >= pdMS_TO_TICKS(10000)) {
        g_current_page = PAGE_CLOCK;
        osSemaphoreRelease(semDisplayUpdateHandle);
      }
    }

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
  }
  /* USER CODE END vTask_Menu */
}

/* USER CODE BEGIN Header_vTask_Bluetooth */
/**
* @brief Function implementing the taskMenu thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_Bluetooth */
void vTask_Bluetooth(void const * argument)
{
  /* USER CODE BEGIN vTask_Bluetooth */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint8_t cycle = 0;

  for(;;)
  {
    /* 蓝牙开关: 关闭时跳过数据收发 */
    if (g_bluetooth_enabled) {
      /* 检查蓝牙时间同步指令 */
      if (Bluetooth_CheckRxCommand()) {
        osSemaphoreRelease(semDisplayUpdateHandle);
      }

      cycle++;
      if (cycle >= 5) {
        cycle = 0;
        MPU6050_Data tmp;
        tmp.accel_x = g_sensor.accel_x;
        tmp.accel_y = g_sensor.accel_y;
        tmp.accel_z = g_sensor.accel_z;
        tmp.gyro_x  = g_sensor.gyro_x;
        tmp.gyro_y  = g_sensor.gyro_y;
        tmp.gyro_z  = g_sensor.gyro_z;
        tmp.pitch   = g_sensor.pitch;
        tmp.roll    = g_sensor.roll;
        tmp.temp    = g_sensor.temp;
        tmp.step_count = g_sensor.step_count;
        Bluetooth_SendSensorData(&tmp);
      }
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
  }
  /* USER CODE END vTask_Bluetooth */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
