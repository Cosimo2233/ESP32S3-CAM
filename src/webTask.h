/**
 * @file webTask.h
 * @brief Web任务管理
 * @details 该文件包含了Web相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */

#define WIFI_Name "AAA电子厂张师傅"
#define WIFI_Password "12345678"

/**
  * @brief 获取文件列表
  * @param 无
  * @note  遍历TF卡内所有文件
  */
void webTask_listFiles();
/**
  * @brief 图片下载
  * @param 无
  * @note  下载选中的图片
  */
void webTask_HandleDownload();

/**
  * @brief 图片预览
  * @param 无
  * @note  预览TF卡中的图片
  */
void webTask_HandleView();

/**
  * @brief 初始化网页服务
  * @param 无
  * @note  用于初始化配置页面服务器
  */
void webTask_Init();

/**
  * @brief 获取下一张照片索引
  * @param 无
  * @note  用于保存图片时的图片名
  */
void webTask_HandleDelete();

/**
  * @brief 页面任务
  * @param 无
  * @note  页面任务处理
  */
void webTask(void *pvParameters);
