/**
 * @file cameraTask.h
 * @brief 摄像头任务管理
 * @details 该文件包含了摄像头相关功能的声明
 * @version 1.1
 * @date 2025-6-30
 */

#include <TFT_eSPI.h>
#include "esp_camera.h"
#include <freertos/queue.h>
#pragma once

#define PWDN_GPIO_NUM 46
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM -1
#define SIOD_GPIO_NUM 17
#define SIOC_GPIO_NUM 18
#define Y9_GPIO_NUM 21
#define Y8_GPIO_NUM 42
#define Y7_GPIO_NUM 40
#define Y6_GPIO_NUM 41
#define Y5_GPIO_NUM 39
#define Y4_GPIO_NUM 15
#define Y3_GPIO_NUM 38
#define Y2_GPIO_NUM 16
#define VSYNC_GPIO_NUM 48
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 45

extern TFT_eSPI tft;
extern camera_config_t config;
extern QueueHandle_t frameQueue;
extern QueueHandle_t jpegQueueAtl;  // USB 流任务的信号队列
extern sensor_t *sensor;
extern int special;
extern int special2;
extern volatile bool usbStreamingEnabled;

void cameraTask_InitCameraConfig();
void cameraTask_InitPicConfig();
void cameraTask(void *pvParameters);
void cameraTask_Init();
void cameraTask_InitCameraSoftwareConfig();
