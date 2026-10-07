#pragma once
#include <Arduino.h>

class LGFX;

#define NX_CONFIG_MAGIC   0x584E  // "NX" in ASCII
#define NX_CONFIG_VERSION 1

// Packed 64-byte binary structure stored in /sys/config.bin on ZDSD NAND
struct __attribute__((packed)) DeviceConfig {
  // 1. Header (8 bytes)
  uint16_t magic;           // 0x584E
  uint8_t  version;         // Schema version (1)
  uint8_t  reserved1;       // Alignment padding
  uint16_t structSize;      // sizeof(DeviceConfig) = 64
  uint16_t crc16;           // CCITT CRC-16 of payload (bytes 8..63)

  // 2. Display Settings (8 bytes)
  uint8_t  brightnessPercent;   // 10..100
  uint8_t  autoDimEnabled;      // 0: OFF, 1: ON
  uint8_t  tiltMode;            // 0: OFF, 1: SENSITIVE, 2: BALANCED, 3: SLUGGISH
  uint8_t  wristCoverSleep;     // 0: OFF, 1: ON
  uint16_t screenTimeoutSec;    // 0..300s (0 = NEVER)
  uint16_t backlightFadeMs;     // 0..2000ms

  // 3. Audio / Buzzer Settings (6 bytes)
  uint8_t  silentMode;           // 0: UNMUTED, 1: MUTED
  uint8_t  buzzerVolumePercent;  // 10..100%
  uint8_t  buzzerTickDurationMs; // 2..30ms
  uint8_t  reservedAudio;        // Alignment padding
  uint16_t buzzerBaseFreqHz;     // 1600..4400Hz

  // 4. Notifications & Alerts (4 bytes)
  uint8_t  notifMode;           // 0: SILENT, 1: ALL, 2: SOUND, 3: LIGHTS
  uint8_t  matrixLedAlerts;     // 0: OFF, 1: ON
  uint8_t  hrmReminderIdx;      // 0: OFF, 1: 30m, 2: 1h, 3: 2h
  uint8_t  reservedNotif;

  // 5. NeoPixel Matrix (NPM) (4 bytes)
  uint8_t  npmPower;             // 0: OFF, 1: ON
  uint8_t  npmBrightnessPercent; // 10..100%
  uint8_t  npmPatternIdx;        // 0..5
  uint8_t  reservedNpm;

  // 6. Power & System (4 bytes)
  uint8_t  ecoMode;              // 0: OFF, 1: ON
  uint8_t  autoStandby;          // 0: OFF, 1: ON
  uint8_t  sensorSleep;          // 0: OFF, 1: ON
  uint8_t  reservedPower;

  // 7. AI & Sensing (4 bytes)
  uint8_t  nxAisCoProc;          // 0: OFF, 1: ON
  uint8_t  adaptiveSensing;      // 0: OFF, 1: ON
  uint16_t reservedAi;

  // 8. Reserved expansion buffer (26 bytes -> total struct size = 64 bytes)
  uint8_t  futureExpansion[26];
};

class StorageManager {
private:
  static bool sdAvailable;
  static DeviceConfig activeConfig;
  static LGFX* displayRef;

public:
  static bool begin(LGFX* display = nullptr);
  static bool isAvailable();
  static bool loadConfig(DeviceConfig& config);
  static bool saveConfig(const DeviceConfig& config);
  static void applyConfigToHAL(const DeviceConfig& config);
  static void extractConfigFromHAL(DeviceConfig& config);
  static void setDefaults(DeviceConfig& config);
  static DeviceConfig& getActiveConfig();
  static uint16_t computeCRC(const DeviceConfig& config);
};
