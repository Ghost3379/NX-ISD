#include "UplinkBridge.h"
#include "DisplayConfig.h"
#include "../../nx-systems/NX-MSF.h"
#include "../../Watchface.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>

extern Watchface watchface;

#define BLE_SERVICE_UUID        "8e7f1a01-2b3c-4d5e-9f01-0123456789ab"
#define BLE_TELEMETRY_CHAR_UUID "8e7f1a02-2b3c-4d5e-9f01-0123456789ab"
#define BLE_COMMAND_CHAR_UUID   "8e7f1a03-2b3c-4d5e-9f01-0123456789ab"

bool UplinkBridge::streamActive = false; // Strictly DORMANT by default - only streams when activated in TOOLS or requested via USB
uint32_t UplinkBridge::streamIntervalMs = 200; // 5 Hz when active
uint32_t UplinkBridge::lastStreamTime = 0;
uint32_t UplinkBridge::packetsSent = 0;
String UplinkBridge::lastCommand = "NONE";
uint32_t UplinkBridge::lastCommandTime = 0;
bool UplinkBridge::buzzerEnabled = false;
uint8_t UplinkBridge::displayBrightness = 200;

bool UplinkBridge::bleActive = false;
bool UplinkBridge::bleConnected = false;
static BLEServer* pBleServer = nullptr;
static BLECharacteristic* pBleTelemetryChar = nullptr;
static BLECharacteristic* pBleCommandChar = nullptr;

class UplinkServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    UplinkBridge::setBleConnected(true);
    Serial.println("{\"status\":\"EVENT\",\"evt\":\"ble.connect\"}");
  }
  void onDisconnect(BLEServer* pServer) override {
    UplinkBridge::setBleConnected(false);
    Serial.println("{\"status\":\"EVENT\",\"evt\":\"ble.disconnect\"}");
    if (UplinkBridge::isBleActive()) {
      BLEDevice::startAdvertising();
    }
  }
};

class UplinkCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    std::string rxVal = pChar->getValue();
    if (!rxVal.empty()) {
      UplinkBridge::processCommand(rxVal.c_str());
    }
  }
};

bool UplinkBridge::matrixPower = false;
uint8_t UplinkBridge::matrixBrightness = 30;
MatrixPattern UplinkBridge::currentPattern = PATTERN_OFF;
uint32_t UplinkBridge::lastMatrixFrame = 0;
uint8_t UplinkBridge::matrixStep = 0;
uint32_t UplinkBridge::customPixels[16] = {0};

char UplinkBridge::rxBuffer[512] = {0};
uint16_t UplinkBridge::rxIndex = 0;

volatile bool UplinkBridge::virtualNavLeft = false;
volatile bool UplinkBridge::virtualNavRight = false;
volatile bool UplinkBridge::virtualNavPush = false;
volatile bool UplinkBridge::virtualNavBtn = false;

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
    FastLED.setBrightness(matrixBrightness);
    FastLED.clear(true);
    if (currentPattern == PATTERN_OFF) {
      currentPattern = PATTERN_CYBER_RADAR;
    }
  } else {
    FastLED.clear(true);
    HAL::setMatrixPower(false);
  }
}

void UplinkBridge::setMatrixBrightness(uint8_t b) {
  matrixBrightness = constrain(b, 0, 80);
  if (matrixPower) {
    FastLED.setBrightness(matrixBrightness);
  }
}

void UplinkBridge::setMatrixPattern(MatrixPattern pat) {
  currentPattern = pat;
  matrixStep = 0;
  if (!matrixPower) return;
  if (currentPattern == PATTERN_OFF) {
    FastLED.clear(true);
  }
}

bool UplinkBridge::consumeNavLeft() {
  if (virtualNavLeft) {
    virtualNavLeft = false;
    return true;
  }
  return false;
}

bool UplinkBridge::consumeNavRight() {
  if (virtualNavRight) {
    virtualNavRight = false;
    return true;
  }
  return false;
}

bool UplinkBridge::consumeNavPush() {
  if (virtualNavPush) {
    virtualNavPush = false;
    return true;
  }
  return false;
}

bool UplinkBridge::consumeNavBtn() {
  if (virtualNavBtn) {
    virtualNavBtn = false;
    return true;
  }
  return false;
}

