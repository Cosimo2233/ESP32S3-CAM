/**
 * @file tfCard.cpp
 * @brief TF卡管理
 * @details 该文件实现了TF卡的初始化和文件读写功能

 * @version 1.0
 * @date 2025-6-30
 */
#include "tfCard.h"
#include "esp_camera.h"
#include "displayTask.h"
SPIClass SPI_SD(VSPI);
// 初始化 SD 卡
void tfCard_Init()
{
  SPI_SD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  while (1)
  {
    if (SD.begin(SD_CS, SPI_SD))
    {
      Serial.println("SD Card initialized successfully");
      break;
    }
    else
    {
      Serial.println("SD Card initialization failed, retrying...");
      displayTask_ErrorLOG("Not Found TFCard.\n\nPlease check the TFCard connection and reboot.");
      delay(1000);
    }
    tft.fillScreen(TFT_BLACK);
  }
}
// 获取下一个未被占用的 photo_编号
int tfCard_GetNextPhotoIndex()
{
  int index = 1;
  char filename[32];
  while (true)
  {
    sprintf(filename, "/photo_%d.jpg", index);
    if (!SD.exists(filename))
    {
      break;
    }
    index++;
  }
  return index;
}
void tfCard_SDWriteFile(const uint8_t *data, size_t size)
{
  int photoIndex = tfCard_GetNextPhotoIndex();
  char filename[32];
  sprintf(filename, "/photo_%d.jpg", photoIndex);
  File file = SD.open(filename, FILE_WRITE);
  if (file)
  {
    file.write(data, size);
    file.close();
    Serial.printf("Saved %s\n", filename);
  }
  else
  {
    Serial.println("Save failed");
  }
}
