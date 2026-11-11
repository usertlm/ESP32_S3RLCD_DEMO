# ESP32-S3-RLCD-4.2 Arduino 综合示例

包含 **WiFi 联网 + 屏幕显示 + TF卡读写** 三部分功能的 Arduino IDE 工程。

## 文件结构

```
ESP32_S3_RLCD_4_2_Demo/
├── ESP32_S3_RLCD_4_2_Demo.ino   # 主程序 (setup/loop)
├── config.h                     # 所有引脚 & WiFi 配置(重点看这个文件)
├── wifi_manager.h / .cpp        # WiFi连接与自动重连/AP回退
├── display_manager.h / .cpp     # 屏幕(U8g2驱动ST7305)
├── sdcard_manager.h / .cpp      # TF卡(SD_MMC)
└── README.md                    # 本说明文件
```

> Arduino IDE 要求：整个文件夹名必须和主 `.ino` 文件同名，
> 请保持 `ESP32_S3_RLCD_4_2_Demo` 文件夹结构不变，用 Arduino IDE
> 直接打开 `ESP32_S3_RLCD_4_2_Demo.ino` 即可，其余 `.h/.cpp` 会自动一起编译。

## 1. 开发环境安装

1. Arduino IDE 中安装 **arduino-esp32 v3.3.0 及以上**（Waveshare 官方要求的最低版本）。
2. 库管理器中安装 **U8g2**（建议装最新版，需要支持
   `U8G2_ST7305_300X400_1_4W_HW_SPI` 这个构造函数，2024年之后的版本才有）。

## 2. 开发板参数 (Tools 菜单)

| 选项 | 建议值 |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) 或按需选择 |
| USB CDC On Boot | Enabled（方便看Serial输出） |

## 3. 修改配置

打开 `config.h`：

```cpp
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
```

改成你自己的路由器信息即可，WiFi、屏幕相关引脚都已经内置好，无需修改。

## 3.1 LD2450 接线（本工程默认配置）

> **请不要把 LD2450 接到 GPIO16。** 此脚连接板载 ES8311 音频时钟，不是可用的雷达输入脚。

| LD2450 | ESP32-S3-RLCD-4.2 的 2×8 扩展排针 P1 |
|---|---|
| `5V` | `VBUS / 5V`（不是 `3V3`） |
| `GND` | `GND` |
| `TX` | 右侧丝印 `RXD / GPIO44` |
| `RX` | 右侧丝印 `TXD / GPIO43` |

LD2450 的官方要求是 **DC 5V、供电能力大于 200mA**，平均工作电流约 120mA；它的 UART
输出电平是 3.3V。ESP32 通过 USB 供电时，排针 `VBUS` 才应为 5V；若采用其他供电方式，请先用
万用表确认 LD2450 的 `5V-GND` 之间为约 5V，并让两块板的 GND 共地。

Arduino IDE 的 **Tools > USB CDC On Boot 必须为 Enabled**，这样下载线使用 ESP32-S3 的原生 USB，
排针右侧的 `TXD/RXD` 才能完全留给雷达。排障时可以只连接 `5V`、`GND`、`TX -> RXD` 三根线：LD2450
本身会主动输出帧，不接 `RX` 也应该出现 `online=1`；确认成功后再接 `RX -> P1-14`。

程序会每秒在串口输出一行诊断：

- `rx=0`：ESP32 没收到任何电平跳变，优先检查 5V 供电、共地和 `TX -> RXD`。
- `rx` 增长但 `frames=0`：已收到数据但不是 LD2450 的实时帧；确认波特率 `256000, 8N1`，并在 PC 测试后对雷达重新断电上电。
- `online=1`、`fps` 约为 `10`：模块已识别，右侧开始显示最多 3 个稳定追踪 ID、速度、角度和轨迹。

## 4. 关于SD卡引脚（重要，请务必阅读）

屏幕(SPI: MOSI=12 / SCK=11 / DC=5 / CS=40 / RST=41)、I2C(SDA=13 / SCL=14)、
电池ADC(GPIO4)、按键(BOOT=0 / KEY=18) 这几组引脚，已经通过 Waveshare 官方
ESPHome 示例配置、开源 MicroPython 驱动仓库、Zephyr 官方板级支持文件三方
交叉核对一致，可以直接使用。

**但TF卡(SD)的具体GPIO编号，Waveshare官方并未在公开文档中给出**：
官方 Arduino 例程 `06_SD_Card` 是通过它自己封装的 `CustomSDPort` 类调用
SDMMC 驱动的，源码没有公开具体引脚定义。因此本工程 `config.h` 里的：

```cpp
#define SD_PIN_CLK  39
#define SD_PIN_CMD  38
#define SD_PIN_D0   21
```

是**占位符**，不保证和你手上的板子一致。烧录后如果串口打印
`[SD] 初始化失败`，请按下面方法核实：

1. **查官方原理图 PDF**（最可靠）：
   https://files.waveshare.com/wiki/ESP32-S3-RLCD-4.2/ESP32-S3-RLCD-4.2-schematic.pdf
   在原理图里搜索 `SD_CLK` / `SD_CMD` / `SD_DATA0`（或 `TF_CLK`等字样），
   找到连接的 ESP32-S3 GPIO 编号，回填到 `config.h`。

2. **下载官方例程包对照**：
   https://docs.waveshare.com/ESP32-S3-RLCD-4.2/Resources-And-Documents
   页面里的示例代码压缩包，解压后搜索 `CustomSDPort` 或
   `SD_MMC.setPins` 相关调用，通常能在库源码里找到确切的引脚宏定义。

3. **实在找不到时，可以逐一尝试**：ESP32-S3 的 SDMMC 外设可以通过
   GPIO Matrix 映射到几乎任意引脚，若原理图/例程都拿不到，可以断电后
   用万用表通断挡测量TF卡座的 CLK/CMD/DATA0 焊盘与主控之间的连线。

## 5. 功能说明

- **WiFi**：开机自动以 STA 模式连接 `config.h` 中配置的路由器；超过
  `WIFI_CONNECT_TIMEOUT_MS`（默认15秒）仍未连上，会自动切换到AP热点模式
  （热点名 `ESP32-S3-RLCD`，密码 `12345678`，可在 `config.h` 修改）；
  STA连接成功后如果掉线，会自动尝试重连。

- **屏幕**：使用 U8g2 库的 `U8G2_ST7305_300X400_1_4W_HW_SPI` 硬件SPI驱动，
  每2秒刷新一次，显示WiFi状态、SD卡状态和开机运行时间。反射式屏幕刷新
  速度较慢，代码里已经做了节流(不要在 `loop()` 里去掉这个节流逻辑，
  否则刷新过于频繁会导致画面拖影或响应迟缓)。

- **TF卡**：使用 `SD_MMC` 库的 1-bit 模式（只占用CLK/CMD/D0三根线），
  开机会自动做一次写入+读回的自测，串口能看到自测结果。

## 6. 常见问题

- **编译报错找不到 `U8G2_ST7305_300X400_1_4W_HW_SPI`**：说明你的 U8g2
  库版本太旧，去库管理器更新到最新版本即可。
- **屏幕花屏或不显示**：检查 `config.h` 里 `LCD_PIN_*` 是否和你板子的
  丝印/原理图一致；也可以尝试把 `u8g2.setBusClock()` 调低（比如500000）。
- **SD卡初始化失败**：参考第4节。
- **WiFi连不上**：确认路由器是 2.4GHz（ESP32-S3 不支持5GHz），
  SSID/密码没有多余空格。
