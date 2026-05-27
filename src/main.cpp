/**
 * @file main.cpp
 * @brief 主程序文件
 * @details 主要功能实现
 * @version 1.1
 * @date 2025-6-30
 */

#include "cameraTask.h"
#include "displayTask.h"
#include "tfCard.h"
#include "webTask.h"
#include "usbStreamTask.h"
#include <SD.h>

#include <Arduino.h>
#include <TJpg_Decoder.h>
#include "keyTask.h"

SemaphoreHandle_t camMutex;
TaskHandle_t cameraTaskHandle = NULL;
TaskHandle_t DisplayTaskHandle = NULL;

void setup()
{
  Serial.begin(115200);
  // 等待 USB CDC 枚举（PC 识别到虚拟串口需要时间）
  delay(500);

  camMutex = xSemaphoreCreateMutex();
  displayTask_Init();
  keyTask_Init();
  tfCard_Init();
  cameraTask_InitCameraConfig();
  cameraTask_Init();
  webTask_Init();
  usbStreamTask_Init();

  // 启动摄像头任务（核心0）
  xTaskCreatePinnedToCore(cameraTask, "CameraTask", 4096, NULL, 1, &cameraTaskHandle, 0);
  // 启动预览任务（核心1）
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, &DisplayTaskHandle, 1);
  // 启动网络任务（核心1）
  xTaskCreatePinnedToCore(webTask, "webTask", 4096, NULL, 1, NULL, 1);
  // 启动按键任务（核心1）
  xTaskCreatePinnedToCore(keyTask, "keyTask", 4096, NULL, 3, NULL, 1);
  // 启动 USB 流任务（核心1）
  xTaskCreatePinnedToCore(usbStreamTask, "usbStream", 4096, NULL, 1, NULL, 1);

  Serial.println("[MAIN] All tasks started");
}

void loop()
{
}
