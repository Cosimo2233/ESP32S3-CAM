/**
 * @file cameraTask.cpp
 * @brief 摄像头任务管理
 * @details 该文件包含了摄像头相关功能的实现
 * @version 1.2
 * @date 2025-6-30
 */
#include "cameraTask.h"
#include "config.h"
#include "displayTask.h"
#include "usbStreamTask.h"

int special2 = 0;
int special = 0;
camera_config_t config;
QueueHandle_t frameQueue;
sensor_t *sensor;

QueueHandle_t jpegQueueAtl;  // USB 流使用的 JPEG 帧队列
volatile bool usbStreamingEnabled = true;

// 预览画面配置：RGB565 输出，直接推 Sprite 最快最稳定
void cameraTask_InitCameraConfig()
{
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;  // 恢复 RGB565
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
}

void cameraTask_InitPicConfig()
{
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  #if defined(OV2640)
    config.frame_size = FRAMESIZE_SXGA;
  #elif defined(OV5640)
    config.frame_size = FRAMESIZE_QSXGA;
  #else
    config.frame_size = FRAMESIZE_UXGA;
  #endif
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
}

void cameraTask(void *pvParameters)
{
  while (1)
  {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // 发送 RGB565 帧给显示任务（旧的正常工作路径）
    if (xQueueSend(frameQueue, &fb, 0) != pdTRUE)
    {
      esp_camera_fb_return(fb);
      continue;
    }

    // USB 流：复制 RGB565 数据发送
    if (usbStreamingEnabled && jpegQueueAtl != NULL)
    {
      size_t frameSize = fb->width * fb->height * 2; // RGB565: 2 bytes per pixel
      uint8_t *usbBuf = (uint8_t *)ps_malloc(frameSize);
      if (usbBuf)
      {
        memcpy(usbBuf, fb->buf, frameSize);
        if (xQueueSend(jpegQueueAtl, &usbBuf, 0) != pdTRUE)
        {
          free(usbBuf);
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void cameraTask_InitCameraSoftwareConfig()
{
  sensor = esp_camera_sensor_get();
  sensor->set_contrast(sensor, special2);
  sensor->set_brightness(sensor, special2);
  sensor->set_saturation(sensor, special2);
  #if defined(OV2640)
    sensor->set_vflip(sensor, 0);
  #elif defined(OV5640)
    sensor->set_vflip(sensor, 1);
  #else
    sensor->set_vflip(sensor, 1);
  #endif
  sensor->set_special_effect(sensor, special);
}

void cameraTask_Init()
{
  cameraTask_InitCameraConfig();
  if (esp_camera_init(&config) != ESP_OK)
  {
    Serial.println("Camera init failed!");
    while (1)
    {
      Serial.println("Retrying camera init...");
      displayTask_ErrorLOG("Not Found Camera.\n\nPlease check the camera connection and reboot.");
      esp_camera_deinit();
      cameraTask_InitCameraConfig();
      if (esp_camera_init(&config) == ESP_OK)
      {
        tft.fillScreen(TFT_BLACK);
        break;
      }
    }
    delay(1000);
  }

  Serial.println("Camera init ok!");

  // 预览帧队列
  frameQueue = xQueueCreate(1, sizeof(camera_fb_t *));
  if (!frameQueue)
  {
    Serial.println("Failed to create frame queue");
    while (1);
  }

  // USB 信号队列（传递 RGB565 帧数据指针）
  jpegQueueAtl = xQueueCreate(1, sizeof(uint8_t *));
  if (!jpegQueueAtl)
  {
    Serial.println("Failed to create jpeg signal queue");
    while (1);
  }

  Serial.println("create queues ok!");
  cameraTask_InitCameraSoftwareConfig();
}
