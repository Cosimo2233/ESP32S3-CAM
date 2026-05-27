/**
 * @file displayTask.cpp
 * @brief 显示任务管理
 * @details 该文件实现了显示相关功能
 * @version 1.1
 * @date 2025-6-30
 */
#include "displayTask.h"
#include "cameraTask.h"
#include "Arduino.h"
#include "image.cpp"
#include "tfCard.h"
#include "keyTask.h"
#include "config.h"
#include <TJpg_Decoder.h>

bool showSavingPopup = false;
SPIClass SPI_LCD(HSPI);
TFT_eSPI tft;
TFT_eSprite sprite = TFT_eSprite(&tft);
volatile int currentPhotoIndex = -1;
volatile bool photoViewMode = false;
static unsigned long lastMillis = 0;
static int frames = 0;
float fps = 0;
camera_fb_t *fb = NULL;
extern TaskHandle_t cameraTaskHandle;
extern SemaphoreHandle_t camMutex;

// TJpg_Decoder 回调 —— 逐块推送到 TFT
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// TJpg_Decoder 回调 —— 逐块推送到 Sprite
static TFT_eSprite *decoderSprite = NULL;
bool sprite_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (decoderSprite && y < decoderSprite->height())
  {
    decoderSprite->pushImage(x, y, w, h, bitmap);
  }
  return 1;
}

