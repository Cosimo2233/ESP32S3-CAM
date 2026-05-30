/**
 * @file faceDetectTask.h
 * @brief 人脸检测与识别任务管理
 * @details 集成了人脸检测、人脸识别和注册功能
 * @version 2.0
 * @date 2025-6-30
 */

#pragma once

#include <Arduino.h>

extern volatile bool enrollRequest;  // 注册请求
extern volatile bool recognizeEnabled; // 识别开关

void faceDetectTask_Init();
void faceDetectTask(void *pvParameters);
