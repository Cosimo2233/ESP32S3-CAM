/**
 * @file displayTask.h
 * @brief 显示任务管理
 * @details 该文件包含了显示相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */
#include <Arduino.h>
#include <TFT_eSPI.h>
#pragma once
extern SPIClass SPI_LCD; 
extern TFT_eSPI tft;

#define BOARD_LCD_MOSI 2
#define BOARD_LCD_MISO -1
#define BOARD_LCD_SCK 5
#define BOARD_LCD_CS 3
#define BOARD_LCD_DC 4
#define BOARD_LCD_RST -1
#define BOARD_LCD_BL 1

 /**
  * @brief 屏幕RTOS任务
  * @param 无
  * @note 屏幕任务处理
  */
void displayTask(void *pvParameters);

 /**
  * @brief 绘制3X3表格
  * @param 无
  * @note  绘制拍摄窗口
  */
void displayTask_DrawGrid3x3(uint16_t *img, int w, int h, uint16_t color);
 /**
  * @brief 绘制保存提示窗
  * @param 无
  * @note  拍摄后存储提示
  */
void displayTask_DrawSavingPopup(uint16_t *img, int w, int h, const char *msg, uint16_t color);
 /**
  * @brief 屏幕初始化
  * @param 无
  * @note  屏幕的初始化内容
  */
void displayTask_Init();
 /**
  * @brief 屏幕错误弹窗
  * @param 无
  * @note  出现错误时可通过此函数打印至屏幕
  */
void displayTask_ErrorLOG(const char *message);
extern bool showSavingPopup;
extern bool saveDone;
extern TaskHandle_t DisplayTaskHandle; // 从别的文件访问
extern TaskHandle_t cameraTaskHandle;  // 从别的文件访问
 /**
  * @brief 拍摄函数
  * @param 无
  * @note  存储拍摄的照片
  */
void displayTask_PhotoSave();
 /**
  * @brief 相册显示
  * @param 无
  * @note  显示存储的照片
  */
void displayTask_Gallery(int index);
