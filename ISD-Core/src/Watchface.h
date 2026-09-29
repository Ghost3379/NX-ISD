#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"

class Watchface {
private:
  LGFX_Sprite canvas;
  bool initialized = false;

  // TVA / Flipper Zero Amber-Orange Retro-Modern Palette
  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

public:
  Watchface(LGFX* tft) : canvas(tft) {}

  void begin() {
    canvas.setColorDepth(16); // 16-bit RGB565
    canvas.setPsram(true);    // Allocate in 8MB PSRAM for smooth double-buffering
    canvas.createSprite(240, 240);

    // Initialize Palette
    COLOR_BG            = canvas.color565(0, 0, 0);          // Pitch Black
    COLOR_ORANGE_BRIGHT = canvas.color565(255, 115, 0);      // High-vis vibrant amber
    COLOR_ORANGE_MID    = canvas.color565(190, 75, 0);       // Mid-tone orange
    COLOR_ORANGE_DIM    = canvas.color565(75, 28, 0);        // Dim hairline borders
    COLOR_ORANGE_DARK   = canvas.color565(35, 12, 0);        // Dark grid background

    initialized = true;
  }

  void render(const SensorState& state) {
    if (!initialized) return;

    // 1. Clear offscreen buffer to Pure Black
    canvas.fillScreen(COLOR_BG);

    // 2. Outer Technical Border Framing
    canvas.drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas.drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);
    canvas.drawFastHLine(4, 168, 232, COLOR_ORANGE_DIM);

    // ==================== HEADER (Y: 6 to 26) ====================
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawString("NX-ISD // v1p3", 10, 10);

