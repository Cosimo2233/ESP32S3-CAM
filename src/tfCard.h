/**
 * @file tfCard.h
 * @brief TF卡管理
 * @details 该文件包含了TF卡相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */
// SPI对象声明（外部链接）
#include <SD.h>
#include <SPI.h>

extern SPIClass SPI_SD;
// 引脚定义
//  测试机型引脚定义（AIOT-PHONE）
//  #define SD_SCK  6
//  #define SD_MISO 7
//  #define SD_MOSI 5
//  #define SD_CS   4
//  发行版本引脚定义
#define SD_SCK 13
#define SD_MISO 14
#define SD_MOSI 12
#define SD_CS 11

 /**
  * @brief TF卡初始化
  * @param 无
  * @note  配置TF卡引脚
  */
void tfCard_Init();

 /**
  * @brief 写入文件
  * @param const uint8_t *data, size_t size
  * @note  写入文件至TF卡
*/
void tfCard_SDWriteFile(const uint8_t *data, size_t size);
 
/**
  * @brief 获取下一张照片索引
  * @param 无
  * @note  用于保存图片时的图片名
*/
int tfCard_GetNextPhotoIndex();