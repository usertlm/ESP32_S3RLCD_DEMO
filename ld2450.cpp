#include "ld2450.h"
#include "config.h"
#include <math.h>

LD2450::LD2450(HardwareSerial& serial) : _serial(serial) {}

void LD2450::begin(int rxPin, int txPin, uint32_t baud) {
  // RLCD 全屏刷新期间也要能容纳多帧数据，默认的 UART FIFO/缓冲区太小。
  _serial.setRxBufferSize(LD2450_RX_BUFFER_SIZE);
  _serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  resetParser();
  memset(_targets, 0, sizeof(_targets));
  _modulePresent = false;
  _lastFrameMs = 0;
  _frameCount = _badFrameCount = _byteCount = 0;
  _lastRateMs = millis();
  _framesInRateWindow = _frameRate = 0;
  _lastByteMs = 0;
}

int16_t LD2450::decodeSigned(uint8_t lo, uint8_t hi) {
  // LD2450 不是二补码：bit15 是符号位，1 为正、0 为负。
  const int16_t magnitude = (int16_t)(((uint16_t)hi << 8 | lo) & 0x7FFF);
  return (hi & 0x80) ? magnitude : -magnitude;
}

void LD2450::resetParser() {
  _idx = 0;
}

bool LD2450::consume(uint8_t byte) {
  // 实时数据帧固定为：AA FF 03 00 + 3 * 8 字节目标 + 55 CC。
  // 用状态机而不是一次性 readBytes，避免开机半帧、噪声或丢字节导致永久失步。
  static const uint8_t header[] = {0xAA, 0xFF, 0x03, 0x00};

  if (_idx < (int)sizeof(header)) {
    if (byte == header[_idx]) {
      _buf[_idx++] = byte;
      return false;
    }

    // 错误字节本身可能是下一帧的首字节，保留它以便立即重同步。
    _idx = (byte == header[0]) ? 1 : 0;
    if (_idx == 1) _buf[0] = byte;
    return false;
  }

  _buf[_idx++] = byte;
  if (_idx < FRAME_SIZE) return false;

  const bool valid = _buf[FRAME_SIZE - 2] == 0x55 && _buf[FRAME_SIZE - 1] == 0xCC;
  if (!valid) {
    ++_badFrameCount;
    // 尾部坏帧可能紧接着新帧；从最后一个 AA 继续解析。
    int next = 0;
    for (int i = FRAME_SIZE - 1; i >= 0; --i) {
      if (_buf[i] == 0xAA) {
        _buf[0] = 0xAA;
        next = 1;
        break;
      }
    }
    _idx = next;
    return false;
  }

  parseFrame();
  _modulePresent = true;
  _lastFrameMs = millis();
  ++_frameCount;
  ++_framesInRateWindow;
  resetParser();
  return true;
}

bool LD2450::update() {
  bool gotFrame = false;

  // 防止断线或干扰后残留的半帧长期占用状态机。
  if (_idx > 0 && millis() - _lastByteMs > 100) resetParser();

  while (_serial.available()) {
    const uint8_t byte = (uint8_t)_serial.read();
    ++_byteCount;
    _lastByteMs = millis();
#if LD2450_DEBUG_RAW
    Serial.printf("%02X ", byte);
#endif
    if (consume(byte)) gotFrame = true;
  }

  const uint32_t now = millis();
  if (now - _lastRateMs >= 1000) {
    _frameRate = (uint16_t)((_framesInRateWindow * 1000UL) / (now - _lastRateMs));
    _framesInRateWindow = 0;
    _lastRateMs = now;
  }

  if (_lastFrameMs == 0 || now - _lastFrameMs > 1200) {
    _modulePresent = false;
  }
  return gotFrame;
}

void LD2450::parseFrame() {
  for (int i = 0; i < MAX_TARGETS; i++) {
    int offset = 4 + i * 8;
    int16_t x = decodeSigned(_buf[offset],     _buf[offset + 1]);
    int16_t y = decodeSigned(_buf[offset + 2], _buf[offset + 3]);
    int16_t s = decodeSigned(_buf[offset + 4], _buf[offset + 5]);
    uint16_t res = (uint16_t)(_buf[offset + 6] | (_buf[offset + 7] << 8));

    RadarTarget& t = _targets[i];
    // 固件对未跟踪槽位会返回 resolution=0。用它判定能避免目标在原点时被误删。
    if (res == 0) {
      t = RadarTarget();
    } else {
      t.valid       = true;
      t.x_mm        = x;
      t.y_mm        = y;
      t.speed_cm_s  = s;
      t.resolution  = res;
      t.distance_m  = sqrtf((float)x * x + (float)y * y) / 1000.0f;
      t.angle_deg   = atan2f((float)x, (float)y) * 180.0f / PI;
    }
  }
}

const RadarTarget& LD2450::getTarget(int idx) const {
  static RadarTarget empty;
  if (idx < 0 || idx >= MAX_TARGETS) return empty;
  return _targets[idx];
}

int LD2450::getValidCount() const {
  int cnt = 0;
  for (int i = 0; i < MAX_TARGETS; i++) {
    if (_targets[i].valid) cnt++;
  }
  return cnt;
}

uint32_t LD2450::getLastFrameAgeMs() const {
  if (_lastFrameMs == 0) return 0xFFFFFFFFUL;
  return millis() - _lastFrameMs;
}