    // Battery Readout
    char batBuf[16];
    if (state.batPercent > 0.0f && state.batPercent <= 100.0f) {
      if (state.usbConnected) {
        snprintf(batBuf, sizeof(batBuf), "%d%% [CHG]", (int)state.batPercent);
      } else {
        snprintf(batBuf, sizeof(batBuf), "%d%%", (int)state.batPercent);
      }
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(batBuf, sizeof(batBuf), "--%%");
      canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas.drawRightString(batBuf, 230, 10);

    // ==================== CLOCK SECTION (Y: 34 to 86) ====================
    // Real time from RV-3028 RTC, or "--:--:--" if not yet set
    const char* timeStr = (state.rtcTime[0] != '\0' && state.rtcTime[0] != '-') ? state.rtcTime : "--:--:--";

    canvas.setTextSize(3);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas.drawCenterString(timeStr, 120, 38);

    // Date
    const char* dateStr = (state.rtcDate[0] != '\0' && state.rtcDate[0] != '-') ? state.rtcDate : "----/--/--";
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawCenterString(dateStr, 120, 70);

    // ==================== TACTICAL ATTITUDE / HORIZON RETICLE (Y: 92 to 162) ====================
    int centerX = 120;
    int centerY = 126;
    int radius  = 24;

    // Reticle circular bounds
    canvas.drawCircle(centerX, centerY, radius, COLOR_ORANGE_DIM);
    canvas.drawCircle(centerX, centerY, 3, COLOR_ORANGE_MID);

    // Fixed pitch scale marks
    canvas.drawFastHLine(centerX - 10, centerY - 10, 20, COLOR_ORANGE_DARK);
    canvas.drawFastHLine(centerX - 10, centerY + 10, 20, COLOR_ORANGE_DARK);
    canvas.drawFastVLine(centerX, centerY - radius, 5, COLOR_ORANGE_DIM);
    canvas.drawFastVLine(centerX, centerY + radius - 5, 5, COLOR_ORANGE_DIM);

    // Dynamic horizon line driven by real IMU tilt (if available)
    if (state.imuDataReady) {
      float rad = -state.roll * 0.0174533f; // Convert roll to radians
      int dy = (int)(state.pitch * 0.4f);   // Pitch vertical offset
      if (dy > 16) dy = 16;
      if (dy < -16) dy = -16;

      int halfLen = 18;
      int x1 = centerX - (int)(cos(rad) * halfLen);
      int y1 = (centerY + dy) - (int)(sin(rad) * halfLen);
      int x2 = centerX + (int)(cos(rad) * halfLen);
      int y2 = (centerY + dy) + (int)(sin(rad) * halfLen);

      canvas.drawLine(x1, y1, x2, y2, COLOR_ORANGE_BRIGHT);
      // Center pip
      canvas.fillCircle(centerX, centerY + dy, 2, COLOR_ORANGE_BRIGHT);
    } else {
      // Level standby horizon line
      canvas.drawFastHLine(centerX - 18, centerY, 36, COLOR_ORANGE_MID);
    }

    // Wing reference bars
    canvas.drawFastHLine(56, centerY, 28, COLOR_ORANGE_DIM);
    canvas.drawFastHLine(156, centerY, 28, COLOR_ORANGE_DIM);

    // Subtle physical input feedback (Visual confirmation of lever and BTN)
    if (state.inputLeverLeft) {
      canvas.fillTriangle(48, centerY, 56, centerY - 6, 56, centerY + 6, COLOR_ORANGE_BRIGHT);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas.drawRightString("< L", 44, centerY - 4);
    }
    if (state.inputLeverRight) {
      canvas.fillTriangle(192, centerY, 184, centerY - 6, 184, centerY + 6, COLOR_ORANGE_BRIGHT);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas.drawString("R >", 196, centerY - 4);
    }
    if (state.inputLeverPush) {
      canvas.fillCircle(centerX, centerY, 7, COLOR_ORANGE_BRIGHT);
    }
    if (state.inputBtn) {
      canvas.drawRoundRect(92, 7, 56, 15, 2, COLOR_ORANGE_BRIGHT);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas.drawCenterString("BTN", 120, 9);
    }

    // ==================== BOTTOM TELEMETRY STRIP (Y: 168 to 236) ====================
    // Vertical hairline dividers
    canvas.drawFastVLine(82, 168, 68, COLOR_ORANGE_DIM);
    canvas.drawFastVLine(158, 168, 68, COLOR_ORANGE_DIM);

    char valBuf[20];

    // Column 1: CLIMATE (Temperature)
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("CLIMATE", 12, 176);
    if (state.envDataReady && state.temp > -40.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.1f C", state.temp);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "--.- C");
      canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas.drawString(valBuf, 12, 196);
    // Relative humidity
    if (state.envDataReady && state.hum > 0.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0f%% RH", state.hum);
      canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "--%% RH");
      canvas.setTextColor(COLOR_ORANGE_DARK, COLOR_BG);
    }
    canvas.drawString(valBuf, 12, 214);

    // Column 2: BARO (Pressure)
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("BARO", 90, 176);
    if (state.envDataReady && state.press > 300.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0fhPa", state.press);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "----hPa");
      canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas.drawString(valBuf, 90, 196);
    // Gas resistance (if available)
    if (state.envDataReady && state.gas > 0.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0fkOhm", state.gas / 1000.0f);
      canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "GAS: --");
      canvas.setTextColor(COLOR_ORANGE_DARK, COLOR_BG);
    }
    canvas.drawString(valBuf, 90, 214);

    // Column 3: BIOMETRICS (Pulse / SpO2)
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("PULSE", 166, 176);
    if (state.fingerDetected && state.heartRate > 35.0f && state.heartRate < 210.0f) {
      snprintf(valBuf, sizeof(valBuf), "%d BPM", (int)state.heartRate);
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "-- BPM");
      canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas.drawString(valBuf, 166, 196);
    // SpO2
    if (state.fingerDetected && state.spo2 >= 80.0f && state.spo2 <= 100.0f) {
      snprintf(valBuf, sizeof(valBuf), "%d%% SpO2", (int)state.spo2);
      canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    } else {
      snprintf(valBuf, sizeof(valBuf), "--%% SpO2");
      canvas.setTextColor(COLOR_ORANGE_DARK, COLOR_BG);
    }
    canvas.drawString(valBuf, 166, 214);

    // ==================== FLIP DOUBLE BUFFER (ZERO FLICKER) ====================
    canvas.pushSprite(0, 0);
  }
};
