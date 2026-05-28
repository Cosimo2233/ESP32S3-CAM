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

/**
  * @brief 获取下一个录像文件索引
  * @param 无
  * @note  用于录像时自动命名 video_X.avi
*/
int tfCard_GetNextVideoIndex();

/**
  * @brief 打开录像文件（保持打开状态）
  * @param index 文件序号
  * @return 已打开的文件对象
  * @note  返回已打开的 File 对象，用于持续写入帧
  */
File tfCard_OpenVideoFile(int index);

/**
  * @brief 写入一帧到已打开的录像文件
  * @param file 已打开的文件引用
  * @param data 帧数据
  * @param size 数据大小
  */
void tfCard_WriteFrame(File &file, const uint8_t *data, size_t size);

/**
  * @brief 关闭录像文件
  * @param file 已打开的文件引用
  */
void tfCard_CloseVideoFile(File &file);