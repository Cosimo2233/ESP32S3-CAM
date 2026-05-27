/**
 * @file webTask.cpp
 * @brief Web任务管理
 * @details 该文件实现了Web相关功能，包括文件浏览、下载、预览和删除

 * @version 1.0
 * @date 2025-6-30
 */
#include "webTask.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
const char *ap_ssid = WIFI_Name;
const char *ap_password = WIFI_Password;
WebServer server(80);
void webTask_listFiles()
{
  File root = SD.open("/");
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset='utf-8'>
      <title>图片浏览器</title>
      <style>
        body { font-family: Arial, sans-serif; background-color: #f0f0f0; padding: 20px; }
        h2 { color: #333; }
        ul { list-style: none; padding: 0; }
        li {
          background: #fff;
          margin: 10px 0;
          padding: 10px;
          border-radius: 10px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        a {
          text-decoration: none;
          margin-right: 10px;
          font-weight: bold;
          color: #007bff;
        }
        a:hover { text-decoration: underline; }
      </style>
    </head>
    <body>
      <h2>📂 TF卡图片浏览器</h2>
      <ul>
  )rawliteral";
  while (true)
  {
    File entry = root.openNextFile();
    if (!entry)
      break;
    String name = entry.name();
    if (!entry.isDirectory() && (name.endsWith(".jpg") || name.endsWith(".png")))
    {
      html += "<li>";
      html += "<p>文件名：" + name + "</p>";
      html += "<a href=\"/view?file=" + name + "\">📷 预览</a >";
      html += "<a href=\"/download?file=" + name + "\">⬇ 下载</a >";
      html += "<a href=\"/delete?file=" + name + "\" onclick=\"return confirm('确定删除 " + name + " 吗？')\">❌ 删除</a >";
      html += "</li>";
    }
    entry.close();
  }
  html += "</ul></body></html>";
  server.send(200, "text/html", html);
}

void webTask_HandleDownload()
{
  if (!server.hasArg("file"))
  {
    server.send(400, "text/plain", "缺少 file 参数");
    return;
  }
  String filename = server.arg("file");
  File file = SD.open("/" + filename);
  if (!file)
  {
    server.send(404, "text/plain", "文件未找到");
    return;
  }

  String contentType = "application/octet-stream";
  server.sendHeader("Content-Type", contentType);
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server.sendHeader("Connection", "close");
  server.streamFile(file, contentType);
  file.close();
}
void webTask_HandleView()
{
  if (!server.hasArg("file"))
  {
    server.send(400, "text/plain", "缺少 file 参数");
    return;
  }
  String filename = server.arg("file");
  File file = SD.open("/" + filename);
  if (!file)
  {
    server.send(404, "text/plain", "图片未找到");
    return;
  }
  String contentType = filename.endsWith(".jpg") ? "image/jpeg" : "image/png";
  server.streamFile(file, contentType);
  file.close();
}

void webTask_HandleDelete()
{
  if (!server.hasArg("file"))
  {
    server.send(400, "text/plain", "缺少 file 参数");
    return;
  }
  String filename = server.arg("file");
  if (SD.exists("/" + filename))
  {
    SD.remove("/" + filename);
    server.sendHeader("Location", "/");                       // 设置重定向目标
    server.send(302, "text/plain", "Redirecting to home..."); // 发送重定向响应
  }
  else
  {
    server.send(404, "text/plain", "文件未找到");
  }
}
void webTask_Init()
{

  // 启动自建WiFi热点
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP地址: ");
  Serial.println(IP); // 默认是 192.168.4.1
  // 路由
  server.on("/", HTTP_GET, webTask_listFiles);
  server.on("/view", HTTP_GET, webTask_HandleView);
  server.on("/download", HTTP_GET, webTask_HandleDownload);
  server.on("/delete", HTTP_GET, webTask_HandleDelete);
  // server.on("/cam", HTTP_GET, displayTask_PhotoSave);
  server.begin();
  Serial.println("Web服务器已启动");
}
void webTask(void *pvParameters)
{
  while (1)
  {
    server.handleClient();
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}
