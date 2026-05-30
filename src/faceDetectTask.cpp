/**
 * @file faceDetectTask.cpp
 * @brief 人脸检测与识别任务（双阶段+完整识别）
 * @version 4.0
 */

#include "faceDetectTask.h"
#include "cameraTask.h"
#include "displayTask.h"

#include <vector>
#include <list>
#include <string>
#include <SD.h>

#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_112_v1_s8.hpp"
#include "face_recognition_tool.hpp"
#include "fb_gfx.h"

volatile bool enrollRequest = false;
volatile bool recognizeEnabled = true;

extern QueueHandle_t frameQueue;

static FaceRecognition112V1S8 *recognizer = NULL;
static HumanFaceDetectMSR01 *detector_s1 = NULL;
static HumanFaceDetectMNP01 *detector_s2 = NULL;

#define FACE_DIR "/face_data"
#define FACE_NAMES_FILE "/face_data/names.txt"
#define MAX_ENROLL 5
#define RECOG_INTERVAL 1000

static unsigned long lastRecogTime = 0;
static char currentLabel[32] = "";

static uint32_t YELLOW() {
    return ((0xFF >> 3) & 0x001F) | ((0xFF >> 2) & 0x07E0) | ((0xFF << 8) & 0xF800);
}

// ===== SD 卡加载/保存 =====
static void loadEnrolledFaces()
{
    if (!SD.exists(FACE_NAMES_FILE)) {
        Serial.println("[FACE] No enrolled faces found");
        return;
    }
    File f = SD.open(FACE_NAMES_FILE, FILE_READ);
    if (!f) return;
    int count = 0;
    while (f.available() && count < MAX_ENROLL) {
        String name = f.readStringUntil('\n');
        name.trim();
        if (name.length() == 0) { count++; continue; }
        char path[64];
        sprintf(path, "%s/emb_%d.bin", FACE_DIR, count);
        File ef = SD.open(path, FILE_READ);
        if (ef) {
            size_t sz = ef.size();
            float *data = (float *)ps_malloc(sz);
            if (data) {
                ef.read((uint8_t *)data, sz);
                Tensor<float> t;
                t.set_element(data).set_shape({(int)(sz / sizeof(float))}).set_auto_free(true);
                recognizer->enroll_id(t, name.c_str(), false);
            }
            ef.close();
        }
        count++;
    }
    f.close();
    Serial.printf("[FACE] Loaded %d face(s)\n", recognizer->get_enrolled_id_num());
}

static void saveEnrolledFaces()
{
    if (!SD.exists(FACE_DIR)) SD.mkdir(FACE_DIR);
    auto ids = recognizer->get_enrolled_ids();
    File f = SD.open(FACE_NAMES_FILE, FILE_WRITE);
    if (!f) return;
    for (int i = 0; i < ids.size() && i < MAX_ENROLL; i++) {
        f.println(ids[i].name.c_str());
        Tensor<float> &emb = recognizer->get_face_emb(ids[i].id);
        char path[64];
        sprintf(path, "%s/emb_%d.bin", FACE_DIR, i);
        File ef = SD.open(path, FILE_WRITE);
        if (ef) {
            ef.write((uint8_t *)emb.get_element_ptr(), emb.get_size() * sizeof(float));
            ef.close();
        }
    }
    f.close();
}

// ===== 画框+文字 =====
static void drawUI(fb_data_t *fb, std::list<dl::detect::result_t> *results, const char *text)
{
    uint32_t c = YELLOW();
    for (auto &p : *results) {
        int x = p.box[0], y = p.box[1];
        int w = p.box[2] - x + 1, h = p.box[3] - y + 1;
        if (x + w > fb->width)  w = fb->width - x;
        if (y + h > fb->height) h = fb->height - y;
        fb_gfx_drawFastHLine(fb, x, y, w, c);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, c);
        fb_gfx_drawFastVLine(fb, x, y, h, c);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, c);
    }
    if (text && strlen(text) > 0)
        fb_gfx_print(fb, 5, 5, c, text);
}

void faceDetectTask_Init()
{
    Serial.println("[FACE] Ready");
}

void faceDetectTask(void *pvParameters)
{
    // 双阶段检测（MSR01 粗检 + MNP01 精检+关键点）
    detector_s1 = new HumanFaceDetectMSR01(0.1F, 0.5F, 10, 0.3F);
    detector_s2 = new HumanFaceDetectMNP01(0.5F, 0.3F, 5);
    recognizer  = new FaceRecognition112V1S8();
    recognizer->set_thresh(0.55F);

    camera_fb_t *fb = NULL;
    loadEnrolledFaces();
    Serial.printf("[FACE] Started, enrolled=%d\n", recognizer->get_enrolled_id_num());

    while (1)
    {
        if (xQueuePeek(frameQueue, &fb, pdMS_TO_TICKS(100)) != pdTRUE || !fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 双阶段检测
        auto &candidates = detector_s1->infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
        auto &results = detector_s2->infer(
            (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);

        fb_data_t rfb = {
            .width = fb->width,
            .height = fb->height,
            .bytes_per_pixel = 2,
            .format = FB_RGB565,
            .data = fb->buf
        };

        if (results.size() > 0) {
            auto &kp = results.front().keypoint;

            // 注册
            if (enrollRequest) {
                enrollRequest = false;
                if (recognizer->get_enrolled_id_num() < MAX_ENROLL) {
                    int id = recognizer->enroll_id(
                        (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3},
                        kp, "Person", false);
                    if (id >= 0) {
                        Serial.printf("[FACE] Enrolled id=%d\n", id);
                        saveEnrolledFaces();
                    }
                }
                drawUI(&rfb, &results, "ENROLLED");
                vTaskDelay(pdMS_TO_TICKS(200));
                continue;
            }

            // 识别（每秒一次）
            unsigned long now = millis();
            if (recognizeEnabled && recognizer->get_enrolled_id_num() > 0 &&
                (now - lastRecogTime > RECOG_INTERVAL))
            {
                face_info_t info = recognizer->recognize(
                    (uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, kp);
                if (info.id >= 0) {
                    snprintf(currentLabel, sizeof(currentLabel), "%s %.2f",
                             info.name.c_str(), info.similarity);
                } else {
                    snprintf(currentLabel, sizeof(currentLabel), "Unknown");
                }
                lastRecogTime = now;
            }
            drawUI(&rfb, &results, currentLabel);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
