/**
 * @file cameraTask.h
 * @brief 摄像头任务管理
 * @details 该文件包含了摄像头相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */
// CameraTask.cpp
#include "cameraTask.h"
#include "config.h" 
#include "displayTask.h"
int special2 = 0;
int special = 0;
camera_config_t config;   // 声明相机配置结构体
QueueHandle_t frameQueue; // 声明队列，用于存储预览画面帧数据
sensor_t *sensor;
// —— 预览画面配置（OV2640/兼容OV5640） ——
// 初始化预览画面配置，RGN565格式，同时设置分辨率为QVGA，与TFT屏幕push格式及分辨率一致
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
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 20;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
}

// —— 相机拍摄配置（OV2640/兼容OV5640） ——
// 初始化拍摄配置，JPEG格式，同时设置分辨率为HD，与png格式要求一致，屏幕比例下最大分辨率，在取得最佳质量下确保能通过屏幕预览画面

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
    config.frame_size = FRAMESIZE_SXGA;    // 1280x1024
  #elif defined(OV5640)
    config.frame_size = FRAMESIZE_QSXGA;   // 2560x1920
  #else
    config.frame_size = FRAMESIZE_UXGA;    // 1600x1200 (安全默认值)
  #endif
  // config.frame_size = FRAMESIZE_SXGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
}

// —— 摄像头任务 ——
// 摄像头任务函数，循环获取摄像头帧数据，并将其发送到队列中
// 如果获取失败，则延时10毫秒后重试
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

    if (xQueueSend(frameQueue, &fb, 0) != pdTRUE)
    {
      esp_camera_fb_return(fb);
    }
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}
void cameraTask_InitCameraSoftwareConfig()
{
  sensor = esp_camera_sensor_get();
  /* 传感器默认配置 */

  sensor->set_contrast(sensor, special2);   // 对比度：0 (中间值，范围通常-2~2)
  sensor->set_brightness(sensor, special2); // 亮度：0 (中间值，范围通常-2~2)
  sensor->set_saturation(sensor, special2); // 饱和度：0 (中间值，范围通常-2~2)
 // sensor->set_sharpness(sensor, 1);                   // 锐度：0 (关闭锐化)
 // sensor->set_denoise(sensor, 1);                   // 降噪：0 (关闭降噪)
 // sensor->set_gainceiling(sensor, GAINCEILING_8X);      // 增益上限：8倍 (防止过曝)
 // sensor->set_quality(sensor, 10);                  // JPEG质量：10 (0-63，值越低压缩率越高)
 // sensor->set_colorbar(sensor, 0);                   // 彩条测试：0 (关闭测试模式)
 // sensor->set_whitebal(sensor, 1);                   // 自动白平衡：1 (开启)
 // sensor->set_gain_ctrl(sensor, 1);                   // 自动增益控制：1 (开启)
 // sensor->set_exposure_ctrl(sensor, 1);                   // 自动曝光控制：1 (开启)
 // sensor->set_hmirror(sensor, 0);                   // 水平镜像：0 (关闭)
  #if defined(OV2640)
    sensor->set_vflip(sensor, 0);    // 1280x1024
  #elif defined(OV5640)
    sensor->set_vflip(sensor, 1);     // 2560x1920
  #else
    sensor->set_vflip(sensor, 1);    // 1600x1200 (安全默认值)
  #endif
  // 垂直翻转：0 (关闭)
  // sensor->set_aec2(sensor, 1);                   // AEC2算法：0 (关闭)
  // sensor->set_awb_gain(sensor, 1);                   // AWB增益：1 (开启)
  // sensor->set_agc_gain(sensor, 0);                  // AGC增益值：0 (自动)
  // sensor->set_aec_value(sensor, 1200);                 // 曝光值：600 (1-1200，单位行数)
  sensor->set_special_effect(sensor, special); // 特效：0 (无特效)
  // sensor->set_wb_mode(sensor, 0);                   // 白平衡模式：0 (自动)
  // sensor->set_ae_level(sensor, 1);                   // AE补偿：0 (无补偿)
  // sensor->set_dcw(sensor, 1);                   // DCW(下采样)：1 (开启)
  // sensor->set_bpc(sensor, 1);                   // 坏点校正：1 (开启)
  // sensor->set_wpc(sensor, 1);                   // 白点校正：1 (开启)
  // sensor->set_raw_gma(sensor, 1);                   // RAW伽马校正：1 (开启)
  // sensor->set_lenc(sensor, 1);                   // 镜头校正：1 (开启)
  // 时钟频率：20MHz (典型工作频率)
}
// —— 摄像头初始化 ——
// 初始化摄像头，配置相机参数，并创建帧队列
void cameraTask_Init()
{
  cameraTask_InitCameraConfig();
  if (esp_camera_init(&config) != ESP_OK)
  {
    Serial.println("Camera init failed!");
    while (1)
    {
      Serial.println("Retrying camera init...");
      // 如果初始化失败，等待100毫秒后重试
      displayTask_ErrorLOG("Not Found Camera.\n\nPlease check the camera connection and reboot.");
      esp_camera_deinit();
      cameraTask_InitCameraConfig();
      if (esp_camera_init(&config) == ESP_OK)
      {
        tft.fillScreen(TFT_BLACK);
        break; // 成功则跳出循环
      }
    }

    delay(1000);
  }

  Serial.println("Camera init ok!");
  // 创建帧队列
  frameQueue = xQueueCreate(1, sizeof(camera_fb_t *));
  if (!frameQueue)
  {
    Serial.println("Failed to create frame queue");
    while (1)
      ;
  }
  Serial.println("create frame queue ok!");
  cameraTask_InitCameraSoftwareConfig();
  // sensor->set_hmirror(sensor, 1); // 设置水平翻转
}
