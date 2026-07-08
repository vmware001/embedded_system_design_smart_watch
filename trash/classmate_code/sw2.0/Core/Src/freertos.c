/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "ssd1306.h"
#include "mpu6050.h"
#include "bluetooth.h"
#include "menu.h"
#include "timekeep.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// 菜单页面枚举
typedef enum {
    PAGE_TIME = 0,
    PAGE_SENSOR,
    PAGE_STEP,
    PAGE_SETTING
} PageTypeDef;

// 菜单全局状态
typedef struct {
    PageTypeDef curPage;
    uint8_t cursorIdx;
    uint32_t idleTick;
} MenuState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define OLED_IDLE_SLEEP_MS 10000U    // 10s无操作熄屏
#define SENSOR_TASK_PERIOD 200U      // 传感器200ms采样一次
#define TIME_TASK_PERIOD   1000U     // 计时1s更新
#define MENU_TASK_PERIOD   20U       // 编码器20ms轮询
#define BT_TASK_PERIOD     100U      // 蓝牙100ms轮询
#define STEP_THRESHOLD     1.1f      // 计步加速度阈值

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

// 全局数据缓存
TimeData g_timeData = {0};
MPU6050_Data g_sensorData = {0};
MenuMsg g_menuMsg = {0};
MenuState_t g_menuState = {.curPage = PAGE_TIME, .cursorIdx = 0, .idleTick = 0};
uint32_t g_stepCount = 0;
uint8_t g_oledSleepFlag = 0;
float g_accelFilter = 0.0f; // 计步滤波缓存
uint32_t g_lastStepTick = 0;

/* USER CODE END Variables */
osThreadId taskTimeKeepHandle;
osThreadId taskSensorHandle;
osThreadId taskDisplayHandle;
osThreadId taskMenuHandle;
osThreadId taskBluetoothHandle;
osMessageQId queueSensorDataHandle;
osMessageQId queueTimeUpdateHandle;
osMessageQId queueMenuMsgHandle;
osMessageQId queueBtCmdHandle;
osMutexId mutexI2CHandle;
osSemaphoreId semDisplayUpdateHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void OLED_WakeUp(void);
static void OLED_Sleep(void);
static void RenderUI(TimeData *time, MPU6050_Data *sensor, uint32_t steps);
static void StepDetection(MPU6050_Data *sensor);

/* USER CODE END FunctionPrototypes */

