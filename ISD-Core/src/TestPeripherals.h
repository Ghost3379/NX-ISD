#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <RV-3028-C7.h>
#include "pins.h"

class RTCTest {
public:
  RV3028 rtc;
  bool initialized = false;

  bool begin() {
    initialized = rtc.begin();
    if (initialized) {
      rtc.set24Hour();
    }
    return initialized;
  }

  void setDummyTime() {
    if (!initialized) return;
    // Set time to compiling time if it's currently invalid (e.g. 2000-01-01)
    rtc.setToCompilerTime();
  }

  void getTimeString(char* buf, size_t len) {
    if (!initialized) {
      snprintf(buf, len, "RTC: FAIL");
      return;
    }
    
    if (rtc.updateTime() == false) {
      snprintf(buf, len, "RTC Read Err");
      return;
    }

    snprintf(buf, len, "%02d:%02d:%02d", 
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
  }

  void getDateString(char* buf, size_t len) {
    if (!initialized) {
      snprintf(buf, len, "RTC: FAIL");
      return;
    }
    rtc.updateTime();
    snprintf(buf, len, "%02d/%02d/%04d", 
             rtc.getDate(), rtc.getMonth(), rtc.getYear());
  }
};

class SDCardTest {
public:
  bool initialized = false;

  bool begin() {
    // NAND-SD Chip Select is CS_SD (47)
    initialized = SD.begin(CS_SD);
    return initialized;
  }

  bool testReadWrite() {
    if (!initialized) return false;

    File file = SD.open("/test.txt", FILE_WRITE);
    if (!file) return false;

    file.println("NX-ISD TEST OK");
    file.close();

    file = SD.open("/test.txt", FILE_READ);
    if (!file) return false;

    String content = "";
    while (file.available()) {
      content += (char)file.read();
    }
    file.close();

    SD.remove("/test.txt");
    return content.indexOf("NX-ISD TEST OK") >= 0;
  }
};

class NeoPixelTest {
public:
  static const uint16_t NUM_PIXELS = 16;
  static const uint8_t NUM_MODES = 6;

  Adafruit_NeoPixel* pixels = nullptr;
  bool powered = false;
  uint8_t currentMode = 0;
  uint16_t animTick = 0;

  // Serpentine coordinate helper for physical 4x4 PCB routing:
  // Row 0 (Y=0): G1  (0)  -> G2  (1)  -> G3  (2)  -> G4  (3)   [L -> R]
  // Row 1 (Y=1): G8  (7)  <- G7  (6)  <- G6  (5)  <- G5  (4)   [R -> L]
  // Row 2 (Y=2): G9  (8)  -> G10 (9)  -> G11 (10) -> G12 (11)  [L -> R]
  // Row 3 (Y=3): G16 (15) <- G15 (14) <- G14 (13) <- G13 (12)  [R -> L]
  static inline uint16_t xy(uint8_t x, uint8_t y) {
    if (x > 3 || y > 3) return 0;
    return (y % 2 == 0) ? (y * 4 + x) : (y * 4 + (3 - x));
  }

  void begin() {
    pinMode(PWR_NPM, OUTPUT);
    digitalWrite(PWR_NPM, HIGH); // Turn on NeoPixel power domain (Q3 pulls PMOS gate LOW)
    powered = true;

    if (pixels == nullptr) {
      pixels = new Adafruit_NeoPixel(NUM_PIXELS, NPM, NEO_GRB + NEO_KHZ800);
    }
    pixels->begin();
    pixels->setBrightness(60); // Clean, comfortable diagnostic brightness
    pixels->clear();
    pixels->show();
  }

  void setAll(uint8_t r, uint8_t g, uint8_t b) {
    if (!pixels || !powered) return;
    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
      pixels->setPixelColor(i, pixels->Color(r, g, b));
    }
    pixels->show();
  }

