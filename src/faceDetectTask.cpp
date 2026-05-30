/**
 * @file faceDetectTask.cpp
 * @brief 人脸检测与识别任务
 * @details
 *   功能：
 *   1. 人脸检测（已实现）— 画黄色框 + 显示 "FACE DETECTED"
 *   2. 人脸识别 — 检测到人脸后提取特征，与已注册人脸对比，显示姓名
 *   3. 人脸注册 — 上键双击触发，拍一张人脸照片提取特征，命名保存
 *
 *   注册的人脸特征存入 SD 卡 `/face_data/` 目录
 *   ESP32-S3 重启后自动从 SD 卡加载已注册的人脸
 * @version 2.0
 * @date 2025-6-30
 */

#include "faceDetectTask.h"
#include "cameraTask.h"
#include "displayTask.h"

#include <vector>
#include <list>
#include <string>
#include <SD.h>

// esp-dl 人脸检测与识别
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_112_v1_s8.hpp"
#include "face_recognition_tool.hpp"
#include "fb_gfx.h"

volatile bool enrollRequest = false;
volatile bool recognizeEnabled = true;

extern QueueHandle_t frameQueue;

// ----- 全局识别器（在任务中初始化）-----
static FaceRecognition112V1S8 *recognizer = NULL;
static HumanFaceDetectMSR01 *detector_s1 = NULL;
static HumanFaceDetectMNP01 *detector_s2 = NULL;

// 已注册人脸数据路径
#define FACE_DIR "/face_data"
#define FACE_NAMES_FILE "/face_data/names.txt"
#define MAX_ENROLL 10

// ----- 从 SD 卡加载已注册的人脸 -----
static bool loadEnrolledFaces()
{
    if (!SD.exists(FACE_NAMES_FILE)) {
        Serial.println("[FACE] No enrolled faces found");
        return false;
    }

    File f = SD.open(FACE_NAMES_FILE, FILE_READ);
    if (!f) return false;

    int count = 0;
    while (f.available() && count < MAX_ENROLL) {
        String name = f.readStringUntil('\n');
        name.trim();
        if (name.length() == 0) continue;

        // 读取对应的特征文件
        char embPath[64];
        sprintf(embPath, "%s/emb_%d.bin", FACE_DIR, count);
        File ef = SD.open(embPath, FILE_READ);
        if (ef) {
            size_t embSize = ef.size();
            if (embSize > 0) {
                float *embData = (float *)ps_malloc(embSize);
                if (embData) {
                    ef.read((uint8_t *)embData, embSize);
                    // 创建 Tensor 并注册
                    std::vector<int> shape = {(int)(embSize / sizeof(float))};
                    Tensor<float> embTensor;
                    embTensor.set_element(embData).set_shape(shape).set_auto_free(true);
                    recognizer->enroll_id(embTensor, name.c_str(), false);
                    Serial.printf("[FACE] Loaded: %s\n", name.c_str());
                }
            }
            ef.close();
        }
        count++;
    }
    f.close();
    Serial.printf("[FACE] Loaded %d faces\n", recognizer->get_enrolled_id_num());
    return true;
}

// ----- 保存注册的人脸到 SD 卡 -----
static void saveEnrolledFaces()
{
    if (!SD.exists(FACE_DIR)) {
        SD.mkdir(FACE_DIR);
    }

    std::vector<face_info_t> ids = recognizer->get_enrolled_ids();
    File f = SD.open(FACE_NAMES_FILE, FILE_WRITE);
    if (!f) return;

    for (int i = 0; i < ids.size() && i < MAX_ENROLL; i++) {
        f.println(ids[i].name.c_str());
        Tensor<float> &emb = recognizer->get_face_emb(ids[i].id);
        size_t embSize = emb.get_size() * sizeof(float);

        char embPath[64];
        sprintf(embPath, "%s/emb_%d.bin", FACE_DIR, i);
        File ef = SD.open(embPath, FILE_WRITE);
        if (ef) {
            ef.write((uint8_t *)emb.get_element_ptr(), embSize);
            ef.close();
        }
    }
    f.close();
    Serial.printf("[FACE] Saved %d faces to SD\n", ids.size());
}

