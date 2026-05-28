/**
 * @file recordingTask.cpp
 * @brief 录像任务
 * @details
 *   架构设计：录像时摄像头继续输出 RGB565 到 frameQueue，
 *   displayTask 正常显示。recordingTask 通过 xQueuePeek 获取当前帧，
 *   用 fmt2jpg 软件编码为 JPEG 后写入 SD 卡（AVI MJPEG 格式）。
 *   不需要切换摄像头模式，避免 deinit/init 带来的卡死和重启问题。
 *
 *   拍照键长按 → 开始/停止录像
 *   下键长按   → 切换闪光灯
 *
 *   AVI 文件包含完整的 RIFF 头，写入完成后回填大小信息。
 * @version 2.0
 * @date 2025-6-30
 */

#include "recordingTask.h"
#include "cameraTask.h"
#include "displayTask.h"
#include "tfCard.h"
#include "keyTask.h"

#include <SD.h>
#include <esp_camera.h>
#include <vector>
#include <list>

volatile bool isRecording = false;

extern QueueHandle_t frameQueue;
extern TaskHandle_t cameraTaskHandle;

// 软件 JPEG 编码函数（由 esp_camera 提供）
extern bool fmt2jpg(const uint8_t *src, size_t src_len, uint16_t width, uint16_t height, pixformat_t format, uint8_t quality, uint8_t **out, size_t *out_len);

static uint32_t aviDataSize = 0;
static uint32_t frameCount = 0;
static File videoFile;

// ---- packed AVI 结构体 ----
typedef struct __attribute__((packed)) {
    uint32_t riff_id;        // "RIFF"
    uint32_t riff_size;
    uint32_t riff_type;      // "AVI "
} riff_chunk_t;

typedef struct __attribute__((packed)) {
    uint32_t fcc;            // "avih"
    uint32_t cb;             // 56
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} avi_main_header_t;

typedef struct __attribute__((packed)) {
    uint32_t list_id;        // "LIST"
    uint32_t list_size;
    uint32_t list_type;      // "strl"
} strl_list_t;

typedef struct __attribute__((packed)) {
    uint32_t fcc;            // "strh"
    uint32_t cb;             // 64
    uint32_t fccType;        // "vids"
    uint32_t fccHandler;     // "MJPG"
    uint32_t dwFlags;
    uint32_t wPriority;
    uint32_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    uint16_t rcFrame[4];
} stream_header_t;

