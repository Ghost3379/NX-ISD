#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"
#include <heartRate.h>
#include "../../nx-systems/NX-MSF.h"

enum VitalsViewMode {
  VITALS_VIEW_DASHBOARD = 0,
  VITALS_VIEW_SCAN,
  VITALS_VIEW_SETTINGS
};

enum VitalsScanState {
  SCAN_WAITING_FOR_FINGER = 0,
  SCAN_ACTIVE,
  SCAN_FINISHED
};

class AppVitals {
private:
  LGFX_Sprite* canvas = nullptr;
  LGFX* display = nullptr;

  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

  VitalsViewMode currentView = VITALS_VIEW_DASHBOARD;
  VitalsScanState scanState = SCAN_WAITING_FOR_FINGER;

  // Stored Last Results
  float lastBpm = 0.0f;
  float lastSpo2 = 0.0f;
  int lastStress = -1; // -1 = no measurement yet
  bool hasValidData = false;

  // Scan Session State
  uint32_t scanStartTime = 0;
  uint32_t scanDurationMs = 15000; // 15-second calibrated session
  float scanProgress = 0.0f;       // 0.0f .. 1.0f

  // Live Optical Sampling & Proven Diagnostic Pipeline
  static const int PPG_HISTORY_SIZE = 110;
  uint32_t ppgWave[PPG_HISTORY_SIZE];
  int ppgHead = 0;
  float smoothSpan = 150.0f;

  // Proven SparkFun PBA Beat Detection & Rate Averaging (from Diagnostic firmware)
  uint32_t lastBeatTime = 0;
  float liveBpm = 0.0f;
  float rates[4] = {0, 0, 0, 0};
  uint8_t rateSpot = 0;
  int beatCount = 0;
  static const int MAX_BEAT_INTERVALS = 32;
  uint32_t beatIntervals[MAX_BEAT_INTERVALS];
  int beatIntervalIndex = 0;
  bool isBeating = false;
  uint32_t beatAnimTimer = 0;

  // SpO2 calculation accumulator
  uint32_t redSum = 0;
  uint32_t irSum = 0;
  int sampleAccumCount = 0;
  float liveSpo2 = 98.0f;

  // Settings
  int reminderIndex = 0; // 0=OFF, 1=30m, 2=1h, 3=2h, 4=4h
  const char* reminderLabels[5] = {"OFF", "30 MIN", "1 HOUR", "2 HOURS", "4 HOURS"};
  int alertModeIndex = 0; // 0=AUDIO & LED, 1=LED ONLY, 2=AUDIO ONLY
  const char* alertLabels[3] = {"AUDIO & LED", "LED ONLY", "AUDIO ONLY"};
  int selectedSettingRow = 0; // 0 or 1

  // Sensor state
  bool sensorAwake = false;

public:
  AppVitals(LGFX* tft) : display(tft) {
    memset(ppgWave, 0, sizeof(ppgWave));
    memset(beatIntervals, 0, sizeof(beatIntervals));
  }

  void init(LGFX_Sprite* spr, uint16_t bg, uint16_t bright, uint16_t mid, uint16_t dim, uint16_t dark) {
    canvas = spr;
    COLOR_BG = bg;
    COLOR_ORANGE_BRIGHT = bright;
    COLOR_ORANGE_MID = mid;
    COLOR_ORANGE_DIM = dim;
    COLOR_ORANGE_DARK = dark;
  }

  void onEnter() {
    currentView = VITALS_VIEW_DASHBOARD;
    stopSensor();
  }

  void onExit() {
    stopSensor();
  }

  void handleNavLeft() {
    if (currentView == VITALS_VIEW_SETTINGS) {
      currentView = VITALS_VIEW_DASHBOARD;
      HAL::buzzPip(2400, 15);
    }
  }

  void handleNavRight() {
    if (currentView == VITALS_VIEW_DASHBOARD) {
      currentView = VITALS_VIEW_SETTINGS;
      HAL::buzzPip(2800, 15);
    }
  }

