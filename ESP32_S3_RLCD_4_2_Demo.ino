#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "display_manager.h"
#include "config.h"

const char* ntpServer1 = "ntp.aliyun.com";
const char* ntpServer2 = "ntp.tencent.com";
const char* ntpServer3 = "cn.pool.ntp.org";
const long  gmtOffset_sec      = 8 * 3600;   // 北京时间
const int   daylightOffset_sec = 0;

static uint32_t wifiConnectStartedMs = 0;
static uint32_t wifiRetryAtMs = 0;
static bool ntpStarted = false;
static bool ntpSynced = false;

void initWiFiAndNTP() {
  // 不能在这里阻塞 20~30 秒：LD2450 在上电后立刻连续输出帧，
  // 阻塞等待会让串口缓冲溢出，表现为雷达界面一直没有数据。
  Serial.printf("[WiFi] Connecting to %s (non-blocking)\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiConnectStartedMs = millis();
  wifiRetryAtMs = 0;
}

void serviceWiFiAndNTP() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiRetryAtMs == 0 && now - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println("[WiFi] Connect timeout; radar remains fully available, retry in 30 s.");
      wifiRetryAtMs = now + 30000UL;
    } else if (wifiRetryAtMs != 0 && (int32_t)(now - wifiRetryAtMs) >= 0) {
      Serial.println("[WiFi] Retrying...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiConnectStartedMs = now;
      wifiRetryAtMs = 0;
    }
    return;
  }

  if (!ntpStarted) {
    Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    ntpStarted = true;
    Serial.println("[NTP] Sync started.");
  }

  if (!ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 1)) {
      ntpSynced = true;
      Serial.println("[NTP] Beijing time synchronized.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32-S3-RLCD-4.2 + LD2450 Radar ===");

  displayManagerBegin();
  initWiFiAndNTP();

  Serial.println("[System] Ready");
}

void loop() {
  serviceWiFiAndNTP();
  displayManagerLoop();
  delay(2);
}
