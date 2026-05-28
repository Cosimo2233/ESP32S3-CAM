/**
 * @file faceDetectTask.cpp
 * @brief 人脸检测任务
 * @details 基于 esp-dl 的 MTMN 模型 (HumanFaceDetectMSR01) 进行人脸检测。
 *          检测到人脸后在帧缓冲区绘制矩形框，并触发自动拍照。
 * @version 1.0
 * @date 2025-6-30
 */

#include "faceDetectTask.h"
#include "cameraTask.h"
#include "displayTask.h"

#include <vector>
#include <list>
#include "human_face_detect_msr01.hpp"
#include "fb_gfx.h"

// 防抖：两次自动拍照的最小间隔（毫秒）
#define AUTO_CAPTURE_COOLDOWN_MS 3000

extern QueueHandle_t frameQueue;

static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results)
{
    // RGB565 颜色：黄色
    uint32_t color = ((0xFF >> 3) & 0x001F) | ((0xFF >> 2) & 0x07E0) | ((0xFF << 8) & 0xF800);

    for (auto &prediction : *results)
    {
        int x = (int)prediction.box[0];
        int y = (int)prediction.box[1];
        int w = (int)prediction.box[2] - x + 1;
        int h = (int)prediction.box[3] - y + 1;

        if ((x + w) > fb->width)
            w = fb->width - x;
        if ((y + h) > fb->height)
            h = fb->height - y;

        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
    }
}

void faceDetectTask_Init()
{
    Serial.println("[FACE] Face detection task ready");
}

void faceDetectTask(void *pvParameters)
{
    // 单阶段检测器：阈值 0.3, NMS 0.5, top_k 10, 缩放 0.2
    HumanFaceDetectMSR01 detector(0.3F, 0.5F, 10, 0.2F);
    camera_fb_t *fb = NULL;
    unsigned long lastCaptureTime = 0;
    bool savingInProgress = false;

    Serial.println("[FACE] Task started");

    while (1)
    {
        // Peek 当前帧（不消费队列，displayTask 仍能正常接收）
        if (xQueuePeek(frameQueue, &fb, pdMS_TO_TICKS(100)) == pdTRUE && fb != NULL)
        {
            // 运行人脸检测
            std::list<dl::detect::result_t> &results = detector.infer(
                (uint16_t *)fb->buf,
                {(int)fb->height, (int)fb->width, 3}
            );

            if (results.size() > 0)
            {
                Serial.printf("[FACE] Detected %d face(s), score: %.2f\n",
                    results.size(), results.front().score);

                // 在帧缓冲区上绘制人脸框
                fb_data_t rfb;
                rfb.width = fb->width;
                rfb.height = fb->height;
                rfb.data = fb->buf;
                rfb.bytes_per_pixel = 2;
                rfb.format = FB_RGB565;
                draw_face_boxes(&rfb, &results);

                // 自动拍照（防抖 + 不在保存中）
                unsigned long now = millis();
                if (!showSavingPopup && (now - lastCaptureTime > AUTO_CAPTURE_COOLDOWN_MS))
                {
                    lastCaptureTime = now;
                    Serial.println("[FACE] Auto capture triggered!");
                    displayTask_PhotoSave();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