void UplinkBridge::setCustomPixels(const uint32_t pixels[16]) {
  for (int i = 0; i < 16; i++) {
    customPixels[i] = pixels[i];
  }
  currentPattern = PATTERN_CUSTOM;
  if (!matrixPower) {
    setMatrixPower(true);
  }
  if (matrixPower) {
    for (uint8_t i = 0; i < 16; i++) {
      uint8_t x = i % 4;
      uint8_t y = i / 4;
      uint8_t r = (customPixels[i] >> 16) & 0xFF;
      uint8_t g = (customPixels[i] >> 8) & 0xFF;
      uint8_t b = customPixels[i] & 0xFF;
      HAL::leds[mapPixel(x, y)] = CRGB(r, g, b);
    }
    FastLED.show();
  }
}

void UplinkBridge::setBleActive(bool active) {
  if (active == bleActive) {
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"ble.state\",\"bleActive\":%s,\"bleConnected\":%s}\n",
      bleActive ? "true" : "false",
      bleConnected ? "true" : "false"
    );
    return;
  }
  bleActive = active;

  if (bleActive) {
    BLEDevice::init("NX-ISD-UPLINK");
    BLEDevice::setMTU(512);
    pBleServer = BLEDevice::createServer();
    pBleServer->setCallbacks(new UplinkServerCallbacks());

    BLEService* pService = pBleServer->createService(BLE_SERVICE_UUID);

    pBleTelemetryChar = pService->createCharacteristic(
      BLE_TELEMETRY_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY
    );
    pBleTelemetryChar->addDescriptor(new BLE2902());

    pBleCommandChar = pService->createCharacteristic(
      BLE_COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pBleCommandChar->setCallbacks(new UplinkCommandCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    BLEDevice::startAdvertising();

    Serial.println("{\"status\":\"ACK\",\"cmd\":\"ble.start\",\"bleActive\":true}");
  } else {
    bleConnected = false;
    if (pBleServer) {
      BLEDevice::deinit(true);
      pBleServer = nullptr;
      pBleTelemetryChar = nullptr;
      pBleCommandChar = nullptr;
    }
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"ble.stop\",\"bleActive\":false}");
  }
}

void UplinkBridge::pollSerial() {
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
}

