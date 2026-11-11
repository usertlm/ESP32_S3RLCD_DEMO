#include "display_manager.h"
#include "config.h"
#include <time.h>
#include <math.h>

// ===================== 硬件对象 =====================
static ST7305_U8g2 lcd(LCD_PIN_SCK, LCD_PIN_MOSI, LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_RST);
static U8G2 *u8g2 = nullptr;

// 使用独立硬件 UART。不要使用 SoftwareSerial：LD2450 的默认速率为 256000。
static HardwareSerial RadarSerial(1);
static LD2450 radar(RadarSerial);

// ===================== 稳定的三目标追踪 =====================
// LD2450 的输出槽位 1~3 并不是永久 ID。这里按最近位置将每帧结果关联为稳定轨迹 ID，
// 人体相距较近/交叉时也比直接按原始槽位画轨迹更自然。
static const uint8_t TRACK_COUNT = LD2450::MAX_TARGETS;
static const uint8_t TRAIL_LEN = 18;
static const uint32_t TRACK_TIMEOUT_MS = 900;
static const uint32_t TRAIL_SAMPLE_MS = 180;
static const int32_t TRACK_MATCH_DISTANCE_MM = 1200;

struct TrailPoint {
  int16_t x_mm;
  int16_t y_mm;
  bool valid;
};

struct Track {
  RadarTarget target;
  TrailPoint trail[TRAIL_LEN];
  uint8_t trailHead;
  uint8_t trailCount;
  uint32_t lastSeenMs;
  uint32_t lastTrailMs;
  bool active;
};

static Track tracks[TRACK_COUNT];

// ===================== UI 布局（旋转后屏幕为 400 x 300） =====================
static const int SCREEN_W = 400;
static const int SCREEN_H = 300;
static const int HEADER_H = 32;
static const int INFO_X = 285;
static const int RADAR_CX = 142;
static const int RADAR_CY = 279;
static const int RADAR_R = 132;
static const float MAX_DIST_M = 6.0f;
static const uint32_t DISPLAY_REFRESH_MS = 150;

static void mapToScreen(float x_m, float y_m, int &sx, int &sy) {
  const float scale = (float)RADAR_R / MAX_DIST_M;
  sx = RADAR_CX + (int)(x_m * scale);
  sy = RADAR_CY - (int)(y_m * scale);
}

static void clearTrack(Track &track) {
  memset(&track, 0, sizeof(track));
}

static void addTrailPoint(Track &track, uint32_t now) {
  if (track.lastTrailMs != 0 && now - track.lastTrailMs < TRAIL_SAMPLE_MS) return;

  TrailPoint &point = track.trail[track.trailHead];
  point.x_mm = track.target.x_mm;
  point.y_mm = track.target.y_mm;
  point.valid = true;
  track.trailHead = (track.trailHead + 1) % TRAIL_LEN;
  if (track.trailCount < TRAIL_LEN) ++track.trailCount;
  track.lastTrailMs = now;
}

static void assignMeasurement(Track &track, const RadarTarget &measurement, uint32_t now) {
  track.target = measurement;
  track.active = true;
  track.lastSeenMs = now;
  addTrailPoint(track, now);
}

static void expireTracks(uint32_t now) {
  for (uint8_t i = 0; i < TRACK_COUNT; ++i) {
    if (tracks[i].active && now - tracks[i].lastSeenMs > TRACK_TIMEOUT_MS) {
      clearTrack(tracks[i]);
    }
  }
}

static int32_t squaredDistance(const RadarTarget &a, const RadarTarget &b) {
  const int32_t dx = (int32_t)a.x_mm - b.x_mm;
  const int32_t dy = (int32_t)a.y_mm - b.y_mm;
  return dx * dx + dy * dy;
}

static int findTrackToReuse() {
  for (uint8_t i = 0; i < TRACK_COUNT; ++i) {
    if (!tracks[i].active) return i;
  }

  // 三个槽位已满而出现全新目标时，复用最久未更新的 ID，并清空其旧轨迹。
  int oldest = 0;
  for (uint8_t i = 1; i < TRACK_COUNT; ++i) {
    if (tracks[i].lastSeenMs < tracks[oldest].lastSeenMs) oldest = i;
  }
  return oldest;
}

