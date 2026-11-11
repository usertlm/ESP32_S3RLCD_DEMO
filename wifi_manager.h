#pragma once
#include <Arduino.h>

// Wi-Fi 连接状态
enum class WifiState {
  IDLE,
  CONNECTING,
  CONNECTED_STA,
  AP_MODE,
  FAILED
};

// 初始化并尝试以STA模式连接路由器；超时后可退回AP模式(取决于config.h中WIFI_AP_FALLBACK)
void wifiManagerBegin();

// 需在loop()中周期调用，用于非阻塞地维护/重连Wi-Fi
void wifiManagerLoop();

// 获取当前状态
WifiState wifiManagerGetState();

// 获取用于显示的状态简述，例如 "STA 192.168.1.23" 或 "AP ESP32-S3-RLCD"
String wifiManagerGetStatusText();
