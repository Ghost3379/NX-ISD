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
  Adafruit_NeoPixel* pixels = nullptr;
  bool powered = false;

  void begin() {
    pinMode(PWR_NPM, OUTPUT);
    digitalWrite(PWR_NPM, HIGH); // Turn on NeoPixel power domain
    powered = true;

    if (pixels == nullptr) {
      pixels = new Adafruit_NeoPixel(9, NPM, NEO_GRB + NEO_KHZ800);
    }
    pixels->begin();
    pixels->show();
  }

  void setAll(uint8_t r, uint8_t g, uint8_t b) {
    if (!pixels || !powered) return;
    for (int i = 0; i < 9; i++) {
      pixels->setPixelColor(i, pixels->Color(r, g, b));
    }
    pixels->show();
  }

  void powerDown() {
    setAll(0, 0, 0);
    digitalWrite(PWR_NPM, LOW); // Cut off power domain (saves battery)
    powered = false;
  }

  void runColorCycle(uint8_t step) {
    if (!pixels || !powered) return;
    for (int i = 0; i < 9; i++) {
      uint8_t hue = (step + i * 28) & 0xFF;
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
      pixels->setPixelColor(i, pixels->Color(r >> 2, g >> 2, b >> 2)); // Dimmed to 25% brightness
    }
    pixels->show();
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