typedef struct __attribute__((packed)) {
    uint32_t fcc;            // "strf"
    uint32_t cb;             // 40
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bitmap_info_header_t;

typedef struct __attribute__((packed)) {
    uint32_t list_id;        // "LIST"
    uint32_t list_size;
    uint32_t list_type;      // "movi"
} movi_list_t;

typedef struct __attribute__((packed)) {
    uint32_t chunk_id;       // "00dc"
    uint32_t chunk_size;
} movi_chunk_t;

#define FRAMES_PER_SEC 5
#define MICROSEC_PER_FRAME (1000000 / FRAMES_PER_SEC)

static bool writeAVIHeader(File &file, int w, int h)
{
    riff_chunk_t riff = {0x46464952, 0, 0x20495641};
    avi_main_header_t avih;
    strl_list_t strl = {0x5453494C, sizeof(strl)+sizeof(stream_header_t)+sizeof(bitmap_info_header_t)-8, 0x6C727473};
    stream_header_t strh;
    bitmap_info_header_t strf;
    movi_list_t movi = {0x5453494C, 0, 0x69766F6D};

    memset(&avih, 0, sizeof(avih));
    avih.fcc = 0x68697661;
    avih.cb = 56;
    avih.dwMicroSecPerFrame = MICROSEC_PER_FRAME;
    avih.dwMaxBytesPerSec = w * h * 3 * FRAMES_PER_SEC / 2;
    avih.dwFlags = 0x10;
    avih.dwStreams = 1;
    avih.dwWidth = w;
    avih.dwHeight = h;

    memset(&strh, 0, sizeof(strh));
    strh.fcc = 0x68727473;
    strh.cb = 64;
    strh.fccType = 0x73646976;
    strh.fccHandler = 0x47504A4D;
    strh.dwScale = 1;
    strh.dwRate = FRAMES_PER_SEC;

    memset(&strf, 0, sizeof(strf));
    strf.fcc = 0x66727473;
    strf.cb = 40;
    strf.biSize = 40;
    strf.biWidth = w;
    strf.biHeight = h;
    strf.biPlanes = 1;
    strf.biBitCount = 24;
    strf.biCompression = 0x47504A4D;

    file.write((uint8_t*)&riff, sizeof(riff));
    file.write((uint8_t*)&avih, sizeof(avih));
    file.write((uint8_t*)&strl, sizeof(strl));
    file.write((uint8_t*)&strh, sizeof(strh));
    file.write((uint8_t*)&strf, sizeof(strf));
    file.write((uint8_t*)&movi, sizeof(movi));
    return true;
}

static void updateAVIHeader(File &file)
{
    if (!file) return;
    uint32_t totalSize = file.size();
    // RIFF size
    file.seek(4);
    uint32_t v = totalSize - 8;
    file.write((uint8_t*)&v, 4);
    // avih dwTotalFrames at offset 12+8+32=52
    file.seek(52);
    file.write((uint8_t*)&frameCount, 4);
    // strh dwLength at offset 12+64+12+8+40=136
    file.seek(136);
    file.write((uint8_t*)&frameCount, 4);
    // movi list_size at offset 12+64+12+72+48+4=212
    file.seek(212);
    v = aviDataSize + 4;
    file.write((uint8_t*)&v, 4);
}

static void writeFrameChunk(File &file, const uint8_t *jpeg, size_t len)
{
    if (!file) return;
    movi_chunk_t c = {0x63643030, (uint32_t)len};
    file.write((uint8_t*)&c, 8);
    file.write(jpeg, len);
    if (len & 1) { uint8_t pad=0; file.write(&pad,1); }
    aviDataSize += 8 + ((len+1)&~1);
    frameCount++;
}

void recordingTask_Init()
{
    Serial.println("[REC] Ready");
}

void recordingTask(void *pvParameters)
{
    camera_fb_t *fb = NULL;
    uint8_t *jpgBuf = NULL;
    size_t jpgLen = 0;

    Serial.println("[REC] Started");

    while (1)
    {
        // 闪光灯控制
        if (flashRequest)
        {
            flashRequest = false;
            flashEnabled = !flashEnabled;
            keyTask_LEDFlash(flashEnabled);
            Serial.printf("[FLASH] %s\n", flashEnabled ? "ON" : "OFF");
        }

        // 录像开关（拍照键长按）
        if (recordingRequest)
        {
            recordingRequest = false;
            if (!isRecording) {
                // --- 开始 ---
                isRecording = true;
                aviDataSize = 0;
                frameCount = 0;
                int idx = tfCard_GetNextVideoIndex();
                videoFile = tfCard_OpenVideoFile(idx);
                if (videoFile) {
                    writeAVIHeader(videoFile, 320, 240);
                    Serial.printf("[REC] Started video_%d.avi\n", idx);
                } else {
                    isRecording = false;
                    Serial.println("[REC] File open failed");
                }
            } else {
                // --- 停止 ---
                isRecording = false;
                if (videoFile) {
                    updateAVIHeader(videoFile);
                    tfCard_CloseVideoFile(videoFile);
                }
                Serial.printf("[REC] Stopped, %u frames\n", frameCount);
            }
        }

        // 录像中：偷帧 + 软件 JPEG 编码 + 写入
        if (isRecording && videoFile)
        {
            if (xQueuePeek(frameQueue, &fb, pdMS_TO_TICKS(50)) == pdTRUE && fb && fb->buf)
            {
                // 软件编码 RGB565 → JPEG
                jpgBuf = NULL;
                jpgLen = 0;
                bool ok = fmt2jpg(fb->buf, fb->width*fb->height*2,
                                  fb->width, fb->height,
                                  PIXFORMAT_RGB565, 15,
                                  &jpgBuf, &jpgLen);

                if (ok && jpgBuf && jpgLen > 0)
                {
                    writeFrameChunk(videoFile, jpgBuf, jpgLen);
                    free(jpgBuf);
                    jpgBuf = NULL;
                }

                // 控制帧率
                vTaskDelay(pdMS_TO_TICKS(200)); // ~5fps
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
