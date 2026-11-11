#include "sdcard_manager.h"
#include "config.h"
#include "FS.h"
#include "SD_MMC.h"

static bool g_sdReady = false;
static uint64_t g_cardSizeBytes = 0;

bool sdManagerBegin() {
  // 使用 SDMMC 1-bit 模式(仅需CLK/CMD/D0三条线，占用引脚最少)
  // !! 若初始化失败，请优先检查 config.h 中 SD_PIN_* 是否与你的板子相符 !!
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);

  if (!SD_MMC.begin(SD_MOUNT_POINT, /*mode1bit=*/true)) {
    Serial.println("[SD] 初始化失败：请检查是否插卡、卡是否为FAT32格式，"
                    "以及 config.h 中的 SD_PIN_CLK/SD_PIN_CMD/SD_PIN_D0 是否正确。");
    g_sdReady = false;
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD] 未检测到TF卡。");
    g_sdReady = false;
    return false;
  }

  g_cardSizeBytes = SD_MMC.cardSize();
  g_sdReady = true;

  Serial.printf("[SD] 初始化成功，容量: %.2f GB\n",
                (double)g_cardSizeBytes / (1024.0 * 1024.0 * 1024.0));
  return true;
}

String sdManagerGetStatusText() {
  if (!g_sdReady) return "not found";
  char buf[24];
  snprintf(buf, sizeof(buf), "OK %.1fGB", (double)g_cardSizeBytes / (1024.0 * 1024.0 * 1024.0));
  return String(buf);
}

bool sdManagerSelfTest() {
  if (!g_sdReady) return false;

  const char *path = "/rlcd_selftest.txt";

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("[SD] 自测失败：无法创建文件。");
    return false;
  }
  f.printf("selftest at millis=%lu\n", millis());
  f.close();

  f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.println("[SD] 自测失败：无法读取文件。");
    return false;
  }
  Serial.print("[SD] 读回内容: ");
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();

  return true;
}
