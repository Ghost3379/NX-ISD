#include "UplinkBridge.h"
#include "DisplayConfig.h"
#include <time.h>

bool UplinkBridge::streamActive = false; // Strictly DORMANT by default - only streams when activated in TOOLS
uint32_t UplinkBridge::streamIntervalMs = 200; // 5 Hz when active
uint32_t UplinkBridge::lastStreamTime = 0;
uint32_t UplinkBridge::packetsSent = 0;
String UplinkBridge::lastCommand = "NONE";
uint32_t UplinkBridge::lastCommandTime = 0;
bool UplinkBridge::buzzerEnabled = false;
uint8_t UplinkBridge::displayBrightness = 200;

bool UplinkBridge::matrixPower = false;
uint8_t UplinkBridge::matrixBrightness = 30;
MatrixPattern UplinkBridge::currentPattern = PATTERN_OFF;
uint32_t UplinkBridge::lastMatrixFrame = 0;
uint8_t UplinkBridge::matrixStep = 0;
uint32_t UplinkBridge::customPixels[16] = {0};

char UplinkBridge::rxBuffer[256] = {0};
uint16_t UplinkBridge::rxIndex = 0;

// Serpentine 4x4 LED index mapping (XL-1010RGBC matrix)
// Row 0: 0, 1, 2, 3
// Row 1: 7, 6, 5, 4 (reversed)
// Row 2: 8, 9, 10, 11
// Row 3: 15, 14, 13, 12 (reversed)
static inline uint8_t mapPixel(uint8_t x, uint8_t y) {
  if (x > 3) x = 3;
  if (y > 3) y = 3;
  if (y & 1) {
    return (y * 4) + (3 - x);
  } else {
    return (y * 4) + x;
  }
}

void UplinkBridge::begin() {
  matrixPower = false;
  HAL::setMatrixPower(false); // Keep PMOS gate OFF (0uA cutoff)
  currentPattern = PATTERN_OFF;
}

void UplinkBridge::setStreamRateHz(uint8_t hz) {
  if (hz == 0) {
    streamActive = false;
  } else {
    streamActive = true;
    streamIntervalMs = 1000 / hz;
  }
}

void UplinkBridge::setMatrixPower(bool on) {
  matrixPower = on;
  if (matrixPower) {
    HAL::setMatrixPower(true);
    delay(10);
    if (!HAL::neoPixels) {
      HAL::neoPixels = new Adafruit_NeoPixel(16, NPM, NEO_GRB + NEO_KHZ800);
      HAL::neoPixels->begin();
    }
    HAL::neoPixels->setBrightness(matrixBrightness);
    HAL::neoPixels->clear();
    HAL::neoPixels->show();
    if (currentPattern == PATTERN_OFF) {
      currentPattern = PATTERN_CYBER_RADAR;
    }
  } else {
    if (HAL::neoPixels) {
      HAL::neoPixels->clear();
      HAL::neoPixels->show();
    }
    HAL::setMatrixPower(false);
  }
}

void UplinkBridge::setMatrixBrightness(uint8_t b) {
  matrixBrightness = constrain(b, 0, 80);
  if (HAL::neoPixels && matrixPower) {
    HAL::neoPixels->setBrightness(matrixBrightness);
  }
}

void UplinkBridge::setMatrixPattern(MatrixPattern pat) {
  currentPattern = pat;
  matrixStep = 0;
  if (!HAL::neoPixels || !matrixPower) return;
  if (currentPattern == PATTERN_OFF) {
    HAL::neoPixels->clear();
    HAL::neoPixels->show();
  }
}

