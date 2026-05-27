/**
 * @file main.cpp
 * @brief 主程序文件
 * @details 主要功能实现

 * @version 1.0
 * @date 2025-6-30
 */

#include "cameraTask.h"
#include "displayTask.h"

#include "tfCard.h"
#include "webTask.h"
#include <SD.h>

#include <Arduino.h>
#include <TJpg_Decoder.h>
#include "keyTask.h"
SemaphoreHandle_t camMutex;

TaskHandle_t cameraTaskHandle = NULL; // 声明全局句柄变量

void setup()
{
  Serial.begin(115200);
  // 按键及闪光灯初始化
  // keyTask_Init();
  camMutex = xSemaphoreCreateMutex();
  displayTask_Init();
  keyTask_Init();
  // 初始化TF卡
  tfCard_Init();
  // 初始化屏幕

  // 初始化摄像头
  cameraTask_InitCameraConfig();
  // 初始化相册配置
  cameraTask_Init();
  // 初始化网络文件管理器
  webTask_Init();
  // 启动摄像头任务
  xTaskCreatePinnedToCore(cameraTask, "CameraTask", 4096, NULL, 1, &cameraTaskHandle, 0); // ← 传出句柄0
  // 启动预览任务
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
  // 启动网络相册任务
  xTaskCreatePinnedToCore(webTask, "webTask", 4096, NULL, 1, NULL, 1);
  // 启动按键任务
  xTaskCreatePinnedToCore(keyTask, "keyTask", 4096, NULL, 3, NULL, 1);
}

void loop()
{
}