static void updateTracks(const RadarTarget measurements[TRACK_COUNT]) {
  const uint32_t now = millis();
  expireTracks(now);

  bool measurementUsed[TRACK_COUNT] = {false};
  bool trackMatched[TRACK_COUNT] = {false};
  const int32_t maxDistanceSq = TRACK_MATCH_DISTANCE_MM * TRACK_MATCH_DISTANCE_MM;

  // 先全局贪心匹配最近的一对，避免因传感器槽位交换而把两人的轨迹连在一起。
  while (true) {
    int bestTrack = -1;
    int bestMeasurement = -1;
    int32_t bestDistanceSq = maxDistanceSq + 1;
    for (uint8_t t = 0; t < TRACK_COUNT; ++t) {
      if (!tracks[t].active || trackMatched[t]) continue;
      for (uint8_t m = 0; m < TRACK_COUNT; ++m) {
        if (!measurements[m].valid || measurementUsed[m]) continue;
        const int32_t distanceSq = squaredDistance(tracks[t].target, measurements[m]);
        if (distanceSq < bestDistanceSq) {
          bestDistanceSq = distanceSq;
          bestTrack = t;
          bestMeasurement = m;
        }
      }
    }
    if (bestTrack < 0) break;
    assignMeasurement(tracks[bestTrack], measurements[bestMeasurement], now);
    trackMatched[bestTrack] = true;
    measurementUsed[bestMeasurement] = true;
  }

  // 未匹配的雷达目标为新目标，分配空闲 ID（或复用最久未更新的 ID）。
  for (uint8_t m = 0; m < TRACK_COUNT; ++m) {
    if (!measurements[m].valid || measurementUsed[m]) continue;
    const int trackIndex = findTrackToReuse();
    clearTrack(tracks[trackIndex]);
    assignMeasurement(tracks[trackIndex], measurements[m], now);
  }
}

static uint8_t activeTrackCount() {
  uint8_t count = 0;
  for (uint8_t i = 0; i < TRACK_COUNT; ++i) {
    if (tracks[i].active) ++count;
  }
  return count;
}

// ===================== 绘制 =====================
static void drawHeader(bool moduleOnline) {
  u8g2->drawHLine(0, HEADER_H, SCREEN_W);
  u8g2->drawDisc(10, 14, 3);
  if (!moduleOnline) u8g2->drawCircle(10, 14, 5);

  u8g2->setFont(u8g2_font_6x10_tf);
  u8g2->drawStr(20, 18, moduleOnline ? "LD2450  TRACKING" : "LD2450  SEARCHING");

  char clockText[12] = "--:--:--";
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1)) {
    strftime(clockText, sizeof(clockText), "%H:%M:%S", &timeinfo);
  }
  // 右上角时钟，实时数据区域完全独立。
  u8g2->drawStr(349, 18, clockText);
}

static void drawRadarBackground() {
  u8g2->drawFrame(2, HEADER_H + 5, INFO_X - 6, SCREEN_H - HEADER_H - 7);

  const uint8_t upperHalf = U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT;
  u8g2->drawCircle(RADAR_CX, RADAR_CY, RADAR_R, upperHalf);
  u8g2->drawCircle(RADAR_CX, RADAR_CY, RADAR_R * 2 / 3, upperHalf);
  u8g2->drawCircle(RADAR_CX, RADAR_CY, RADAR_R / 3, upperHalf);

  for (int angle = -60; angle <= 60; angle += 30) {
    const float radians = angle * PI / 180.0f;
    const int x2 = RADAR_CX + (int)(RADAR_R * sinf(radians));
    const int y2 = RADAR_CY - (int)(RADAR_R * cosf(radians));
    u8g2->drawLine(RADAR_CX, RADAR_CY, x2, y2);
  }

  u8g2->setFont(u8g2_font_5x7_tf);
  u8g2->drawStr(9, 47, "RADAR / 6m");
  u8g2->drawStr(RADAR_CX + 5, RADAR_CY - RADAR_R + 9, "6m");
  u8g2->drawStr(RADAR_CX + 5, RADAR_CY - RADAR_R * 2 / 3 + 9, "4m");
  u8g2->drawStr(RADAR_CX + 5, RADAR_CY - RADAR_R / 3 + 9, "2m");
  u8g2->drawStr(RADAR_CX - 7, RADAR_CY - 5, "0");
}