  void setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (!pixels || !powered) return;
    pixels->setPixelColor(xy(x, y), pixels->Color(r, g, b));
  }

  void setPixelXY(uint8_t x, uint8_t y, uint32_t color) {
    if (!pixels || !powered) return;
    pixels->setPixelColor(xy(x, y), color);
  }

  void powerDown() {
    if (pixels && powered) {
      pixels->clear();
      pixels->show();
    }
    digitalWrite(PWR_NPM, LOW); // Cut off power domain via PMOS switch (0uA leakage)
    powered = false;
  }

  const char* getAnimName() const {
    switch (currentMode) {
      case 0: return "CYBER RADAR";
      case 1: return "GLYPH BREATH";
      case 2: return "QUANTUM RIPPLE";
      case 3: return "NEON TRACER";
      case 4: return "MATRIX RAIN";
      case 5: return "SPECTRUM PLASMA";
      default: return "CYBER RADAR";
    }
  }

  void nextAnimation() {
    currentMode = (currentMode + 1) % NUM_MODES;
    animTick = 0;
    if (pixels && powered) {
      pixels->clear();
      pixels->show();
    }
  }

  void prevAnimation() {
    currentMode = (currentMode - 1 + NUM_MODES) % NUM_MODES;
    animTick = 0;
    if (pixels && powered) {
      pixels->clear();
      pixels->show();
    }
  }

  // Update active animation frame
  void updateAnimation() {
    if (!pixels || !powered) return;
    animTick++;

    switch (currentMode) {
      case 0: runRadar(animTick); break;
      case 1: runGlyphBreath(animTick); break;
      case 2: runQuantumRipple(animTick); break;
      case 3: runNeonTracer(animTick); break;
      case 4: runMatrixRain(animTick); break;
      case 5: runSpectrumPlasma(animTick); break;
    }
    pixels->show();
  }

  void runColorCycle(uint8_t step) {
    if (!pixels || !powered) return;
    for (uint16_t i = 0; i < NUM_PIXELS; i++) {
      uint8_t hue = (step + i * 16) & 0xFF;
      uint8_t r, g, b;
      if (hue < 85) {
        r = hue * 3; g = 255 - hue * 3; b = 0;
      } else if (hue < 170) {
        hue -= 85;
        r = 255 - hue * 3; g = 0; b = hue * 3;
      } else {
        hue -= 170;
        r = 0; g = hue * 3; b = 255 - hue * 3;
      }
      pixels->setPixelColor(i, pixels->Color(r >> 2, g >> 2, b >> 2));
    }
    pixels->show();
  }

