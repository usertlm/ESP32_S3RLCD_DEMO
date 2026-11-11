#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>

static WifiState g_state = WifiState::IDLE;
static unsigned long g_connectStartMs = 0;
static unsigned long g_lastRetryMs = 0;

static void startStaConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // 减少Wi-Fi休眠导致的响应延迟
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  g_connectStartMs = millis();
  g_state = WifiState::CONNECTING;
  Serial.printf("[WiFi] 正在连接 SSID: %s ...\n", WIFI_SSID);
}

static void startApFallback() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  g_state = WifiState::AP_MODE;
  Serial.printf("[WiFi] STA连接失败，已切换到AP模式。SSID: %s  IP: %s\n",
                WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void wifiManagerBegin() {
  startStaConnect();
}

void wifiManagerLoop() {
  switch (g_state) {
    case WifiState::CONNECTING: {
      if (WiFi.status() == WL_CONNECTED) {
        g_state = WifiState::CONNECTED_STA;
        Serial.printf("[WiFi] 连接成功！IP: %s\n", WiFi.localIP().toString().c_str());
        break;
      }
      if (millis() - g_connectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[WiFi] 连接超时。");
        if (WIFI_AP_FALLBACK) {
          startApFallback();
        } else {
          g_state = WifiState::FAILED;
        }
      }
      break;
    }

    case WifiState::CONNECTED_STA: {
      // 掉线自动重连
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] 连接已断开，尝试重连...");
        startStaConnect();
      }
      break;
    }

    case WifiState::FAILED: {
      // 每隔一段时间重试一次STA连接
      if (millis() - g_lastRetryMs > 30000UL) {
        g_lastRetryMs = millis();
        startStaConnect();
      }
      break;
    }

    case WifiState::AP_MODE:
    case WifiState::IDLE:
    default:
      break;
  }
}

WifiState wifiManagerGetState() {
  return g_state;
}

String wifiManagerGetStatusText() {
  switch (g_state) {
    case WifiState::CONNECTING:
      return "WiFi: connecting...";
    case WifiState::CONNECTED_STA:
      return "STA " + WiFi.localIP().toString();
    case WifiState::AP_MODE:
      return "AP " + WiFi.softAPIP().toString();
    case WifiState::FAILED:
      return "WiFi: failed";
    default:
      return "WiFi: idle";
  }
}