static void drawTracks() {
  for (uint8_t i = 0; i < TRACK_COUNT; ++i) {
    const Track &track = tracks[i];
    if (!track.active) continue;

    // 轨迹使用虚线，单色反射屏上仍能清楚地区分目标点与历史路径。
    for (uint8_t j = 1; j < track.trailCount; ++j) {
      const uint8_t p1 = (track.trailHead + TRAIL_LEN - track.trailCount + j - 1) % TRAIL_LEN;
      const uint8_t p2 = (track.trailHead + TRAIL_LEN - track.trailCount + j) % TRAIL_LEN;
      if (!track.trail[p1].valid || !track.trail[p2].valid || (j & 1)) continue;

      int x1, y1, x2, y2;
      mapToScreen(track.trail[p1].x_mm / 1000.0f, track.trail[p1].y_mm / 1000.0f, x1, y1);
      mapToScreen(track.trail[p2].x_mm / 1000.0f, track.trail[p2].y_mm / 1000.0f, x2, y2);
      u8g2->drawLine(x1, y1, x2, y2);
    }

    int sx, sy;
    mapToScreen(track.target.x_mm / 1000.0f, track.target.y_mm / 1000.0f, sx, sy);
    if (sx < 4 || sx >= INFO_X - 3 || sy < HEADER_H + 4 || sy >= SCREEN_H - 3) continue;

    u8g2->drawDisc(sx, sy, 4);
    u8g2->drawCircle(sx, sy, 7);
    char label[4];
    snprintf(label, sizeof(label), "T%u", i + 1);
    u8g2->setFont(u8g2_font_6x10_tf);
    u8g2->drawStr(sx + 9, sy + 3, label);
  }
}

static void drawInfoPanel(bool moduleOnline) {
  u8g2->drawVLine(INFO_X, HEADER_H, SCREEN_H - HEADER_H);
  u8g2->setFont(u8g2_font_6x10_tf);

  char heading[20];
  snprintf(heading, sizeof(heading), "TRACKS %u/3", activeTrackCount());
  u8g2->drawStr(INFO_X + 8, 47, heading);

  int y = 64;
  for (uint8_t i = 0; i < TRACK_COUNT; ++i) {
    const Track &track = tracks[i];
    u8g2->drawFrame(INFO_X + 5, y - 12, SCREEN_W - INFO_X - 10, 52);

    char line1[22];
    char line2[22];
    char line3[22];
    if (track.active) {
      snprintf(line1, sizeof(line1), "T%u  %.2fm", i + 1, track.target.distance_m);
      snprintf(line2, sizeof(line2), "V %+d cm/s", track.target.speed_cm_s);
      snprintf(line3, sizeof(line3), "A %+d  R%umm", (int)track.target.angle_deg, track.target.resolution);
    } else {
      snprintf(line1, sizeof(line1), "T%u  --", i + 1);
      snprintf(line2, sizeof(line2), "waiting");
      line3[0] = '\0';
    }

    u8g2->drawStr(INFO_X + 10, y, line1);
    u8g2->setFont(u8g2_font_5x7_tf);
    u8g2->drawStr(INFO_X + 10, y + 13, line2);
    if (line3[0]) u8g2->drawStr(INFO_X + 10, y + 25, line3);
    u8g2->setFont(u8g2_font_6x10_tf);
    y += 59;
  }

  char stats[30];
  snprintf(stats, sizeof(stats), "%s  F:%u/s", moduleOnline ? "ONLINE" : "NO FRAME", radar.getFrameRate());
  u8g2->setFont(u8g2_font_5x7_tf);
  u8g2->drawStr(INFO_X + 8, 251, stats);
  snprintf(stats, sizeof(stats), "RX:%lu ERR:%lu", (unsigned long)radar.getByteCount(),
           (unsigned long)radar.getBadFrameCount());
  u8g2->drawStr(INFO_X + 8, 264, stats);
  snprintf(stats, sizeof(stats), "FRAMES:%lu", (unsigned long)radar.getFrameCount());
  u8g2->drawStr(INFO_X + 8, 277, stats);
  u8g2->drawStr(INFO_X + 8, 290, "3-TARGET AUTO ID");
}