void vTask_TimeKeep(void const * argument);
void vTask_Sensor(void const * argument);
void vTask_Display(void const * argument);
void vTask_Menu(void const * argument);
void vTask_Bluetooth(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* Fix FreeRTOS SysTick - vPortSetupTimerInterrupt may fail on STM32F1 */
  SysTick->LOAD = SystemCoreClock / configTICK_RATE_HZ - 1;
  SysTick->VAL  = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
  /* USER CODE END Init */
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

  /* Create the queue(s) */
  /* definition and creation of queueSensorData */
  osMessageQDef(queueSensorData, 4, 40);
  queueSensorDataHandle = osMessageCreate(osMessageQ(queueSensorData), NULL);

  /* definition and creation of queueTimeUpdate */
  osMessageQDef(queueTimeUpdate, 2, 8);
  queueTimeUpdateHandle = osMessageCreate(osMessageQ(queueTimeUpdate), NULL);

  /* definition and creation of queueMenuMsg */
  osMessageQDef(queueMenuMsg, 4, 4);
  queueMenuMsgHandle = osMessageCreate(osMessageQ(queueMenuMsg), NULL);

  /* definition and creation of queueBtCmd */
  osMessageQDef(queueBtCmd, 4, 32);
  queueBtCmdHandle = osMessageCreate(osMessageQ(queueBtCmd), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of taskTimeKeep */
  osThreadDef(taskTimeKeep, vTask_TimeKeep, osPriorityHigh, 0, 256);
  taskTimeKeepHandle = osThreadCreate(osThread(taskTimeKeep), NULL);

  /* definition and creation of taskSensor */
  osThreadDef(taskSensor, vTask_Sensor, osPriorityAboveNormal, 0, 512);
  taskSensorHandle = osThreadCreate(osThread(taskSensor), NULL);

  /* definition and creation of taskDisplay */
  osThreadDef(taskDisplay, vTask_Display, osPriorityNormal, 0, 1024);
  taskDisplayHandle = osThreadCreate(osThread(taskDisplay), NULL);

  /* definition and creation of taskMenu */
  osThreadDef(taskMenu, vTask_Menu, osPriorityNormal, 0, 512);
  taskMenuHandle = osThreadCreate(osThread(taskMenu), NULL);

  /* definition and creation of taskBluetooth */
  osThreadDef(taskBluetooth, vTask_Bluetooth, osPriorityBelowNormal, 0, 512);
  taskBluetoothHandle = osThreadCreate(osThread(taskBluetooth), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

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
  /* Infinite loop */
  TickType_t xLastWake = xTaskGetTickCount();
  for(;;)
  {
	  // 时间自增
	  	      g_timeData.sec++;
	  	      if(g_timeData.sec >= 60)
	  	      {
	  	        g_timeData.sec = 0;
	  	        g_timeData.min++;
	  	        if(g_timeData.min >= 60)
	  	        {
	  	          g_timeData.min = 0;
	  	          g_timeData.hour++;
	  	          if(g_timeData.hour >= 24)
	  	          {
	  	            g_timeData.hour = 0;
	  	          }
	  	        }
	  	      }
	  	      // 发送时间给显示任务
	  	      xQueueSend(queueTimeUpdateHandle, &g_timeData, 0);
	  	      // 触发屏幕刷新
	  	      xSemaphoreGive(semDisplayUpdateHandle);

	  	      vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TIME_TASK_PERIOD));
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
  /* Infinite loop */
	TickType_t xLastWake = xTaskGetTickCount();
  MPU6050_Init();

  for(;;)
  {
	  // 获取I2C互斥锁
	      MPU6050_ReadAll(&g_sensorData);  /* mutex handled internally */
	      // 计步检测
	      StepDetection(&g_sensorData);
	      // 推送传感器数据
	      xQueueSend(queueSensorDataHandle, &g_sensorData, 0);
	      // 刷新屏幕
	      xSemaphoreGive(semDisplayUpdateHandle);

	      vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(SENSOR_TASK_PERIOD));
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
  /* Infinite loop */
	  TimeData tmpTime;
	  MPU6050_Data tmpSensor;
	  TickType_t xNowTick;
  for(;;)
  {

	   // 等待刷新信号，超时500ms兜底刷新
	    xSemaphoreTake(semDisplayUpdateHandle, pdMS_TO_TICKS(500));

	    // 读取最新数据
	    xQueueReceive(queueTimeUpdateHandle, &tmpTime, 0);
	    xQueueReceive(queueSensorDataHandle, &tmpSensor, 0);

	    // 加锁操作I2C OLED
	    SSD1306_Clear();
	    RenderUI(&tmpTime, &tmpSensor, g_stepCount);  /* SSD1306_SendByte handles mutex */

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
  /* Infinite loop */
	  TickType_t xLastWake = xTaskGetTickCount();
	  int16_t encDelta = 0;
	  uint8_t btnEvt = 0;
  for(;;)
  {
	  // 读取编码器增量并清零计数器
	      encDelta = (int16_t)TIM3->CNT;
	      TIM3->CNT = 0;
	      // 读取按键事件
	      btnEvt = ReadEncoderButton();

	      if(encDelta != 0 || btnEvt != BTN_NONE)
	      {
	        // 重置闲置计时
	        g_menuState.idleTick = xTaskGetTickCount();
	        g_menuMsg.delta = encDelta;
	        g_menuMsg.button = btnEvt;
	        // 发送菜单消息
	        xQueueSend(queueMenuMsgHandle, &g_menuMsg, 0);

	        // 菜单页面逻辑
	        if(btnEvt == BTN_SHORT)
	        {
	          // 短按切换页面
	          g_menuState.curPage = (g_menuState.curPage + 1) % 4;
	          g_menuState.cursorIdx = 0;
	        }
	        else if(encDelta > 0)
	        {
	          g_menuState.cursorIdx++;
	        }
	        else if(encDelta < 0)
	        {
	          if(g_menuState.cursorIdx > 0)
	            g_menuState.cursorIdx--;
	        }
	        // 刷新屏幕
	        xSemaphoreGive(semDisplayUpdateHandle);
	      }
	      vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(MENU_TASK_PERIOD));
  }
  /* USER CODE END vTask_Menu */
}

/* USER CODE BEGIN Header_vTask_Bluetooth */
/**
* @brief Function implementing the taskBluetooth thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_vTask_Bluetooth */
void vTask_Bluetooth(void const * argument)
{
  /* USER CODE BEGIN vTask_Bluetooth */
  /* Infinite loop */
	 TickType_t xLastWake = xTaskGetTickCount();
	  uint32_t sendCnt = 0;
  for(;;)
  {
	  // 解析蓝牙接收帧
	      Bluetooth_ProcessReceived();
	      // 每1秒上报一次传感器数据
	      sendCnt++;
	      if(sendCnt >= 10)
	      {
	        Bluetooth_SendSensorData(&g_sensorData);
	        Bluetooth_SendStepData(g_stepCount);
	        sendCnt = 0;
	      }
	      vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BT_TASK_PERIOD));
  }
  /* USER CODE END vTask_Bluetooth */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* fmt_fixed: format float as fixed-point string (nano newlib %f workaround) */