// ----- 画人脸框（带识别结果）-----
static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, const char *label)
{
    uint32_t color = ((0xFF >> 3) & 0x001F) | ((0xFF >> 2) & 0x07E0) | ((0xFF << 8) & 0xF800);

    for (auto &prediction : *results) {
        int x = (int)prediction.box[0];
        int y = (int)prediction.box[1];
        int w = (int)prediction.box[2] - x + 1;
        int h = (int)prediction.box[3] - y + 12;

        if ((x + w) > fb->width) w = fb->width - x;
        if ((y + h) > fb->height) h = fb->height - y;

        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
    }

    // 显示标签
    if (label && strlen(label) > 0) {
        fb_gfx_print(fb, 5, 5, color, label);
    } else {
        fb_gfx_print(fb, 5, 5, color, "FACE DETECTED");
    }
}

void faceDetectTask_Init()
{
    Serial.println("[FACE] Face detection & recognition task ready");
}

void faceDetectTask(void *pvParameters)
{
    // ----- 初始化检测器（双阶段提高准确率，获取面部关键点用于识别）-----
    detector_s1 = new HumanFaceDetectMSR01(0.1F, 0.5F, 10, 0.2F);
    detector_s2 = new HumanFaceDetectMNP01(0.5F, 0.3F, 5);
    recognizer = new FaceRecognition112V1S8();

    recognizer->set_thresh(0.55F);  // 相似度阈值

    camera_fb_t *fb = NULL;
    bool enrolled = false;

    // 加载已注册人脸
    loadEnrolledFaces();

    Serial.printf("[FACE] Task started. Enrolled: %d\n", recognizer->get_enrolled_id_num());

    while (1)
    {
        if (xQueuePeek(frameQueue, &fb, pdMS_TO_TICKS(100)) == pdTRUE && fb != NULL)
        {
            // 1. 检测人脸（双阶段，获取关键点用于识别）
            std::list<dl::detect::result_t> &candidates = detector_s1->infer(
                (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
            std::list<dl::detect::result_t> &results = detector_s2->infer(
                (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);

            if (results.size() > 0)
            {
                // 提取关键点（用于识别对齐）
                std::vector<int> landmarks = results.front().keypoint;

                fb_data_t rfb;
                rfb.width = fb->width;
                rfb.height = fb->height;
                rfb.data = fb->buf;
                rfb.bytes_per_pixel = 2;
                rfb.format = FB_RGB565;

                // 2. 检查是否有注册请求
                if (enrollRequest)
                {
                    enrollRequest = false;
                    if (recognizer->get_enrolled_id_num() < MAX_ENROLL) {
                        int id = recognizer->enroll_id(
                            (uint16_t *)fb->buf,
                            {(int)fb->height, (int)fb->width, 3},
                            landmarks,
                            "Person",
                            false
                        );
                        if (id >= 0) {
                            Serial.printf("[FACE] Enrolled ID: %d\n", id);
                            fb_gfx_print(&rfb, 5, 25, ((0xFF>>3)&0x001F)|((0xFF>>2)&0x07E0)|((0xFF<<8)&0xF800), "ENROLLED!");
                            saveEnrolledFaces();
                        }
                    } else {
                        Serial.println("[FACE] Enroll list full!");
                        fb_gfx_print(&rfb, 5, 25, TFT_RED, "FULL!");
                    }
                    draw_face_boxes(&rfb, &results, "ENROLLING...");
                }
                // 3. 识别
                else if (recognizeEnabled && recognizer->get_enrolled_id_num() > 0)
                {
                    face_info_t info = recognizer->recognize(
                        (uint16_t *)fb->buf,
                        {(int)fb->height, (int)fb->width, 3},
                        landmarks
                    );

                    char label[32];
                    if (info.id >= 0) {
                        snprintf(label, sizeof(label), "%s (%.2f)",
                            info.name.c_str(), info.similarity);
                        Serial.printf("[FACE] Recognized: %s (%.2f)\n",
                            info.name.c_str(), info.similarity);
                    } else {
                        snprintf(label, sizeof(label), "Unknown");
                    }
                    draw_face_boxes(&rfb, &results, label);
                }
                else
                {
                    draw_face_boxes(&rfb, &results, NULL);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}