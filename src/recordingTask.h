/**
 * @file recordingTask.h
 * @brief 录像任务管理
 * @details 摄像头连续 JPEG 录制到 SD 卡（AVI 格式）
 * @version 1.0
 * @date 2025-6-30
 */

#pragma once

#include <Arduino.h>

extern volatile bool isRecording;  // 是否正在录像

void recordingTask_Init();
void recordingTask(void *pvParameters);
