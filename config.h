#pragma once

// ===================== LCD 引脚（Waveshare ESP32-S3-RLCD-4.2） =====================
#define LCD_PIN_SCK     11
#define LCD_PIN_MOSI    12
#define LCD_PIN_DC      5
#define LCD_PIN_CS      40
#define LCD_PIN_RST     41

// ===================== LD2450 串口引脚 =====================
// 重要：本板 GPIO16 接到了板载 ES8311 音频 MCLK，不能作为雷达 RX 使用。
// 请接到 2x8 扩展排针右侧丝印的 RXD、TXD：它们分别是 GPIO44、GPIO43。
// 图片中右侧从上往下第 5 行为 TXD、第 6 行为 RXD。使用 UART1（可通过 GPIO Matrix 映射），
// 不会占用 USB 串口日志。接线：LD2450 TX -> RXD / GPIO44，LD2450 RX -> TXD / GPIO43，GND 必须共地。
// 供电务必接 VBUS/5V（LD2450 要求 5V、>200mA），不可接板上的 3V3；其 UART 输出为 3.3V TTL。
#define LD2450_RX_PIN   44      // ESP32 RX  <- LD2450 TX（排针右侧 RXD）
#define LD2450_TX_PIN   43      // ESP32 TX  -> LD2450 RX（排针右侧 TXD）
#define LD2450_BAUD     256000
#define LD2450_RX_BUFFER_SIZE  2048

// 串口每秒输出一次连接诊断。需要抓取原始字节时可临时设为 true。
#define LD2450_DEBUG_RAW false

// ===================== SD 卡引脚（板载 TF 卡槽，1-bit SDMMC） =====================
#define SD_PIN_CLK      39
#define SD_PIN_CMD      38
#define SD_PIN_D0       21
#define SD_MOUNT_POINT  "/sdcard"

// ===================== WiFi 配置 =====================
#define WIFI_SSID               "your_wifi_ssid"
#define WIFI_PASSWORD           "your_wifi_password"

// AP 模式备用（可选）
#define WIFI_AP_SSID            "ESP32-S3-Radar"
#define WIFI_AP_PASSWORD        "12345678"
#define WIFI_AP_FALLBACK        false
#define WIFI_CONNECT_TIMEOUT_MS 15000