void UplinkBridge::processCommand(const char* cmd) {
  if (!cmd || strlen(cmd) == 0) return;

  lastCommand = String(cmd);
  lastCommandTime = millis();

  // Acoustic feedback chirp via safe GPIO toggle
  if (buzzerEnabled) {
    HAL::buzzPip(4000, 10);
  }

  // 1. Virtual Navigation Controls (companion dashboard remote clicks)
  if (strcmp(cmd, "nav.left") == 0) {
    virtualNavLeft = true;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"nav.left\"}");
    return;
  }
  if (strcmp(cmd, "nav.right") == 0) {
    virtualNavRight = true;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"nav.right\"}");
    return;
  }
  if (strcmp(cmd, "nav.push") == 0) {
    virtualNavPush = true;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"nav.push\"}");
    return;
  }
  if (strcmp(cmd, "nav.btn") == 0) {
    virtualNavBtn = true;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"nav.btn\"}");
    return;
  }

  // 2. Stream Control (stream.start, stream.stop, stream.rate)
  if (strcmp(cmd, "stream.start") == 0 || strcmp(cmd, "stream.on") == 0) {
    streamActive = true;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"stream.start\",\"active\":true}");
    return;
  }
  if (strcmp(cmd, "stream.stop") == 0 || strcmp(cmd, "stream.off") == 0) {
    streamActive = false;
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"stream.stop\",\"active\":false}");
    return;
  }
  if (strncmp(cmd, "stream.rate=", 12) == 0) {
    int hz = atoi(cmd + 12);
    setStreamRateHz((uint8_t)constrain(hz, 1, 100));
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"stream.rate\",\"val\":%d}\n", hz);
    return;
  }

  // 3. Custom NeoPixel Matrix Frame (JSON {"npm.matrix":["#hex",...]} or {"led.matrix":[...]})
  if (strstr(cmd, "npm.matrix") != nullptr || strstr(cmd, "led.matrix") != nullptr) {
    uint8_t count = 0;
    const char* p = cmd;
    while (*p && count < 16) {
      if (*p == '#') {
        p++;
        char hexBuf[7] = {0};
        for (int i = 0; i < 6 && *p; i++, p++) {
          hexBuf[i] = *p;
        }
        customPixels[count++] = (uint32_t)strtoul(hexBuf, nullptr, 16);
      } else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        char hexBuf[7] = {0};
        for (int i = 0; i < 6 && *p; i++, p++) {
          hexBuf[i] = *p;
        }
        customPixels[count++] = (uint32_t)strtoul(hexBuf, nullptr, 16);
      } else {
        p++;
      }
    }

    if (count > 0) {
      currentPattern = PATTERN_CUSTOM;
      if (!matrixPower) {
        setMatrixPower(true);
      }
      if (matrixPower) {
        for (uint8_t i = 0; i < count; i++) {
          uint8_t x = i % 4;
          uint8_t y = i / 4;
          uint8_t r = (customPixels[i] >> 16) & 0xFF;
          uint8_t g = (customPixels[i] >> 8) & 0xFF;
          uint8_t b = customPixels[i] & 0xFF;
          HAL::leds[mapPixel(x, y)] = CRGB(r, g, b);
        }
        FastLED.show();
      }
      Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.matrix\",\"count\":%d}\n", count);
      return;
    }
  }

  // 4. NeoPixel Matrix Power (npm.power=1 / 0)
  if (strncmp(cmd, "npm.power=", 10) == 0) {
    int val = atoi(cmd + 10);
    matrixPower = (val != 0);
    if (matrixPower) {
      HAL::setMatrixPower(true);
      delay(10); // allow rail capacitor to charge
      FastLED.setBrightness(matrixBrightness);
      FastLED.clear(true);
      currentPattern = PATTERN_CYBER_RADAR;
    } else {
      currentPattern = PATTERN_OFF;
      FastLED.clear(true);
      HAL::setMatrixPower(false);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.power\",\"val\":%d}\n", matrixPower);
    return;
  }

  // 5. NeoPixel Matrix Brightness (npm.brightness=80)
  if (strncmp(cmd, "npm.brightness=", 15) == 0) {
    int val = atoi(cmd + 15);
    matrixBrightness = (uint8_t)constrain(val, 0, 80);
    if (matrixPower) {
      FastLED.setBrightness(matrixBrightness);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"npm.brightness\",\"val\":%d}\n", matrixBrightness);
    return;
  }

  // 6. NeoPixel Animation Pattern (npm.pattern=cyber_radar)
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

  // 7. IMU & Telemetry Rate (sensor.imu.rate=50)
  if (strncmp(cmd, "sensor.imu.rate=", 16) == 0) {
    int hz = atoi(cmd + 16);
    if (hz >= 1 && hz <= 100) {
      setStreamRateHz((uint8_t)hz);
    }
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sensor.imu.rate\",\"val\":%d}\n", hz);
    return;
  }

  // 8. Environment Rate (sensor.env.rate=1)
  if (strncmp(cmd, "sensor.env.rate=", 16) == 0) {
    float hz = atof(cmd + 16);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sensor.env.rate\",\"val\":%.1f}\n", hz);
    return;
  }

  // 9. Display Brightness (display.brightness=140)
  if (strncmp(cmd, "display.brightness=", 19) == 0) {
    int val = atoi(cmd + 19);
    displayBrightness = (uint8_t)constrain(val, 0, 255);
    extern LGFX tft;
    tft.setBrightness(displayBrightness);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"display.brightness\",\"val\":%d}\n", displayBrightness);
    return;
  }

  // 10. Buzzer Enable (buzzer.enable=1 / 0)
  if (strncmp(cmd, "buzzer.enable=", 14) == 0) {
    int val = atoi(cmd + 14);
    buzzerEnabled = (val != 0);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"buzzer.enable\",\"val\":%d}\n", buzzerEnabled);
    return;
  }

  // 11. RTC Sync (rtc.sync=1727989345)
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

  // 12. Storage List (storage.list)
  if (strcmp(cmd, "storage.list") == 0) {
    Serial.println("{\"roots\":[{\"name\":\"Internal Flash\",\"type\":\"flash\",\"totalBytes\":16777216,\"freeBytes\":12582912,\"entries\":[{\"name\":\"firmware\",\"type\":\"dir\",\"entries\":[{\"name\":\"isd-core.bin\",\"type\":\"file\",\"size\":1240000},{\"name\":\"bootloader.bin\",\"type\":\"file\",\"size\":32768},{\"name\":\"partitions.bin\",\"type\":\"file\",\"size\":4096}]},{\"name\":\"config\",\"type\":\"dir\",\"entries\":[{\"name\":\"settings.json\",\"type\":\"file\",\"size\":2048},{\"name\":\"calib.json\",\"type\":\"file\",\"size\":1024}]},{\"name\":\"logs\",\"type\":\"dir\",\"entries\":[{\"name\":\"sys.log\",\"type\":\"file\",\"size\":81920}]}]},{\"name\":\"NAND SD (ZDSD02G)\",\"type\":\"sd\",\"totalBytes\":268435456,\"freeBytes\":251658240,\"entries\":[{\"name\":\"telemetry.csv\",\"type\":\"file\",\"size\":409600},{\"name\":\"vitals.dat\",\"type\":\"file\",\"size\":65536}]}]}");
    return;
  }

  // 13. Ping / System Ping
  if (strcmp(cmd, "ping") == 0 || strcmp(cmd, "sys.ping=1") == 0 || strcmp(cmd, "sys.ping") == 0) {
    Serial.printf("{\"status\":\"PONG\",\"ms\":%lu}\n", millis());
    return;
  }

  // 14. BLE Uplink Control (ble.start, ble.stop, ble.status)
  if (strcmp(cmd, "ble.start") == 0 || strcmp(cmd, "ble.on") == 0 || strcmp(cmd, "ble.enable=1") == 0) {
    setBleActive(true);
    return;
  }
  if (strcmp(cmd, "ble.stop") == 0 || strcmp(cmd, "ble.off") == 0 || strcmp(cmd, "ble.enable=0") == 0) {
    setBleActive(false);
    return;
  }
  if (strcmp(cmd, "ble.status") == 0) {
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"ble.status\",\"bleActive\":%s,\"bleConnected\":%s}\n",
      bleActive ? "true" : "false",
      bleConnected ? "true" : "false"
    );
    return;
  }

  // 15. System Information & Health Diagnostics (CLI Inspection)
  if (strcmp(cmd, "sys.info") == 0) {
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sys.info\",\"chip\":\"ESP32-S3-WROOM-1\",\"cpuMHz\":%u,\"freeHeap\":%u,\"freePsram\":%u,\"uptimeMs\":%lu,\"stream\":%s,\"ble\":%s,\"screenStandby\":%s}\n",
      ESP.getCpuFreqMHz(),
      ESP.getFreeHeap(),
      ESP.getFreePsram(),
      millis(),
      streamActive ? "true" : "false",
      bleActive ? "true" : "false",
      watchface.isInStandby() ? "true" : "false"
    );
    return;
  }

  // 16. Single-Shot Sensor Telemetry Query
  if (strcmp(cmd, "sys.sensors") == 0) {
    SensorState s;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      s = sharedState;
      xSemaphoreGive(stateMutex);
    }
    static float baseGas = 150.0f;
    NX_MSF::AirQualityAnalysis a = NX_MSF::calculateAirQuality(s.gas, s.temp, s.hum, baseGas);
    Serial.printf("{\"status\":\"ACK\",\"cmd\":\"sys.sensors\",\"ts\":%lu,\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f,\"heading\":%.1f,\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"gas\":%.0f,\"iaq\":%d,\"lux\":%.1f,\"hr\":%.0f,\"spo2\":%.0f,\"batt\":%.0f,\"voltage\":%.2f,\"charging\":%s}\n",
      millis(), s.roll, s.pitch, s.yaw, s.heading, s.temp, s.hum, s.press, s.gas, a.iaqScore, s.lightLux, s.heartRate, s.spo2, s.batPercent, s.batVoltage, s.isCharging ? "true" : "false"
    );
    return;
  }

  // 17. System Reboot
  if (strcmp(cmd, "sys.reboot") == 0 || strcmp(cmd, "reboot") == 0) {
    Serial.println("{\"status\":\"ACK\",\"cmd\":\"sys.reboot\"}");
    delay(100);
    ESP.restart();
    return;
  }

  // Fallback generic ACK
  Serial.printf("{\"status\":\"ACK\",\"cmd\":\"%s\"}\n", cmd);
}

