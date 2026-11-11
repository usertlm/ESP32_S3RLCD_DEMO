#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

struct RadarTarget {
  bool     valid       = false;
  int16_t  x_mm        = 0;
  int16_t  y_mm        = 0;
  int16_t  speed_cm_s  = 0;
  uint16_t resolution  = 0;
  float    distance_m  = 0;
  float    angle_deg   = 0;
};

class LD2450 {
public:
  static const int MAX_TARGETS = 3;
  static const int FRAME_SIZE  = 30;

  LD2450(HardwareSerial& serial);
  void begin(int rxPin, int txPin, uint32_t baud = 256000);
  bool update();
  const RadarTarget& getTarget(int idx) const;
  int  getValidCount() const;
  bool isModulePresent() const { return _modulePresent; }
  uint32_t getFrameCount() const { return _frameCount; }
  uint32_t getBadFrameCount() const { return _badFrameCount; }
  uint32_t getByteCount() const { return _byteCount; }
  uint16_t getFrameRate() const { return _frameRate; }
  uint32_t getLastFrameAgeMs() const;

private:
  HardwareSerial& _serial;
  uint8_t         _buf[FRAME_SIZE];
  int             _idx = 0;
  RadarTarget     _targets[MAX_TARGETS];
  bool            _modulePresent = false;
  uint32_t        _lastFrameMs   = 0;
  uint32_t        _frameCount    = 0;
  uint32_t        _badFrameCount = 0;
  uint32_t        _byteCount     = 0;
  uint32_t        _lastRateMs    = 0;
  uint16_t        _framesInRateWindow = 0;
  uint16_t        _frameRate     = 0;
  uint32_t        _lastByteMs    = 0;

  static int16_t decodeSigned(uint8_t lo, uint8_t hi);
  void    resetParser();
  bool    consume(uint8_t byte);
  void    parseFrame();
};
