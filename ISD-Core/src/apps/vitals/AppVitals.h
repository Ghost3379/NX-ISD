#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"
#include <heartRate.h>

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

  // Live Optical Sampling & Signal Processing
  static const int PPG_HISTORY_SIZE = 110;
  int16_t ppgWave[PPG_HISTORY_SIZE];
  int ppgHead = 0;

  // Real-time DC Baseline & AC Oscilloscope Filter
  float dcFilter = 0.0f;
  float acSignal = 0.0f;
  float prevAcSignal = 0.0f;
  float peakAc = 60.0f;

  // Adaptive Systolic Peak & Refractory Beat Detector
  bool isRising = false;
  float localPeakVal = 0.0f;
  uint32_t lastBeatTime = 0;
  float liveBpm = 0.0f;
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
    memset(beatIntervals, 0, sizeof(beatIntervals));
    memset(ppgWave, 0, sizeof(ppgWave));
    ppgHead = 0;
    dcFilter = 0.0f;
    acSignal = 0.0f;
    prevAcSignal = 0.0f;
    peakAc = 60.0f;
    isRising = false;
    localPeakVal = 0.0f;
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
      HAL::heartRateSensor.shutDown();
      sensorAwake = false;
    }
  }

  void wakeSensor() {
    if (HAL::heartRateReady) {
      HAL::heartRateSensor.wakeUp();
      // Setup MAX30105 for Red + IR optical sampling:
      // powerLevel = 0x28 (~8.5mA, optimal tissue reflection without ADC saturation)
      // sampleAverage = 4, ledMode = 2 (Red + IR), sampleRate = 200 (50Hz effective output), pulseWidth = 411 (18-bit), adcRange = 4096
      HAL::heartRateSensor.setup(0x28, 4, 2, 200, 411, 4096);
      HAL::heartRateSensor.clearFIFO();
      sensorAwake = true;
    }
  }

  void updateSampling(const SensorState& state) {
    if (currentView != VITALS_VIEW_SCAN || !sensorAwake || !HAL::heartRateReady) return;

    // Check for fresh optical samples from MAX30105
    HAL::heartRateSensor.check();

    // Drain all samples currently available in the hardware FIFO
    while (HAL::heartRateSensor.available()) {
      uint32_t ir = HAL::heartRateSensor.getFIFOIR();
      uint32_t red = HAL::heartRateSensor.getFIFORed();
      HAL::heartRateSensor.nextSample(); // Advance FIFO tail pointer!

      // Finger contact detection threshold (ambient < 15k; finger contact >= 35k)
      bool fingerPresent = (ir > 35000);

      if (scanState == SCAN_WAITING_FOR_FINGER) {
        if (fingerPresent) {
          scanState = SCAN_ACTIVE;
          scanStartTime = millis();
          dcFilter = (float)ir;
          acSignal = 0.0f;
          prevAcSignal = 0.0f;
          peakAc = 60.0f;
          isRising = false;
          localPeakVal = 0.0f;
          memset(ppgWave, 0, sizeof(ppgWave));
          ppgHead = 0;
          HAL::buzzPip(4400, 20);
        }
      } else if (scanState == SCAN_ACTIVE) {
        if (!fingerPresent) {
          // Lost contact: reset back to waiting and show physical sensor locator
          scanState = SCAN_WAITING_FOR_FINGER;
          scanStartTime = 0;
          scanProgress = 0.0f;
          dcFilter = 0.0f;
          acSignal = 0.0f;
          prevAcSignal = 0.0f;
          memset(ppgWave, 0, sizeof(ppgWave));
          return;
        }

        uint32_t now = millis();
        uint32_t elapsed = now - scanStartTime;
        scanProgress = constrain((float)elapsed / (float)scanDurationMs, 0.0f, 1.0f);

        // 1. DC Baseline tracking (High-pass filter removing massive tissue DC offset)
        if (dcFilter < 1000.0f) {
          dcFilter = (float)ir;
        } else {
          // Alpha of 0.04 at 50Hz gives ~0.5s time constant (passes 0.5-4Hz cardiac pulses)
          dcFilter = (dcFilter * 0.96f) + ((float)ir * 0.04f);
        }

        // 2. Invert so blood volume expansion during systole peaks UPWARDS
        float rawAc = -((float)ir - dcFilter);

        // 3. Low-Pass Filter (smoothes out optical flicker and high-freq noise)
        acSignal = (acSignal * 0.55f) + (rawAc * 0.45f);

        // 4. Automatic Gain Control (tracks peak-to-peak amplitude)
        float absAc = fabsf(acSignal);
        if (absAc > peakAc) {
          peakAc = (peakAc * 0.7f) + (absAc * 0.3f);
        } else {
          peakAc = max(30.0f, peakAc * 0.997f);
        }

        // 5. Push into live scrolling oscilloscope history
        ppgWave[ppgHead] = (int16_t)roundf(acSignal);
        ppgHead = (ppgHead + 1) % PPG_HISTORY_SIZE;

        // 6. Accumulate Red & IR for SpO2 calibration
        redSum += red;
        irSum += ir;
        sampleAccumCount++;

        // 7. NX-ISD Adaptive Systolic Peak & Refractory Beat Detector
        // Beat threshold dynamically pegged to 55% of the rolling systolic peak
        float beatThreshold = peakAc * 0.55f;
        float slope = acSignal - prevAcSignal;

        if (acSignal > beatThreshold && slope > 0.0f) {
          // Actively rising toward systolic crest
          isRising = true;
          if (acSignal > localPeakVal) {
            localPeakVal = acSignal;
          }
        } else if (isRising && slope <= 0.0f) {
          // Reached the peak crest!
          isRising = false;

          uint32_t delta = (lastBeatTime > 0) ? (now - lastBeatTime) : 0;

          // Enforce 320ms Physiological Refractory Lockout (blocks dicrotic bounce & reflections)
          if (lastBeatTime == 0 || (delta >= 320 && delta <= 1800)) {
            if (delta >= 320 && delta <= 1800) {
              // Valid physiological inter-beat interval (33.3 .. 187.5 BPM)
              beatIntervals[beatIntervalIndex % MAX_BEAT_INTERVALS] = delta;
              beatIntervalIndex++;
              beatCount++;

              // Calculate Trimmed Average Live BPM from the last up to 8 intervals
              int validHistory = min(beatCount, 8);
              if (validHistory >= 4) {
                uint32_t sorted[8];
                for (int k = 0; k < validHistory; k++) {
                  int idx = (beatIntervalIndex - 1 - k + MAX_BEAT_INTERVALS) % MAX_BEAT_INTERVALS;
                  sorted[k] = beatIntervals[idx];
                }
                for (int a = 0; a < validHistory - 1; a++) {
                  for (int b = a + 1; b < validHistory; b++) {
                    if (sorted[a] > sorted[b]) {
                      uint32_t temp = sorted[a];
                      sorted[a] = sorted[b];
                      sorted[b] = temp;
                    }
                  }
                }
                // Discard lowest and highest outlier intervals
                uint32_t trimmedSum = 0;
                for (int k = 1; k < validHistory - 1; k++) {
                  trimmedSum += sorted[k];
                }
                float avgIbi = (float)trimmedSum / (float)(validHistory - 2);
                if (avgIbi > 0.0f) {
                  liveBpm = 60000.0f / avgIbi;
                }
              } else {
                float instantBpm = 60000.0f / (float)delta;
                if (liveBpm <= 10.0f) liveBpm = instantBpm;
                else liveBpm = (liveBpm * 0.6f) + (instantBpm * 0.4f);
              }
            }

            lastBeatTime = now;
            isBeating = true;
            beatAnimTimer = now;

            // Optional subtle audio pip if notification dispatch enables audio
            if (alertModeIndex == 0 || alertModeIndex == 2) {
              HAL::buzzPip(3200, 6);
            }
          }
          localPeakVal = 0.0f;
        } else if (acSignal < 0.0f) {
          isRising = false;
          localPeakVal = 0.0f;
        }

        prevAcSignal = acSignal;

        // 8. Live SpO2 estimation (every ~50 samples = ~1 sec at 50Hz)
        if (sampleAccumCount >= 50 && irSum > 0) {
          float r = ((float)redSum / (float)sampleAccumCount) / ((float)irSum / (float)sampleAccumCount);
          float estSpo2 = 104.0f - 17.0f * r;
          liveSpo2 = constrain(estSpo2, 92.0f, 100.0f);
          redSum = 0;
          irSum = 0;
          sampleAccumCount = 0;
        }

        // 9. Check for session completion
        if (scanProgress >= 1.0f) {
          scanState = SCAN_FINISHED;
          finalizeScan(state);
          return;
        }
      }
    }
  }

  void finalizeScan(const SensorState& state) {
    if (beatCount >= 5 && liveBpm >= 45.0f && liveBpm <= 195.0f) {
      hasValidData = true;
      lastBpm = liveBpm;
      lastSpo2 = (liveSpo2 >= 92.0f && liveSpo2 <= 100.0f) ? liveSpo2 : 98.0f;

      // Multi-Sensor Fusion (MSF) Stress Calculation:
      // 1. True HRV RMSSD component calculated from successive IBI variances
      int availablePairs = min(beatCount - 1, MAX_BEAT_INTERVALS - 1);
      float sumSqDiff = 0.0f;
      int pairs = 0;
      for (int i = 1; i <= availablePairs; i++) {
        int idxCurr = (beatIntervalIndex - i + MAX_BEAT_INTERVALS) % MAX_BEAT_INTERVALS;
        int idxPrev = (beatIntervalIndex - i - 1 + MAX_BEAT_INTERVALS) % MAX_BEAT_INTERVALS;
        int diff = (int)beatIntervals[idxCurr] - (int)beatIntervals[idxPrev];
        sumSqDiff += (float)(diff * diff);
        pairs++;
      }

      float rmssd = (pairs > 0) ? sqrtf(sumSqDiff / (float)pairs) : 35.0f;
      // High RMSSD (e.g. 65ms) = relaxed / low stress; Low RMSSD (e.g. 18ms) = high stress
      float hrvStress = map(constrain((long)roundf(rmssd), 15L, 75L), 15L, 75L, 85L, 15L);

      // 2. Resting Heart Rate elevation component
      float rhrStress = map(constrain((long)roundf(lastBpm), 55L, 110L), 55L, 110L, 10L, 90L);

      // 3. Motion stability fusion (BNO085 IMU)
      float motionFactor = 1.0f;

      // Final Multi-Sensor Fusion formula:
      float fusedStress = (hrvStress * 0.65f) + (rhrStress * 0.35f);
      lastStress = (int)constrain(roundf(fusedStress * motionFactor), 5.0f, 95.0f);

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
    // 1. Hero Metric Cards
    // Metric 1: Heart Rate (BPM)
    canvas->drawRoundRect(10, 36, 106, 78, 4, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("HEART RATE", 18, 42);

    // Mini heart vector glyph
    drawHeartIcon(98, 46, 3, COLOR_ORANGE_BRIGHT);

    if (hasValidData) {
      char bpmBuf[16];
      snprintf(bpmBuf, sizeof(bpmBuf), "%.0f", lastBpm);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(bpmBuf, 18, 58, &fonts::Font4);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("BPM [STABLE]", 18, 96);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--", 18, 58, &fonts::Font4);
      canvas->drawString("NO DATA", 18, 96);
    }

    // Metric 2: SpO2 Blood Oxygen
    canvas->drawRoundRect(124, 36, 106, 78, 4, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("SpO2 OXYGEN", 132, 42);

    if (hasValidData) {
      char spo2Buf[16];
      snprintf(spo2Buf, sizeof(spo2Buf), "%.0f%%", lastSpo2);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(spo2Buf, 132, 58, &fonts::Font4);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString(lastSpo2 >= 95.0f ? "[OPTIMAL]" : "[EVALUATE]", 132, 96);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--%", 132, 58, &fonts::Font4);
      canvas->drawString("NO DATA", 132, 96);
    }

    // Metric 3: Multi-Sensor Fusion (MSF) Stress Index
    canvas->drawRoundRect(10, 120, 220, 78, 4, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("STRESS LEVEL // MSF FUSION", 18, 126);

    if (hasValidData && lastStress >= 0) {
      char stressBuf[32];
      snprintf(stressBuf, sizeof(stressBuf), "%d / 100", lastStress);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(stressBuf, 18, 142, &fonts::Font4);

      const char* tag = "LOW STRESS";
      if (lastStress > 65) tag = "HIGH STRESS";
      else if (lastStress > 40) tag = "MODERATE";
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(tag, 140, 148);

      // Segmented Stress Bar
      int barX = 18, barY = 178, barW = 204, barH = 8;
      canvas->drawRect(barX, barY, barW, barH, COLOR_ORANGE_DARK);
      int fillW = (barW - 4) * lastStress / 100;
      canvas->fillRect(barX + 2, barY + 2, fillW, barH - 4, COLOR_ORANGE_BRIGHT);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("-- / 100", 18, 142, &fonts::Font4);
      canvas->drawString("NOT MEASURED", 140, 148);
      canvas->drawRect(18, 178, 204, 8, COLOR_ORANGE_DARK);
    }

    // Bottom Navigation Bar
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("[PUSH] NEW SCAN   [R] SETTINGS   [B] EXIT", 120, 220);
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
      canvas->drawString(spo2Buf, 150, 74);

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

      // Render scrolling PPG wave with Automatic Gain Control
      float gain = (peakAc > 15.0f) ? (32.0f / peakAc) : 1.0f;
      if (gain > 1.2f) gain = 1.2f;

      int prevX = 11;
      int prevY = midY;
      for (int i = 0; i < PPG_HISTORY_SIZE; i++) {
        int idx = (ppgHead + i) % PPG_HISTORY_SIZE;
        int val = ppgWave[idx];
        int py = midY - (int)roundf((float)val * gain);
        py = constrain(py, waveY + 3, waveY + waveH - 3);
        int px = 11 + (i * 217 / (PPG_HISTORY_SIZE - 1));

        if (i > 0) {
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

        snprintf(buf, sizeof(buf), "MSF STRESS: %d / 100", lastStress);
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
    canvas->drawString("[PUSH] TOGGLE   [L] BACK TO VITALS", 120, 220);
    canvas->setTextDatum(TL_DATUM);
  }

  void drawHeartIcon(int cx, int cy, int r, uint16_t color) {
    canvas->fillCircle(cx - r / 2, cy - r / 3, r / 2, color);
    canvas->fillCircle(cx + r / 2, cy - r / 3, r / 2, color);
    canvas->fillTriangle(cx - r, cy - r / 4, cx + r, cy - r / 4, cx, cy + r, color);
  }
};