void UplinkBridge::updateMatrixAnimation() {
  if (!matrixPower || currentPattern == PATTERN_OFF || currentPattern == PATTERN_CUSTOM) {
    return;
  }

  uint32_t now = millis();
  if (now - lastMatrixFrame < 40) return; // ~25 FPS
  lastMatrixFrame = now;
  matrixStep++;

  FastLED.clear();

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
      HAL::leds[mapPixel(perim[angle][0], perim[angle][1])] = CRGB(255, 115, 0);
      // Center hub
      HAL::leds[mapPixel(1, 1)] = CRGB(60, 25, 0);
      HAL::leds[mapPixel(2, 2)] = CRGB(60, 25, 0);
      // Trail
      uint8_t trail1 = (angle + 11) % 12;
      uint8_t trail2 = (angle + 10) % 12;
      HAL::leds[mapPixel(perim[trail1][0], perim[trail1][1])] = CRGB(100, 45, 0);
      HAL::leds[mapPixel(perim[trail2][0], perim[trail2][1])] = CRGB(30, 12, 0);
      break;
    }

    case PATTERN_MATRIX_RAIN: {
      // Falling digital stream
      for (int x = 0; x < 4; x++) {
        int y = (matrixStep / 3 + x * 2) % 6;
        if (y >= 0 && y < 4) {
          HAL::leds[mapPixel(x, y)] = CRGB(0, 220, 40); // Bright head
        }
        if (y - 1 >= 0 && y - 1 < 4) {
          HAL::leds[mapPixel(x, y - 1)] = CRGB(0, 70, 15); // Tail
        }
      }
      break;
    }

    case PATTERN_GLYPH_BREATH: {
      // Relaxing breathing cycle with FastLED 8-bit sine
      uint8_t breath = beatsin8(15, 20, 255); // 15 BPM, range 20..255
      CRGB col = CRGB(breath, (breath * 100) / 255, 0);
      for (int i = 0; i < 16; i++) {
        HAL::leds[i] = col;
      }
      break;
    }

    case PATTERN_SPECTRUM_PLASMA: {
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          // FastLED 8-bit sine math - super smooth and fast
          uint8_t v = sin8(x * 64 + matrixStep * 8) / 2 + cos8(y * 64 + matrixStep * 6) / 2;
          HAL::leds[mapPixel(x, y)] = CHSV(v, 240, 200);
        }
      }
      break;
    }

    case PATTERN_NEON_TRACER: {
      uint8_t idx = matrixStep % 16;
      HAL::leds[idx] = CRGB(255, 120, 0);
      HAL::leds[(idx + 15) % 16] = CRGB(60, 20, 0);
      break;
    }

    case PATTERN_QUANTUM_RIPPLE: {
      int ring = (matrixStep / 3) % 3;
      for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
          int d = max(abs(x * 2 - 3), abs(y * 2 - 3)); // 1 or 3
          if ((d == 1 && ring == 0) || (d == 3 && ring == 1)) {
            HAL::leds[mapPixel(x, y)] = CRGB(255, 115, 0);
          }
        }
      }
      break;
    }

    default:
      break;
  }

  FastLED.show();
}