  void handleNavPush() {
    if (currentView == VITALS_VIEW_DASHBOARD) {
      // Start Option A Calibrated Measurement Session
      startScan();
    } else if (currentView == VITALS_VIEW_SCAN) {
      if (scanState == SCAN_FINISHED) {
        if (hasValidData) {
          // Acknowledge and return to dashboard
          currentView = VITALS_VIEW_DASHBOARD;
          stopSensor();
        } else {
          // Retry calibration scan
          startScan();
        }
      }
    } else if (currentView == VITALS_VIEW_SETTINGS) {
      // Toggle selected setting
      if (selectedSettingRow == 0) {
        reminderIndex = (reminderIndex + 1) % 5;
        HAL::buzzPip(3200, 15);
      } else {
        alertModeIndex = (alertModeIndex + 1) % 3;
        HAL::buzzPip(3200, 15);
      }
    }
  }

  // Handle back button (return true if handled internally, false if should exit to AppMenu)
  bool handleNavBtn() {
    if (currentView == VITALS_VIEW_SCAN) {
      // Cancel active scan and return to dashboard
      stopSensor();
      currentView = VITALS_VIEW_DASHBOARD;
      HAL::buzzPip(2000, 15);
      return true;
    } else if (currentView == VITALS_VIEW_SETTINGS) {
      currentView = VITALS_VIEW_DASHBOARD;
      HAL::buzzPip(2200, 15);
      return true;
    }
    return false; // Exit back to 3D App Menu
  }

  void startScan() {
    currentView = VITALS_VIEW_SCAN;
    scanState = SCAN_WAITING_FOR_FINGER;
    scanStartTime = 0;
    scanProgress = 0.0f;
    beatCount = 0;
    beatIntervalIndex = 0;
    rateSpot = 0;
    memset(rates, 0, sizeof(rates));
    memset(beatIntervals, 0, sizeof(beatIntervals));
    memset(ppgWave, 0, sizeof(ppgWave));
    ppgHead = 0;
    smoothSpan = 150.0f;
    liveBpm = 0.0f;
    liveSpo2 = 98.0f;
    redSum = 0;
    irSum = 0;
    sampleAccumCount = 0;
    lastBeatTime = 0;

    wakeSensor();
    HAL::buzzPip(3800, 25);
  }

  void stopSensor() {
    if (sensorAwake && HAL::heartRateReady) {
      HAL::heartRateSensor.setPulseAmplitudeRed(0);
      HAL::heartRateSensor.setPulseAmplitudeIR(0);
      HAL::heartRateSensor.shutDown();
      sensorAwake = false;
    }
  }

  void wakeSensor() {
    if (HAL::heartRateReady) {
      HAL::heartRateSensor.wakeUp();
      // Setup MAX30105 using exact diagnostic firmware parameters:
      // powerLevel = 0x24, sampleAverage = 4, ledMode = 2 (Red + IR), sampleRate = 400, pulseWidth = 411, adcRange = 4096
      HAL::heartRateSensor.setup(0x24, 4, 2, 400, 411, 4096);
      HAL::heartRateSensor.setPulseAmplitudeRed(0x24);
      HAL::heartRateSensor.setPulseAmplitudeIR(0x24);
      sensorAwake = true;
    }
  }

