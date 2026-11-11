#pragma once
#include <Arduino.h>

// 初始化TF卡(SD_MMC 1-bit模式)。成功返回true。
bool sdManagerBegin();

// 返回简短状态描述，用于屏幕显示，例如 "OK 15.9GB" 或 "not found"
String sdManagerGetStatusText();

// 简单的写入/读取自测(可选调用，用于验证TF卡工作正常)
bool sdManagerSelfTest();