static void fmt_fixed(char *out, float val, int decimals)
{
    int mult = 1, neg;
    for (int i = 0; i < decimals; i++) mult *= 10;
    int v = (int)(val * mult + (val < 0 ? -0.5f : 0.5f));
    neg = (v < 0);
    int av = neg ? -v : v;
    int ip = av / mult;
    int fp = av % mult;
    if (decimals == 1)
        sprintf(out, "%s%d.%d", neg ? "-" : "", ip, fp);
    else if (decimals == 2)
        sprintf(out, "%s%d.%02d", neg ? "-" : "", ip, fp);
}

// 计步算法
static void StepDetection(MPU6050_Data *sensor)
{
  float mag = sqrtf(sensor->accel_x * sensor->accel_x +
                    sensor->accel_y * sensor->accel_y +
                    sensor->accel_z * sensor->accel_z);
  // IIR低通滤波
  g_accelFilter = 0.1f * mag + 0.9f * g_accelFilter;
  TickType_t now = xTaskGetTickCount();
  // 峰值检测，最小步间隔200ms
  if(g_accelFilter > STEP_THRESHOLD)
  {
    if((now - g_lastStepTick) > pdMS_TO_TICKS(200))
    {
      g_stepCount++;
      g_lastStepTick = now;
    }
  }
}

// 界面渲染函数
static void RenderUI(TimeData *time, MPU6050_Data *sensor, uint32_t steps)
{
  char buf[32];
  switch(g_menuState.curPage)
  {
    case PAGE_TIME:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("=== TIME PAGE ===");
      sprintf(buf, "Time:%02d:%02d:%02d", time->hour, time->min, time->sec);
      SSD1306_SetCursor(0, 2);
      SSD1306_WriteString(buf);
      sprintf(buf, "Steps:%lu", steps);
      SSD1306_SetCursor(0, 4);
      SSD1306_WriteString(buf);
      break;
    case PAGE_SENSOR:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("=== SENSOR ===");
      {
        char pa[14], ra[14], ta[14];
        fmt_fixed(pa, sensor->pitch, 1);
        fmt_fixed(ra, sensor->roll, 1);
        fmt_fixed(ta, sensor->temp, 1);
        sprintf(buf, "P:%s R:%s", pa, ra);
        SSD1306_SetCursor(0, 2);
        SSD1306_WriteString(buf);
        sprintf(buf, "Temp:%sC", ta);
        SSD1306_SetCursor(0, 4);
        SSD1306_WriteString(buf);
      }
      break;
    case PAGE_STEP:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("=== STEP DATA ===");
      sprintf(buf, "Today Steps:%lu", steps);
      SSD1306_SetCursor(0, 3);
      SSD1306_WriteString(buf);
      break;
    case PAGE_SETTING:
      SSD1306_SetCursor(0, 0);
      SSD1306_WriteString("=== SETTING ===");
      SSD1306_SetCursor(0, 2);
      SSD1306_WriteString("1.BT Sync Time");
      SSD1306_SetCursor(0, 4);
      SSD1306_WriteString("2.Brightness");
      break;
    default: break;
  }
}

/* USER CODE END Application */