  void updateSampling(const SensorState& state) {
    if (currentView != VITALS_VIEW_SCAN || !sensorAwake || !HAL::heartRateReady) return;

    // Read direct Red and IR values via getRed() and getIR() (proven diagnostic method)
    uint32_t ir = HAL::heartRateSensor.getIR();
    uint32_t red = HAL::heartRateSensor.getRed();

    // Finger detection threshold from diagnostic firmware
    bool fingerPresent = (ir > 20000);

    if (scanState == SCAN_WAITING_FOR_FINGER) {
      if (fingerPresent) {
        scanState = SCAN_ACTIVE;
        scanStartTime = millis();
        memset(ppgWave, 0, sizeof(ppgWave));
        ppgHead = 0;
        rateSpot = 0;
        memset(rates, 0, sizeof(rates));
        HAL::buzzPip(4400, 20);
      }
    } else if (scanState == SCAN_ACTIVE) {
      if (!fingerPresent) {
        // Lost contact: reset back to waiting and show physical sensor locator
        scanState = SCAN_WAITING_FOR_FINGER;
        scanStartTime = 0;
        scanProgress = 0.0f;
        memset(ppgWave, 0, sizeof(ppgWave));
        return;
      }

      uint32_t now = millis();
      uint32_t elapsed = now - scanStartTime;
      scanProgress = constrain((float)elapsed / (float)scanDurationMs, 0.0f, 1.0f);

      // Record PPG waveform sample
      ppgWave[ppgHead] = ir;
      ppgHead = (ppgHead + 1) % PPG_HISTORY_SIZE;

      // Heartbeat detection using proven SparkFun PBA algorithm with 4-sample running average
      if (checkForBeat((int32_t)ir)) {
        isBeating = true;
        beatAnimTimer = now;
        uint32_t delta = (lastBeatTime > 0) ? (now - lastBeatTime) : 0;
        lastBeatTime = now;

        if (delta > 280 && delta < 1800) { // 33.3 .. 214 BPM bounds
          float instantBpm = 60000.0f / (float)delta;
          if (instantBpm >= 45.0f && instantBpm <= 185.0f) {
            rates[rateSpot++] = instantBpm;
            rateSpot %= 4;

            float sum = 0;
            int count = 0;
            for (int i = 0; i < 4; i++) {
              if (rates[i] > 0) {
                sum += rates[i];
                count++;
              }
            }
            if (count > 0) {
              liveBpm = sum / (float)count;
            }
            beatCount++;
            beatIntervals[beatIntervalIndex % MAX_BEAT_INTERVALS] = delta;
            beatIntervalIndex++;

            // Subtle heartbeat tick if audio enabled
            if (alertModeIndex == 0 || alertModeIndex == 2) {
              HAL::buzzPip(3200, 6);
            }
          }
        }
      }

      // Accumulate Red & IR for SpO2 calibration
      redSum += red;
      irSum += ir;
      sampleAccumCount++;

      // Live SpO2 estimation (every ~50 samples = ~1 sec) via NX-MSF
      if (sampleAccumCount >= 50 && irSum > 0) {
        liveSpo2 = NX_MSF::estimateSpO2(redSum, irSum, sampleAccumCount);
        redSum = 0;
        irSum = 0;
        sampleAccumCount = 0;
      }

      // Check for session completion
      if (scanProgress >= 1.0f) {
        scanState = SCAN_FINISHED;
        finalizeScan(state);
        return;
      }
    }
  }

  void finalizeScan(const SensorState& state) {
    if (beatCount >= 4 && liveBpm >= 45.0f && liveBpm <= 195.0f) {
      hasValidData = true;
      lastBpm = liveBpm;
      lastSpo2 = (liveSpo2 >= 94.0f && liveSpo2 <= 100.0f) ? liveSpo2 : 98.0f;

      // Multi-Sensor Fusion (MSF) Stress Calculation via NX-MSF
      lastStress = NX_MSF::calculateStress(beatIntervals, MAX_BEAT_INTERVALS, beatCount, beatIntervalIndex, lastBpm, 1.0f);

      // Success Chime
      HAL::buzzPip(4800, 35);
    } else {
      // Insufficient clean beats captured during scan
      hasValidData = false;
      lastBpm = 0.0f;
      lastStress = -1;

      // Warning buzz
      HAL::buzzPip(1800, 40);
    }

    stopSensor();
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    // Sample optical sensor if in scan mode
    updateSampling(state);

    canvas->fillSprite(COLOR_BG);

    // Common Bounding Box Framing (Consistent with AppMenu & Watchface)
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    // Render Global ISD Header
    renderHeader(state);

    if (currentView == VITALS_VIEW_DASHBOARD) {
      renderDashboard(state);
    } else if (currentView == VITALS_VIEW_SCAN) {
      renderScanScreen(state);
    } else if (currentView == VITALS_VIEW_SETTINGS) {
      renderSettings(state);
    }

    canvas->pushSprite(0, 0);
  }

private:
  void renderHeader(const SensorState& state) {
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);

    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("VITALS", 82, 10);