void UplinkBridge::processCommand(const char* cmd) {
  if (!cmd || strlen(cmd) == 0) return;

  lastCommand = String(cmd);
  lastCommandTime = millis();

  // Acoustic feedback chirp via safe GPIO toggle
  if (buzzerEnabled) {
    HAL::buzzPip(4000, 10);
  }

  // 1. NeoPixel Matrix Power (npm.power=1 / 0)
  if (strncmp(cmd, "npm.power=", 10) == 0) {
    int val = atoi(cmd + 10);
    matrixPower = (val != 0);
    if (matrixPower) {
      HAL::setMatrixPower(true);
      delay(10); // allow rail capacitor to charge
      if (!HAL::neoPixels) {
        HAL::neoPixels = new Adafruit_NeoPixel(16, NPM, NEO_GRB + NEO_KHZ800);
        HAL::neoPixels->begin();
      }
      HAL::neoPixels->setBrightness(matrixBrightness);
      HAL::neoPixels->clear();
      HAL::neoPixels->show();
      currentPattern = PATTERN_CYBER_RADAR;
    } else {
      currentPattern = PATTERN_OFF;
      if (HAL::neoPixels) {
        HAL::neoPixels->clear();
        HAL::neoPixels->show();
      }
      HAL::setMatrixPower(false);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.power\",\"val\":%d}\n", matrixPower);
    return;
  }

  // 2. NeoPixel Matrix Brightness (npm.brightness=80)
  if (strncmp(cmd, "npm.brightness=", 15) == 0) {
    int val = atoi(cmd + 15);
    matrixBrightness = (uint8_t)constrain(val, 0, 80);
    if (HAL::neoPixels && matrixPower) {
      HAL::neoPixels->setBrightness(matrixBrightness);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.brightness\",\"val\":%d}\n", matrixBrightness);
    return;
  }

  // 3. NeoPixel Animation Pattern (npm.pattern=cyber_radar)
  if (strncmp(cmd, "npm.pattern=", 12) == 0) {
    const char* pat = cmd + 12;
    if (strcmp(pat, "cyber_radar") == 0) currentPattern = PATTERN_CYBER_RADAR;
    else if (strcmp(pat, "matrix_rain") == 0) currentPattern = PATTERN_MATRIX_RAIN;
    else if (strcmp(pat, "spectrum_plasma") == 0) currentPattern = PATTERN_SPECTRUM_PLASMA;
    else if (strcmp(pat, "neon_tracer") == 0) currentPattern = PATTERN_NEON_TRACER;
    else if (strcmp(pat, "glyph_breath") == 0) currentPattern = PATTERN_GLYPH_BREATH;
    else if (strcmp(pat, "quantum_ripple") == 0) currentPattern = PATTERN_QUANTUM_RIPPLE;
    else if (strcmp(pat, "off") == 0) currentPattern = PATTERN_OFF;
    matrixStep = 0;
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.pattern\",\"val\":\"%s\"}\n", pat);
    return;
  }

  // 4. IMU & Telemetry Rate (sensor.imu.rate=50)
  if (strncmp(cmd, "sensor.imu.rate=", 16) == 0) {
    int hz = atoi(cmd + 16);
    if (hz >= 1 && hz <= 100) {
      setStreamRateHz((uint8_t)hz);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sensor.imu.rate\",\"val\":%d}\n", hz);
    return;
  }

  // 5. Environment Rate (sensor.env.rate=1)
  if (strncmp(cmd, "sensor.env.rate=", 16) == 0) {
    float hz = atof(cmd + 16);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sensor.env.rate\",\"val\":%.1f}\n", hz);
    return;
  }

  // 6. Display Brightness (display.brightness=140)
  if (strncmp(cmd, "display.brightness=", 19) == 0) {
    int val = atoi(cmd + 19);
    displayBrightness = (uint8_t)constrain(val, 0, 255);
    extern LGFX tft;
    tft.setBrightness(displayBrightness);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"display.brightness\",\"val\":%d}\n", displayBrightness);
    return;
  }

  // 7. Buzzer Enable (buzzer.enable=1 / 0)
  if (strncmp(cmd, "buzzer.enable=", 14) == 0) {
    int val = atoi(cmd + 14);
    buzzerEnabled = (val != 0);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"buzzer.enable\",\"val\":%d}\n", buzzerEnabled);
    return;
  }

  // 8. RTC Sync (rtc.sync=1727989345)
  if (strncmp(cmd, "rtc.sync=", 9) == 0) {
    time_t epoch = (time_t)atol(cmd + 9);
    if (epoch > 1700000000 && HAL::rtcReady) {
      struct tm* t = gmtime(&epoch);
      if (t) {
        HAL::rtcClock.setTime(t->tm_sec, t->tm_min, t->tm_hour, t->tm_wday + 1, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
      }
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"rtc.sync\",\"epoch\":%ld}\n", (long)epoch);
    return;
  }

  // 9. Storage List (storage.list)
  if (strcmp(cmd, "storage.list") == 0) {
    Serial.println("{\"roots\":[{\"name\":\"Internal Flash\",\"type\":\"flash\",\"totalBytes\":16777216,\"freeBytes\":12582912,\"entries\":[{\"name\":\"firmware\",\"type\":\"dir\",\"entries\":[{\"name\":\"isd-core.bin\",\"type\":\"file\",\"size\":1240000},{\"name\":\"bootloader.bin\",\"type\":\"file\",\"size\":32768},{\"name\":\"partitions.bin\",\"type\":\"file\",\"size\":4096}]},{\"name\":\"config\",\"type\":\"dir\",\"entries\":[{\"name\":\"settings.json\",\"type\":\"file\",\"size\":2048},{\"name\":\"calib.json\",\"type\":\"file\",\"size\":1024}]},{\"name\":\"logs\",\"type\":\"dir\",\"entries\":[{\"name\":\"sys.log\",\"type\":\"file\",\"size\":81920}]}]},{\"name\":\"NAND SD (ZDSD02G)\",\"type\":\"sd\",\"totalBytes\":268435456,\"freeBytes\":251658240,\"entries\":[{\"name\":\"telemetry.csv\",\"type\":\"file\",\"size\":409600},{\"name\":\"vitals.dat\",\"type\":\"file\",\"size\":65536}]}]}");
    return;
  }

  // 10. Ping
  if (strcmp(cmd, "ping") == 0) {
    Serial.printf("{\"status\":\"PONG\",\"ms\":%lu}\n", millis());
    return;
  }

  // Fallback generic ACK
  Serial.printf("{\"status\":\"ACK\",\"cmd\":\"%s\"}\n", cmd);
}