// ===================== 对外接口 =====================
void displayManagerBegin() {
  lcd.begin(0, U8G2_R1);
  u8g2 = lcd.getU8g2();

  Serial.printf("[LCD] Resolution: %d x %d\n", u8g2->getDisplayWidth(), u8g2->getDisplayHeight());
  Serial.printf("[LD2450] UART1 RX=GPIO%d TX=GPIO%d baud=%lu\n",
                LD2450_RX_PIN, LD2450_TX_PIN, (unsigned long)LD2450_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT == 0
  Serial.println("[LD2450] WARNING: enable Tools > USB CDC On Boot. UART0 pins 43/44 are reserved for radar.");
#endif

  u8g2->setFontMode(1);
  radar.begin(LD2450_RX_PIN, LD2450_TX_PIN, LD2450_BAUD);
  memset(tracks, 0, sizeof(tracks));

  u8g2->clearBuffer();
  u8g2->setFont(u8g2_font_7x13B_tf);
  u8g2->drawStr(93, 125, "LD2450 RADAR");
  u8g2->setFont(u8g2_font_6x10_tf);
  u8g2->drawStr(100, 147, "Starting receiver...");
  u8g2->sendBuffer();
}

void displayManagerLoop() {
  const bool gotFrame = radar.update();
  if (gotFrame) {
    RadarTarget measurements[TRACK_COUNT];
    for (uint8_t i = 0; i < TRACK_COUNT; ++i) measurements[i] = radar.getTarget(i);
    updateTracks(measurements);
  } else {
    expireTracks(millis());
  }

  // 诊断数字应在 update() 之后读取，否则 available() 往往已经被清空而没有意义。
  static uint32_t lastDebugMs = 0;
  const uint32_t now = millis();
  if (now - lastDebugMs >= 1000) {
    lastDebugMs = now;
    const uint32_t age = radar.getLastFrameAgeMs();
    Serial.printf("[LD2450] online=%d tracks=%u fps=%u rx=%lu frames=%lu bad=%lu age=%lums\n",
                  radar.isModulePresent(), activeTrackCount(), radar.getFrameRate(),
                  (unsigned long)radar.getByteCount(), (unsigned long)radar.getFrameCount(),
                  (unsigned long)radar.getBadFrameCount(), (unsigned long)age);
    if (!radar.isModulePresent()) {
      if (radar.getByteCount() == 0) {
        Serial.println("[LD2450] No RX bytes: check 5V/VBUS power, common GND, and LD2450 TX -> header RXD (GPIO44).");
      } else if (radar.getFrameCount() == 0) {
        Serial.println("[LD2450] RX is active but no valid frame: check 256000 8N1 and power-cycle the radar after PC testing.");
      }
    }
  }

  static uint32_t lastDrawMs = 0;
  if (now - lastDrawMs < DISPLAY_REFRESH_MS) return;
  lastDrawMs = now;

  u8g2->clearBuffer();
  drawHeader(radar.isModulePresent());
  drawRadarBackground();
  drawTracks();
  drawInfoPanel(radar.isModulePresent());
  u8g2->sendBuffer();
}

U8G2* displayGetU8g2() { return u8g2; }
LD2450& displayGetRadar() { return radar; }