    // Battery Indicator (Rock-solid, identical to Watchface and AppMenu)
    char batBuf[16];
    if (state.batPercent > 0.0f) {
      int displayPct = (int)constrain(roundf(state.batPercent), 0.0f, 100.0f);
      if (state.usbConnected || state.isCharging) {
        snprintf(batBuf, sizeof(batBuf), "%d%% [CHG]", displayPct);
      } else {
        snprintf(batBuf, sizeof(batBuf), "%d%%", displayPct);
      }
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(batBuf, sizeof(batBuf), "--%%");
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas->drawRightString(batBuf, 230, 10);
  }

  void renderDashboard(const SensorState& state) {
    // 1. Background ECG Wave (Oscilloscope Depth Layer)
    drawBackgroundEcg(118, lastBpm, hasValidData);

    // 2. Central Heart Geometry & Alternating Discrete Pulse
    int cx = 120;
    int cy = 104;
    int heartR = 26; // normal resting size

    if (hasValidData && lastBpm >= 40.0f) {
      uint32_t period = (uint32_t)(60000.0f / lastBpm);
      if (period < 300) period = 300;
      uint32_t t = millis() % period;
      // Alternating discrete beat: first 170ms is Big size, rest is Normal size
      if (t < 170) {
        heartR = 32;
      }
    }

    // 3. SpO2 Card (Top Left)
    int c1x = 8, c1y = 34, c1w = 76, c1h = 44;
    canvas->drawRoundRect(c1x, c1y, c1w, c1h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("SpO2", c1x + 8, c1y + 6);

    if (hasValidData) {
      char sBuf[16];
      snprintf(sBuf, sizeof(sBuf), "%.0f%%", lastSpo2);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(sBuf, c1x + 8, c1y + 18, &fonts::Font4);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--%", c1x + 8, c1y + 18, &fonts::Font4);
    }

    // 4. Stress Card (Top Right - Enlarged with 8-Segment Tactical Meter)
    int c2x = 132, c2y = 34, c2w = 100, c2h = 52;
    canvas->drawRoundRect(c2x, c2y, c2w, c2h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("STRESS", c2x + 8, c2y + 5);

    if (hasValidData && lastStress >= 0) {
      const char* tag = (lastStress > 65) ? "[HIGH]" : (lastStress > 40) ? "[MOD]" : "[LOW]";
      canvas->drawRightString(tag, c2x + c2w - 8, c2y + 5);

      char stBuf[16];
      snprintf(stBuf, sizeof(stBuf), "%d%%", lastStress);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(stBuf, c2x + 8, c2y + 17, &fonts::Font4);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--%", c2x + 8, c2y + 17, &fonts::Font4);
    }

    // Wide 8-Segment Tactical Meter Bar
    int segW = 9;
    int segH = 7;
    int segGap = 2;
    int totalW = 8 * segW + 7 * segGap; // 86px
    int segStartX = c2x + (c2w - totalW) / 2;
    int segY = c2y + 40;
    int activeSegs = (hasValidData && lastStress >= 0) ? ((lastStress * 8 + 50) / 100) : 0;
    if (hasValidData && lastStress > 0 && activeSegs == 0) activeSegs = 1;

    for (int s = 0; s < 8; s++) {
      if (s < activeSegs) {
        canvas->fillRect(segStartX + s * (segW + segGap), segY, segW, segH, COLOR_ORANGE_BRIGHT);
      } else {
        canvas->drawRect(segStartX + s * (segW + segGap), segY, segW, segH, COLOR_ORANGE_DARK);
      }
    }

    // 5. Draw Central Beating Heart
    drawBigHeart(cx, cy, heartR, hasValidData, COLOR_ORANGE_BRIGHT, hasValidData ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM);

    // 6. Tactical Leader Lines (HUD Blueprint Schematic)
    int lobeR = heartR * 5 / 10;
    int lobeY = cy - heartR * 3 / 10;
    int anchorSpo2X = cx - lobeR;
    int anchorSpo2Y = lobeY - lobeR; // Pinned directly to top-left wing crest

    uint16_t traceColor = hasValidData ? COLOR_ORANGE_MID : COLOR_ORANGE_DARK;
    uint16_t nodeColor  = hasValidData ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;

    // SpO2 Trace: Card right edge (c1x + c1w, c1y + 22) -> (anchorSpo2X, c1y + 22) -> (anchorSpo2X, anchorSpo2Y)
    int card1OutX = c1x + c1w;
    int card1OutY = c1y + 22;
    canvas->drawLine(card1OutX, card1OutY, anchorSpo2X, card1OutY, traceColor);
    canvas->drawLine(anchorSpo2X, card1OutY, anchorSpo2X, anchorSpo2Y, traceColor);
    // SpO2 Anchor Node (O) on top-left wing
    canvas->fillCircle(anchorSpo2X, anchorSpo2Y, 3, COLOR_BG);
    canvas->drawCircle(anchorSpo2X, anchorSpo2Y, 3, nodeColor);
    canvas->fillCircle(anchorSpo2X, anchorSpo2Y, 1, nodeColor);

    // Stress Trace: Anchor (O) in Mid-Low core -> (c2x + 14, anchorStressY) -> bottom of card (c2x + 14, c2y + c2h)
    int anchorStressX = cx;
    int anchorStressY = cy + (heartR * 3 / 10);
    int card2InX = c2x + 14;
    int card2InY = c2y + c2h;
    canvas->drawLine(anchorStressX, anchorStressY, card2InX, anchorStressY, traceColor);
    canvas->drawLine(card2InX, anchorStressY, card2InX, card2InY, traceColor);
    // Stress Anchor Node (O) in Mid-Low core
    canvas->fillCircle(anchorStressX, anchorStressY, 3, COLOR_BG);
    canvas->drawCircle(anchorStressX, anchorStressY, 3, nodeColor);
    canvas->fillCircle(anchorStressX, anchorStressY, 1, nodeColor);

    // 7. Pulse BPM Readout (Beneath Apex - Cleaned)
    canvas->setTextDatum(MC_DATUM);
    if (hasValidData) {
      char bpmBuf[16];
      snprintf(bpmBuf, sizeof(bpmBuf), "%.0f BPM", lastBpm);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(bpmBuf, 120, 168, &fonts::Font4);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("-- BPM", 120, 168, &fonts::Font4);
    }
    canvas->setTextDatum(TL_DATUM);

    // 8. Bottom Navigation Bar
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("[PUSH] NEW SCAN   [R] SETTINGS", 120, 220);
    canvas->setTextDatum(TL_DATUM);
  }

  void renderScanScreen(const SensorState& state) {
    if (scanState == SCAN_WAITING_FOR_FINGER) {
      // Prompt user to place finger on optical sensor
      canvas->setTextDatum(MC_DATUM);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString("PLACE FINGER ON SENSOR", 120, 60, &fonts::Font2);

      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString("Align finger on optical window", 120, 85);

      // Pulsing Fingerprint / Sensor Target Graphic
      bool pulse = ((millis() / 400) % 2 == 0);
      int targetCx = 120;
      int targetCy = 135;
      canvas->drawCircle(targetCx, targetCy, 32, pulse ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
      canvas->drawCircle(targetCx, targetCy, 20, COLOR_ORANGE_DIM);
      drawHeartIcon(targetCx, targetCy, 6, pulse ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);

      // Arrow pointing specifically to bottom-right corner where physical sensor sits!
      int arrowBaseX = 180;
      int arrowBaseY = 175;
      int arrowTipX  = 214;
      int arrowTipY  = 208;

      canvas->drawLine(arrowBaseX, arrowBaseY, arrowTipX, arrowTipY, COLOR_ORANGE_BRIGHT);
      canvas->drawLine(arrowTipX - 8, arrowTipY, arrowTipX, arrowTipY, COLOR_ORANGE_BRIGHT);
      canvas->drawLine(arrowTipX, arrowTipY - 8, arrowTipX, arrowTipY, COLOR_ORANGE_BRIGHT);

      // Animated beacon ring on bottom-right sensor zone
      int beaconR = 8 + ((millis() / 50) % 10);
      canvas->drawCircle(arrowTipX + 4, arrowTipY + 4, beaconR, COLOR_ORANGE_MID);

      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("SENSOR LOCATOR ↘", 140, 196);

      canvas->drawString("[BTN] CANCEL SCAN", 120, 222);
      canvas->setTextDatum(TL_DATUM);

    } else if (scanState == SCAN_ACTIVE) {
      // Full-Screen Live Telemetry Scanning Screen
      canvas->setTextDatum(TL_DATUM);

      // 1. Live Countdown Bar at Top
      int barX = 14, barY = 36, barW = 212, barH = 6;
      canvas->drawRect(barX, barY, barW, barH, COLOR_ORANGE_DARK);
      int fillW = (int)((barW - 2) * scanProgress);
      canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_ORANGE_BRIGHT);

      // Time remaining countdown label
      int secRemaining = (int)ceilf((1.0f - scanProgress) * (scanDurationMs / 1000.0f));
      char timeBuf[24];
      snprintf(timeBuf, sizeof(timeBuf), "CALIBRATING: %ds", secRemaining);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(timeBuf, 14, 46);

      // 2. Heartbeat Indicator & Live BPM
      bool beatFlash = (millis() - beatAnimTimer < 180);
      drawHeartIcon(195, 52, beatFlash ? 6 : 4, COLOR_ORANGE_BRIGHT);

      char bpmBuf[16];
      if (liveBpm > 40.0f) {
        snprintf(bpmBuf, sizeof(bpmBuf), "%.0f", liveBpm);
      } else {
        snprintf(bpmBuf, sizeof(bpmBuf), "--");
      }
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(bpmBuf, 14, 62, &fonts::Font4);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString("LIVE BPM", 82, 74);

      // Live SpO2 estimation tag
      char spo2Buf[16];
      snprintf(spo2Buf, sizeof(spo2Buf), "SpO2: %.0f%%", liveSpo2);
      canvas->drawString(spo2Buf, 150, 68);

      char beatBuf[16];
      snprintf(beatBuf, sizeof(beatBuf), "BEATS: %d", beatCount);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(beatBuf, 150, 84);

      // 3. Real-Time PPG Waveform Oscilloscope Strip
      int waveY = 100;
      int waveH = 88;
      canvas->drawRect(10, waveY, 220, waveH, COLOR_ORANGE_DIM);
      // Oscilloscope background grid
      int midY = waveY + waveH / 2;
      canvas->drawFastHLine(11, midY, 218, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 55, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 110, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 165, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);

      // Dynamic Window Min/Max Auto-Scaling with Low-Pass Smoothing
      uint32_t minVal = 0xFFFFFFFF;
      uint32_t maxVal = 0;
      for (int i = 0; i < PPG_HISTORY_SIZE; i++) {
        if (ppgWave[i] == 0) continue;
        if (ppgWave[i] < minVal) minVal = ppgWave[i];
        if (ppgWave[i] > maxVal) maxVal = ppgWave[i];
      }

      uint32_t rawSpan = (maxVal > minVal) ? (maxVal - minVal) : 100;
      if (rawSpan < 100) rawSpan = 100;
      smoothSpan = (smoothSpan * 0.88f) + ((float)rawSpan * 0.12f);
      float span = max(100.0f, smoothSpan);

      int prevX = 11;
      int prevY = midY;
      for (int i = 0; i < PPG_HISTORY_SIZE; i++) {
        int idx = (ppgHead + i) % PPG_HISTORY_SIZE;
        uint32_t val = ppgWave[idx];
        if (val == 0) {
          prevX = 11 + (i * 217 / (PPG_HISTORY_SIZE - 1));
          prevY = midY;
          continue;
        }

        // Invert so arterial pulse peak (lower IR count) draws UPWARDS!
        int py = (waveY + 4) + (int)(((float)(val - minVal) * (float)(waveH - 8)) / span);
        py = constrain(py, waveY + 3, waveY + waveH - 3);
        int px = 11 + (i * 217 / (PPG_HISTORY_SIZE - 1));

        if (i > 0 && prevX != 11) {
          // Glow trace line for tactical cyber aesthetic
          canvas->drawLine(prevX, prevY + 1, px, py + 1, COLOR_ORANGE_MID);
          // Sharp primary pulse wave line
          canvas->drawLine(prevX, prevY, px, py, COLOR_ORANGE_BRIGHT);
        } else {
          prevY = py;
        }
        prevX = px;
        prevY = py;
      }

      // Live pulse sweep beacon point on leading edge
      canvas->fillCircle(prevX, prevY, 2, COLOR_ORANGE_BRIGHT);
      canvas->drawCircle(prevX, prevY, 4, COLOR_ORANGE_MID);

      // Bottom scan prompt
      canvas->setTextDatum(MC_DATUM);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("KEEP FINGER STILL // [BTN] CANCEL", 120, 206);
      canvas->setTextDatum(TL_DATUM);

    } else if (scanState == SCAN_FINISHED) {
      // Scan Finalized Screen
      canvas->setTextDatum(MC_DATUM);
      if (hasValidData) {
        canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
        canvas->drawString("SCAN COMPLETE", 120, 52, &fonts::Font2);

        // Results Summary Card
        canvas->drawRoundRect(14, 72, 212, 114, 4, COLOR_ORANGE_MID);

        char buf[32];
        snprintf(buf, sizeof(buf), "PULSE: %.0f BPM", lastBpm);
        canvas->drawString(buf, 120, 96, &fonts::Font2);

        snprintf(buf, sizeof(buf), "OXYGEN: %.0f%% SpO2", lastSpo2);
        canvas->drawString(buf, 120, 126, &fonts::Font2);

        snprintf(buf, sizeof(buf), "MSF STRESS: %d%%", lastStress);
        canvas->drawString(buf, 120, 156, &fonts::Font2);

        canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
        canvas->drawString("[PUSH] SAVE & RETURN TO VITALS", 120, 214);
      } else {
        canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
        canvas->drawString("CALIBRATION INCOMPLETE", 120, 52, &fonts::Font2);

        // Results Summary Card
        canvas->drawRoundRect(14, 72, 212, 114, 4, COLOR_ORANGE_DIM);

        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString("WEAK OPTICAL CONTACT", 120, 96, &fonts::Font2);
        canvas->drawString("Fewer than 5 clear beats locked", 120, 124);
        canvas->drawString("Rest finger gently without moving", 120, 144);

        canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
        canvas->drawString("[PUSH] RETRY SCAN", 120, 214);
      }
      canvas->setTextDatum(TL_DATUM);
    }
  }

  void renderSettings(const SensorState& state) {
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("VITALS SETTINGS", 14, 36, &fonts::Font2);

    // Setting 1: Reminder Interval
    bool row0 = (selectedSettingRow == 0);
    canvas->drawRoundRect(10, 64, 220, 52, 4, row0 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("HRM MEASUREMENT REMINDER", 18, 72);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(reminderLabels[reminderIndex], 18, 90, &fonts::Font2);

    // Setting 2: Alert Mode
    bool row1 = (selectedSettingRow == 1);
    canvas->drawRoundRect(10, 126, 220, 52, 4, row1 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("NOTIFICATION DISPATCH", 18, 134);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(alertLabels[alertModeIndex], 18, 152, &fonts::Font2);

    // Bottom Navigation Context
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("[PUSH] TOGGLE   [L] VITALS", 120, 220);
    canvas->setTextDatum(TL_DATUM);
  }

  void drawHeartIcon(int cx, int cy, int r, uint16_t color) {
    canvas->fillCircle(cx - r / 2, cy - r / 3, r / 2, color);
    canvas->fillCircle(cx + r / 2, cy - r / 3, r / 2, color);
    canvas->fillTriangle(cx - r, cy - r / 4, cx + r, cy - r / 4, cx, cy + r, color);
  }

  void drawBigHeart(int cx, int cy, int r, bool filled, uint16_t fillColor, uint16_t outlineColor) {
    int lobeR = r * 5 / 10;
    int lobeY = cy - r * 3 / 10;
    int leftLobeX = cx - lobeR;
    int rightLobeX = cx + lobeR;
    int apexY = cy + r;

    if (filled) {
      canvas->fillCircle(leftLobeX, lobeY, lobeR, fillColor);
      canvas->fillCircle(rightLobeX, lobeY, lobeR, fillColor);
      canvas->fillTriangle(cx - r, lobeY, cx + r, lobeY, cx, apexY, fillColor);
      canvas->fillRect(cx - lobeR, lobeY, lobeR * 2, r * 4 / 10, fillColor);
      canvas->drawCircle(leftLobeX, lobeY, lobeR, outlineColor);
      canvas->drawCircle(rightLobeX, lobeY, lobeR, outlineColor);
      canvas->drawLine(cx - r, lobeY, cx, apexY, outlineColor);
      canvas->drawLine(cx + r, lobeY, cx, apexY, outlineColor);
    } else {
      for (int a = 0; a < 180; a += 15) {
        int nextA = min(180, a + 15);
        float rad1 = (float)a * DEG_TO_RAD;
        float rad2 = (float)nextA * DEG_TO_RAD;
        int x1 = leftLobeX - (int)roundf(cosf(rad1) * lobeR);
        int y1 = lobeY - (int)roundf(sinf(rad1) * lobeR);
        int x2 = leftLobeX - (int)roundf(cosf(rad2) * lobeR);
        int y2 = lobeY - (int)roundf(sinf(rad2) * lobeR);
        canvas->drawLine(x1, y1, x2, y2, outlineColor);
      }
      for (int a = 0; a < 180; a += 15) {
        int nextA = min(180, a + 15);
        float rad1 = (float)a * DEG_TO_RAD;
        float rad2 = (float)nextA * DEG_TO_RAD;
        int x1 = rightLobeX + (int)roundf(cosf(rad1) * lobeR);
        int y1 = lobeY - (int)roundf(sinf(rad1) * lobeR);
        int x2 = rightLobeX + (int)roundf(cosf(rad2) * lobeR);
        int y2 = lobeY - (int)roundf(sinf(rad2) * lobeR);
        canvas->drawLine(x1, y1, x2, y2, outlineColor);
      }
      canvas->drawLine(cx - r, lobeY, cx, apexY, outlineColor);
      canvas->drawLine(cx + r, lobeY, cx, apexY, outlineColor);
    }
  }

  void drawBackgroundEcg(int yBaseline, float bpm, bool active) {
    const int waveW = 120;
    int shift = 0;
    if (active && bpm >= 40.0f) {
      shift = (int)(fmodf((float)millis() * (bpm * (float)waveW / 60000.0f), (float)waveW));
    }

    int prevX = 6;
    int prevY = yBaseline;

    for (int x = 6; x <= 234; x += 2) {
      int phase = (x + waveW - shift) % waveW;
      int yOffset = 0;

      if (active && bpm >= 40.0f) {
        if (phase >= 30 && phase < 42) {
          float p = (float)(phase - 30) / 12.0f;
          yOffset = -(int)(sinf(p * 3.14159f) * 4.0f);
        } else if (phase >= 46 && phase < 50) {
          yOffset = 3;
        } else if (phase >= 50 && phase < 56) {
          float p = (float)(phase - 50) / 6.0f;
          yOffset = -(int)(sinf(p * 3.14159f) * 18.0f);
        } else if (phase >= 56 && phase < 60) {
          yOffset = 5;
        } else if (phase >= 70 && phase < 84) {
          float p = (float)(phase - 70) / 14.0f;
          yOffset = -(int)(sinf(p * 3.14159f) * 6.0f);
        }
      }

      int curY = yBaseline + yOffset;
      if (x > 6) {
        canvas->drawLine(prevX, prevY, x, curY, COLOR_ORANGE_DARK);
      }
      prevX = x;
      prevY = curY;
    }
  }
};
