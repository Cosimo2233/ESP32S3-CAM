/**
 * @file keyTask.h
 * @brief 按键任务管理
 * @details 该文件包含了按键相关功能的声明

 * @version 1.0
 * @date 2025-6-30
 */
#include "keyTask.h"

#define DOUBLE_CLICK_THRESHOLD 400 // 双击最大间隔
#define LONG_PRESS_THRESHOLD 800   // 长按判断阈值
#define DEBOUNCE_DELAY 100         // 去抖动时间

// 按键状态枚举
enum KeyState
{
  IDLE,             // 空闲状态
  PRESSED,          // 按下状态
  WAIT_SECOND_PRESS,// 等待第二次按下（用于双击判断）
  LONG_PRESSED      // 长按状态
};

struct KeyInfo
{
  int pin;
  volatile bool flag;
  KeyState state;
  unsigned long pressStart;
  unsigned long lastPress;
  bool skipNextSingle; // 用于跳过单击事件
};

KeyInfo keys[4] = {
    {KEY_CAM_NUM, false, IDLE, 0, 0, true}, // 拍照
    {KEY_TOP_NUM, false, IDLE, 0, 0, true}, // 上键
    {KEY_MID_NUM, false, IDLE, 0, 0, true}, // 中键
    {KEY_DOWN_NUM, false, IDLE, 0, 0, true} // 下键
};

// 用于主程序判断按键逻辑
int btMIDstate = 0;
int btTOPstate = 0;
int btDownstate = 0;

// 中断回调
void IRAM_ATTR onKeyCamPress() { keys[0].flag = true; }
void IRAM_ATTR onKeyTopPress() { keys[1].flag = true; }
void IRAM_ATTR onKeyMidPress() { keys[2].flag = true; }
void IRAM_ATTR onKeyDownPress() { keys[3].flag = true; }

void keyTask_Init()
{
  for (int i = 0; i < 4; i++)
  {
    pinMode(keys[i].pin, INPUT_PULLUP);
  }

  pinMode(LED_FLASH_NUM, OUTPUT);
  digitalWrite(LED_FLASH_NUM, HIGH);

  attachInterrupt(digitalPinToInterrupt(KEY_CAM_NUM), onKeyCamPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(KEY_TOP_NUM), onKeyTopPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(KEY_MID_NUM), onKeyMidPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(KEY_DOWN_NUM), onKeyDownPress, FALLING);
}
void handleKey(KeyInfo &key, const char *name, int &stateVar, void (*singleClick)(), void (*doubleClick)(), void (*longPress)())
{
  unsigned long now = millis();

  if (key.flag)
  {
    key.flag = false;

    if (key.state == IDLE)
    {
      key.pressStart = now;
      key.state = PRESSED;
      key.skipNextSingle = false; // 重置标志
    }
    else if (key.state == WAIT_SECOND_PRESS && (now - key.lastPress) < DOUBLE_CLICK_THRESHOLD)
    {
      if (doubleClick)
        doubleClick();
      Serial.printf("%s 双击\n", name);
      key.state = IDLE;
      key.skipNextSingle = true;
    }
  }

  if (key.state == PRESSED && (now - key.pressStart > LONG_PRESS_THRESHOLD))
  {
    if (longPress)
      longPress();
    Serial.printf("%s 长按\n", name);
    key.state = LONG_PRESSED;
    key.skipNextSingle = true;
  }

  if ((key.state == PRESSED || key.state == LONG_PRESSED) && digitalRead(key.pin) == HIGH)
  {
    if (key.state == PRESSED)
    {
      key.lastPress = now;
      key.state = WAIT_SECOND_PRESS;
    }
    else
    {
      key.state = IDLE; // 长按后松开
    }
  }

  if (key.state == WAIT_SECOND_PRESS && (now - key.lastPress > DOUBLE_CLICK_THRESHOLD))
  {
    if (!key.skipNextSingle && singleClick)
    {
      singleClick();
      Serial.printf("%s 单击\n", name);
    }
    key.state = IDLE;
  }
}

// 回调函数示例（你可按需修改）
void camSingleClick() { displayTask_PhotoSave(); }
void camDoubleClick()
{
  keyTask_LEDFlash(true);
  displayTask_PhotoSave();
  keyTask_LEDFlash(false);
}
void camLongPress()
{
  keyTask_LEDFlash(true);
  displayTask_PhotoSave();
  keyTask_LEDFlash(false);
}

void topSingleClick() { btTOPstate = 1; }
void topDoubleClick() { Serial.println("上键双击触发"); }
void topLongPress() { Serial.println("上键长按触发"); }

void midSingleClick() { btMIDstate = !btMIDstate; }
void midDoubleClick() { Serial.println("中键双击触发"); }
void midLongPress() { Serial.println("中键长按触发"); }

void downSingleClick() { btDownstate = 1; }
void downDoubleClick() { Serial.println("下键双击触发"); }
void downLongPress() { Serial.println("下键长按触发"); }

void keyTask(void *pvParameters)
{
  while (1)
  {
    handleKey(keys[0], "拍照键", btMIDstate, camSingleClick, camDoubleClick, camLongPress);
    handleKey(keys[1], "上键", btTOPstate, topSingleClick, topDoubleClick, topLongPress);
    handleKey(keys[2], "中键", btMIDstate, midSingleClick, midDoubleClick, midLongPress);
    handleKey(keys[3], "下键", btDownstate, downSingleClick, downDoubleClick, downLongPress);

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void keyTask_LEDFlash(bool on)
{
  // 控制LED闪烁
  if (on)
  {
    digitalWrite(LED_FLASH_NUM, LOW); // LED亮
  }
  else
  {
    digitalWrite(LED_FLASH_NUM, HIGH); // LED灭
  }
}
