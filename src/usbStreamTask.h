/**
 * @file usbStreamTask.h
 * @brief USB 流任务管理
 * @details 通过 USB CDC 将摄像头 RGB565 帧实时传输到 PC
 * @version 1.0
 * @date 2025-6-30
 */

#pragma once

void usbStreamTask_Init();
void usbStreamTask(void *pvParameters);
