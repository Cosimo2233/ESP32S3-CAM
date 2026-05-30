/**
 * @file keyTask.h
 * @brief 按键任务管理
 * @details 该文件包含了按键相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */
#include <Arduino.h>
#include "displayTask.h"
#include "faceDetectTask.h"
#define KEY_CAM_NUM 10
#define KEY_TOP_NUM 9
#define KEY_MID_NUM 8
#define KEY_DOWN_NUM 7
#define LED_FLASH_NUM 6

extern volatile bool keyCamPressed;
extern volatile bool keyTopPressed;
extern volatile bool keyMidPressed;
extern volatile bool keyDownPressed;
extern int btTOPstate;
extern int btMIDstate;
extern int btDownstate;
 /**
  * @brief 按键任务初始化
  * @param 无
  * @note  配置按键引脚
  */
void keyTask_Init();
 /**
  * @brief 补光灯控制函数
  * @param bool
  * @note  控制补光灯
  */
void keyTask_LEDFlash(bool on);
 /**
  * @brief 补光灯控制函数
  * @param 无
  * @note  控制补光灯
  */
void keyTask(void *pvParameters);