void UplinkBridge::update(const SensorState& state) {
  // 1. Ingest Incoming Commands from Serial (USB-CDC)
  pollSerial();

  // 2. Stream Out Telemetry Packets (Line-delimited JSON)
  uint32_t now = millis();
  if (streamActive && (now - lastStreamTime >= streamIntervalMs)) {
    lastStreamTime = now;
    packetsSent++;

    static float gasBaselineKohm = 150.0f;
    NX_MSF::AirQualityAnalysis air = NX_MSF::calculateAirQuality(state.gas, state.temp, state.hum, gasBaselineKohm);

    float radRoll = state.roll * 0.0174532925f;
    float radPitch = state.pitch * 0.0174532925f;
    float calcAx = sinf(radPitch);
    float calcAy = -sinf(radRoll);
    float calcAz = cosf(radPitch) * cosf(radRoll);

    // Format clean JSON packet matching nx-uplink normalizer schema (all pure float/uint)
    char telemetryBuf[280];
    snprintf(telemetryBuf, sizeof(telemetryBuf),
      "{\"ts\":%lu,\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":%.2f,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"gas\":%.0f,\"iaq\":%d,\"lux\":%.1f,\"hr\":%.0f,\"spo2\":%.0f,\"batt\":%.0f,\"voltage\":%.2f,\"charging\":%s}",
      now,
      state.roll,
      state.pitch,
      state.yaw,
      calcAx,
      calcAy,
      calcAz,
      state.temp,
      state.hum,
      state.press,
      state.gas,
      air.iaqScore,
      state.lightLux,
      state.heartRate,
      state.spo2,
      state.batPercent,
      state.batVoltage,
      state.isCharging ? "true" : "false"
    );

    // USB-CDC Serial Stream
    if (streamActive) {
      Serial.println(telemetryBuf);
    }

    // Wireless BLE Notification
    if (bleActive && bleConnected && pBleTelemetryChar) {
      pBleTelemetryChar->setValue((uint8_t*)telemetryBuf, strlen(telemetryBuf));
      pBleTelemetryChar->notify();
    }
  }

  // 3. Drive 4x4 NeoPixel Matrix Animation (only if powered and enabled)
  if (matrixPower && currentPattern != PATTERN_OFF && currentPattern != PATTERN_CUSTOM) {
    updateMatrixAnimation();
  }
}