private:
  // 0. CYBER RADAR: 360-degree rotating HUD beam in electric cyan with phosphor decay
  void runRadar(uint16_t tick) {
    float angle = (tick % 72) * (2.0f * PI / 72.0f);
    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        float px = (x - 1.5f);
        float py = (y - 1.5f);
        float pAngle = atan2f(py, px);
        float diff = angle - pAngle;
        while (diff < -PI) diff += 2.0f * PI;
        while (diff > PI) diff -= 2.0f * PI;

        if (diff >= -0.35f && diff <= 0.15f) {
          setPixelXY(x, y, 0, 240, 255); // Sharp neon cyan beam
        } else if (diff > 0.15f && diff < 1.8f) {
          float fade = 1.0f - (diff / 1.8f);
          setPixelXY(x, y, 0, (uint8_t)(fade * 80.0f), (uint8_t)(fade * 220.0f));
        } else {
          setPixelXY(x, y, 0, 8, 20); // Ambient floor
        }
      }
    }
  }

  // 1. GLYPH BREATH: Nothing-Phone aesthetic cold-white & amber geometric pulse
  void runGlyphBreath(uint16_t tick) {
    float phase = (tick % 60) * (2.0f * PI / 60.0f);
    float outerIntensity = (sinf(phase) + 1.0f) * 0.5f;
    float innerIntensity = (sinf(phase + PI) + 1.0f) * 0.5f;

    uint8_t outW = (uint8_t)(outerIntensity * 230.0f);
    uint8_t inW = (uint8_t)(innerIntensity * 255.0f);

    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        bool isCore = (x >= 1 && x <= 2 && y >= 1 && y <= 2);
        if (isCore) {
          setPixelXY(x, y, inW, (uint8_t)(inW * 0.65f), (uint8_t)(inW * 0.10f)); // Warm amber
        } else {
          setPixelXY(x, y, outW, outW, (uint8_t)(outW * 1.1f > 255 ? 255 : outW * 1.1f)); // Cold white
        }
      }
    }
  }

  // 2. QUANTUM RIPPLE: Center-outward concentric droplet wave in electric violet / aqua
  void runQuantumRipple(uint16_t tick) {
    float wavePos = fmodf((float)tick * 0.12f, 3.5f);
    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        float dx = x - 1.5f;
        float dy = y - 1.5f;
        float dist = sqrtf(dx * dx + dy * dy);
        float delta = fabsf(dist - wavePos);

        if (delta < 0.65f) {
          float bright = 1.0f - (delta / 0.65f);
          setPixelXY(x, y, (uint8_t)(bright * 240.0f), (uint8_t)(bright * 40.0f), (uint8_t)(bright * 255.0f));
        } else {
          setPixelXY(x, y, 5, 10, 25);
        }
      }
    }
  }

  // 3. NEON TRACER: High-speed orbital tracer around border with pulsing core
  void runNeonTracer(uint16_t tick) {
    static const uint8_t borderX[12] = {0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
    static const uint8_t borderY[12] = {0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1};

    uint8_t head = (tick / 2) % 12;

    for (uint8_t i = 0; i < 12; i++) {
      setPixelXY(borderX[i], borderY[i], 0, 0, 0);
    }

    for (uint8_t tail = 0; tail < 4; tail++) {
      int idx = (head - tail + 12) % 12;
      uint8_t x = borderX[idx];
      uint8_t y = borderY[idx];
      if (tail == 0) setPixelXY(x, y, 255, 255, 255);
      else if (tail == 1) setPixelXY(x, y, 255, 120, 0);
      else if (tail == 2) setPixelXY(x, y, 220, 20, 0);
      else setPixelXY(x, y, 60, 5, 0);
    }

    float pulse = (sinf(tick * 0.15f) + 1.0f) * 0.5f;
    uint8_t coreG = (uint8_t)(pulse * 180.0f);
    uint8_t coreB = (uint8_t)(pulse * 255.0f);
    setPixelXY(1, 1, 0, coreG, coreB);
    setPixelXY(2, 1, 0, coreG, coreB);
    setPixelXY(1, 2, 0, coreG, coreB);
    setPixelXY(2, 2, 0, coreG, coreB);
  }

  // 4. MATRIX RAIN: Cyber digital green rain dropping down columns
  void runMatrixRain(uint16_t tick) {
    for (uint8_t x = 0; x < 4; x++) {
      uint8_t colOffset = (x * 7) + (tick / 3);
      int dropY = colOffset % 6;

      for (uint8_t y = 0; y < 4; y++) {
        if (y == dropY) {
          setPixelXY(x, y, 160, 255, 160); // Droplet head
        } else if (y == dropY - 1) {
          setPixelXY(x, y, 0, 220, 30);    // Green trail
        } else if (y == dropY - 2) {
          setPixelXY(x, y, 0, 60, 10);     // Fade
        } else {
          setPixelXY(x, y, 0, 0, 0);
        }
      }
    }
  }

  // 5. SPECTRUM PLASMA: Continuous 2D undulating RGB fluid plasma wave
  void runSpectrumPlasma(uint16_t tick) {
    float t = (float)tick * 0.08f;
    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        float v1 = sinf(x * 1.5f + t);
        float v2 = sinf(y * 1.5f + t * 0.7f);
        float v3 = sinf((x + y) * 1.2f + t * 1.2f);
        float val = (v1 + v2 + v3 + 3.0f) / 6.0f;

        uint8_t hue = (uint8_t)(val * 255.0f);
        uint8_t r, g, b;
        if (hue < 85) {
          r = hue * 3; g = 255 - hue * 3; b = 0;
        } else if (hue < 170) {
          hue -= 85;
          r = 255 - hue * 3; g = 0; b = hue * 3;
        } else {
          hue -= 170;
          r = 0; g = hue * 3; b = 255 - hue * 3;
        }
        setPixelXY(x, y, r >> 2, g >> 2, b >> 2);
      }
    }
  }
};

class BuzzerTest {
public:
  void begin() {
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
  }

  void playBeep(uint16_t freq, uint16_t duration) {
    tone(BUZZER, freq, duration);
    delay(duration + 10); // Wait for the note to finish
  }

  void playClick() {
    tone(BUZZER, 2500, 8); // Very quick high-pitch tick for buttons
  }

  void playStartupMelody() {
    playBeep(880, 80);  // A5
    playBeep(1109, 80); // C#6
    playBeep(1318, 120); // E6
  }

  void playFailureBeep() {
    playBeep(300, 150);
    playBeep(200, 200);
  }

  void playSuccessBeep() {
    playBeep(1500, 80);
    playBeep(2000, 100);
  }
};