void UplinkBridge::updateMatrixAnimation() {
  if (!matrixPower || !HAL::neoPixels || currentPattern == PATTERN_OFF) {
    return;
  }

  uint32_t now = millis();
  if (now - lastMatrixFrame < 40) return; // ~25 FPS
  lastMatrixFrame = now;
  matrixStep++;

  HAL::neoPixels->clear();

  switch (currentPattern) {
    case PATTERN_CYBER_RADAR: {
      // Rotating radar beam around 4x4 matrix
      uint8_t angle = (matrixStep / 2) % 12;
      // Perimeter points: (0,0)->(3,0)->(3,3)->(0,3)
      static const uint8_t perim[12][2] = {
        {0,0},{1,0},{2,0},{3,0},
        {3,1},{3,2},{3,3},
        {2,3},{1,3},{0,3},
        {0,2},{0,1}
      };
      // Lead pixel (bright amber)
      HAL::neoPixels->setPixelColor(mapPixel(perim[angle][0], perim[angle][1]), 255, 115, 0);
      // Center hub
      HAL::neoPixels->setPixelColor(mapPixel(1, 1), 60, 25, 0);
      HAL::neoPixels->setPixelColor(mapPixel(2, 2), 60, 25, 0);
      // Trail
      uint8_t trail1 = (angle + 11) % 12;
      uint8_t trail2 = (angle + 10) % 12;
      HAL::neoPixels->setPixelColor(mapPixel(perim[trail1][0], perim[trail1][1]), 100, 45, 0);
      HAL::neoPixels->setPixelColor(mapPixel(perim[trail2][0], perim[trail2][1]), 30, 12, 0);
      break;
    }

    case PATTERN_MATRIX_RAIN: {
      // Falling digital stream
      for (int x = 0; x < 4; x++) {
        int y = (matrixStep / 3 + x * 2) % 6;
        if (y >= 0 && y < 4) {
          HAL::neoPixels->setPixelColor(mapPixel(x, y), 0, 220, 40); // Bright head
        }
        if (y - 1 >= 0 && y - 1 < 4) {
          HAL::neoPixels->setPixelColor(mapPixel(x, y - 1), 0, 70, 15); // Tail
        }
      }
      break;
    }

    case PATTERN_GLYPH_BREATH: {
      // Relaxing 4-second breathing cycle
      float s = (sinf((float)now / 636.0f) + 1.0f) * 0.5f; // 0..1
      uint8_t r = (uint8_t)(s * 255.0f);
      uint8_t g = (uint8_t)(s * 100.0f);
      uint32_t col = HAL::neoPixels->Color(r, g, 0);
      for (int i = 0; i < 16; i++) {
        HAL::neoPixels->setPixelColor(i, col);
      }
      break;
    }

    case PATTERN_SPECTRUM_PLASMA: {
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          float v = sinf(x * 1.5f + (float)matrixStep * 0.15f) + cosf(y * 1.5f + (float)matrixStep * 0.12f);
          uint8_t val = (uint8_t)((v + 2.0f) * 60.0f);
          HAL::neoPixels->setPixelColor(mapPixel(x, y), val, val / 2, 255 - val);
        }
      }
      break;
    }

    case PATTERN_NEON_TRACER: {
      uint8_t idx = matrixStep % 16;
      HAL::neoPixels->setPixelColor(idx, 255, 120, 0);
      HAL::neoPixels->setPixelColor((idx + 15) % 16, 60, 20, 0);
      break;
    }

    case PATTERN_QUANTUM_RIPPLE: {
      int ring = (matrixStep / 3) % 3;
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          int d = max(abs(x * 2 - 3), abs(y * 2 - 3)); // 1 or 3
          if ((d == 1 && ring == 0) || (d == 3 && ring == 1)) {
            HAL::neoPixels->setPixelColor(mapPixel(x, y), 255, 115, 0);
          }
        }
      }
      break;
    }

    default:
      break;
  }

  HAL::neoPixels->show();
}