void displayTask_PhotoSave()
{
  showSavingPopup = true;
  vTaskDelay(100 / portTICK_PERIOD_MS);

  if (cameraTaskHandle != NULL)
  {
    vTaskDelete(cameraTaskHandle);
    cameraTaskHandle = NULL;
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
  esp_camera_deinit();
  cameraTask_InitPicConfig();
  esp_err_t err = esp_camera_init(&config);
  cameraTask_InitCameraSoftwareConfig();
  if (err != ESP_OK)
  {
    Serial.printf("Camera reinit JPEG failed: %d\n", err);
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  tfCard_SDWriteFile(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  esp_camera_deinit();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  cameraTask_InitCameraConfig();
  esp_camera_init(&config);
  cameraTask_InitCameraSoftwareConfig();
  showSavingPopup = false;
  xTaskCreatePinnedToCore(cameraTask, "CameraTask", 4096, NULL, 1, &cameraTaskHandle, 0);
}

bool tft_output_gallery(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  if (y >= tft.height())
    return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void displayTask_Gallery(int index)
{
  #if defined(OV2640)
      TJpgDec.setJpgScale(4);
  #elif defined(OV5640)
      TJpgDec.setJpgScale(8);
  #else
       TJpgDec.setJpgScale(8);
  #endif

  TJpgDec.setCallback(tft_output_gallery);
  tft.setSwapBytes(true);
  char filename[32];
  sprintf(filename, "/photo_%d.jpg", index);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(5, 220);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.printf("Photo loading...\n");
  TJpgDec.drawSdJpg(0, 0, filename);
  tft.setCursor(5, 0);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  uint16_t w = 0, h = 0;
  TJpgDec.getSdJpgSize(&w, &h, filename);
  tft.printf(filename);
  tft.setCursor(5, 220);
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW);
  tft.printf("DPI:%dx%d\n", w, h);
  tft.pushImage(290, 105, 30, 30, camera);
  tft.pushImage(290, 5, 30, 30, up);
  tft.pushImage(290, 205, 30, 30, down);
  Serial.printf("Showing: %s\n", filename);
}

void displayTask_Init()
{
  SPI_LCD.begin(BOARD_LCD_SCK, BOARD_LCD_MOSI, BOARD_LCD_CS);
  tft.begin();
  tft.setRotation(1);
  pinMode(BOARD_LCD_BL, OUTPUT);
  digitalWrite(BOARD_LCD_BL, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0, 0, 320, 240, logo);
  delay(5000);
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
}

void displayTask_DrawGrid3x3(uint16_t *img, int w, int h, uint16_t color)
{
  int cellW = w / 3, cellH = h / 3;
  for (int y = 0; y < h; y++)
  {
    img[y * w + cellW] = color;
    img[y * w + 2 * cellW] = color;
  }
  for (int x = 0; x < w; x++)
  {
    img[cellH * w + x] = color;
    img[2 * cellH * w + x] = color;
  }
}

void displayTask_ErrorLOG(const char *message)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.setTextSize(2);
  tft.setCursor(5, 170);
  tft.println(message);
  tft.pushImage(100, 10, 128, 128, tfcard);
  while (1) { delay(1000); }
}

void displaycamera()
{
  if (xQueueReceive(frameQueue, &fb, portMAX_DELAY) == pdTRUE)
  {
    frames++;
    unsigned long now = millis();
    if (now - lastMillis >= 1000)
    {
      fps = frames * 1000.0f / (now - lastMillis);
      frames = 0;
      lastMillis = now;
    }

    // 获取图像数据和尺寸
    uint16_t *img = (uint16_t *)fb->buf;
    int w = fb->width;
    int h = fb->height;

    // 创建 Sprite 并显示图像
    sprite.createSprite(w, h);
    sprite.setSwapBytes(false);
    sprite.pushImage(0, 0, w, h, img);

    // 网格绘制
    displayTask_DrawGrid3x3((uint16_t *)sprite.getPointer(), w, h, TFT_WHITE);

    // 显示 FPS
    sprite.setTextColor(TFT_WHITE);
    sprite.setTextSize(2);
    char infoStr3[32];
    sprintf(infoStr3, "FPS: %d", (int)fps);
    sprite.drawString(infoStr3, 5, 200);

    // 显示固定信息
    sprite.setTextColor(TFT_YELLOW);
    sprintf(infoStr3, "Mode:%d", special);
    sprite.drawString(infoStr3, 210, 15);

    sprite.pushImage(290, 105, 30, 30, photo);
    sprite.pushImage(290, 5, 30, 30, color);
    sprite.pushImage(290, 205, 30, 30, sun);

    sprite.setTextColor(TFT_YELLOW);
    sprintf(infoStr3, "light:%d", special2);
    sprite.drawString(infoStr3, 195, 220);
    sprintf(infoStr3, "DPI: %dx%d", w, h);
    sprite.drawString(infoStr3, 5, 220);

    if (showSavingPopup)
    {
      sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
      sprite.drawString(" S A V E ... ", 90, 100);
    }

    sprite.pushSprite(0, 0);
    sprite.deleteSprite();

    esp_camera_fb_return(fb);
  }
}

void displayTask(void *pvParameters)
{
  static bool executed = false;

  while (1)
  {
    if (btMIDstate == 0)
    {
      tft.setSwapBytes(false);
      executed = false;
      photoViewMode = false;
      displaycamera();

      if (btTOPstate == 1)
      {
        btTOPstate = 0;
        if (special > 5)
          special = 0;
        else
          special++;
        cameraTask_InitCameraSoftwareConfig();
      }
      if (btDownstate == 1)
      {
        btDownstate = 0;
        if (special2 > 1)
          special2 = -2;
        else
          special2++;
        cameraTask_InitCameraSoftwareConfig();
      }
    }
    else
    {
      if (!executed)
      {
        int lastIndex = tfCard_GetNextPhotoIndex() - 1;
        if (lastIndex >= 1)
        {
          currentPhotoIndex = lastIndex;
          displayTask_Gallery(currentPhotoIndex);
          photoViewMode = true;
        }
        else
        {
          displayTask_ErrorLOG("  No photos found .\n\n  Please take photos first .");
        }
        executed = true;
      }

      if (photoViewMode)
      {
        if (btTOPstate == 1)
        {
          btTOPstate = 0;
          if (currentPhotoIndex > 1)
          {
            currentPhotoIndex--;
            displayTask_Gallery(currentPhotoIndex);
          }
        }
        if (btDownstate == 1)
        {
          btDownstate = 0;
          if (currentPhotoIndex < tfCard_GetNextPhotoIndex() - 1)
          {
            currentPhotoIndex++;
            displayTask_Gallery(currentPhotoIndex);
          }
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
