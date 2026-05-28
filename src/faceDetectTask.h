/**
 * @file faceDetectTask.h
 * @brief 人脸检测任务管理
 * @details 基于 esp-dl 的 MTMN 模型进行人脸检测，检测到人脸后自动拍照
 * @version 1.0
 * @date 2025-6-30
 */

#pragma once

#include <Arduino.h>

/**
 * @brief 人脸检测任务初始化
 */
void faceDetectTask_Init();

/**
 * @brief 人脸检测 RTOS 任务
 * @param pvParameters 任务参数
 */
void faceDetectTask(void *pvParameters);