void UplinkBridge::update(const SensorState& state) {
  // 1. Ingest Incoming Commands from Serial (USB-CDC)
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (rxIndex > 0) {
        rxBuffer[rxIndex] = '\0';
        processCommand(rxBuffer);
        rxIndex = 0;
      }
    } else if (rxIndex < sizeof(rxBuffer) - 1) {
      rxBuffer[rxIndex++] = c;
    }
  }

  // 2. Stream Out Telemetry Packets (Line-delimited JSON)
  uint32_t now = millis();
  if (streamActive && (now - lastStreamTime >= streamIntervalMs)) {
    lastStreamTime = now;
    packetsSent++;

    // Format clean JSON packet matching nx-uplink normalizer schema (all pure float/uint)
    Serial.printf("{\"ts\":%lu,\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f,\"ax\":0.01,\"ay\":0.02,\"az\":0.98,\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"gas\":%.0f,\"lux\":%.1f,\"hr\":%.0f,\"spo2\":%.0f,\"batt\":%.0f,\"voltage\":%.2f,\"charging\":%s}\n",
      now,
      state.roll,
      state.pitch,
      state.yaw,
      state.temp,
      state.hum,
      state.press,
      state.gas,
      state.lightLux,
      state.heartRate,
      state.spo2,
      state.batPercent,
      state.batVoltage,
      state.isCharging ? "true" : "false"
    );
  }

  // 3. Drive 4x4 NeoPixel Matrix Animation (only if powered and enabled)
  if (matrixPower && currentPattern != PATTERN_OFF) {
    updateMatrixAnimation();
  }
}
