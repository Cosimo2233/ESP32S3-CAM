/**
 * @file usbStreamTask.cpp
 * @brief USB 流任务管理
 * @details
 *   从 cameraTask 的 jpegQueueAtl 获取 RGB565 帧数据，
 *   通过 USB CDC 实时发送到 PC。
 *
 *   通信协议：
 *   - 4字节小端长度头 (uint32_t)
 *   - N 字节 RGB565 像素数据 (320x240 = 153600 bytes)
 * @version 2.0
 * @date 2025-6-30
 */

#include "usbStreamTask.h"
#include "cameraTask.h"
#include <Arduino.h>

extern QueueHandle_t jpegQueueAtl;

void usbStreamTask_Init()
{
  Serial.println("[USB] Stream task ready");
}

void usbStreamTask(void *pvParameters)
{
  uint8_t *frameBuf = NULL;
  uint32_t frameCount = 0;
  const size_t frameSize = 320 * 240 * 2; // RGB565 QVGA

  delay(2000);
  Serial.println("[USB] Stream task started, sending RGB565 frames...");

  while (1)
  {
    if (xQueueReceive(jpegQueueAtl, &frameBuf, portMAX_DELAY) == pdTRUE)
    {
      if (frameBuf)
      {
        frameCount++;

        // 发送协议头：4字节长度
        uint32_t len = frameSize;
        Serial.write((uint8_t *)&len, 4);

        // 发送 RGB565 数据（分块发送，避免阻塞 USB 缓冲区）
        uint32_t sent = 0;
        while (sent < frameSize)
        {
          uint32_t chunk = (frameSize - sent > 1024) ? 1024 : (frameSize - sent);
          Serial.write(frameBuf + sent, chunk);
          sent += chunk;
        }
        Serial.flush();

        if (frameCount % 30 == 0)
        {
          Serial.printf("[USB] Sent %u frames\n", frameCount);
        }

        free(frameBuf);
      }
    }
    // 略作延迟降低 USB 发送频率，避免 buffer 溢出
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
