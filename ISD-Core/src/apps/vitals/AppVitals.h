#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"
#include "../../nx-systems/NX-MSF.h"

enum VitalsViewMode {
  VITALS_VIEW_DASHBOARD = 0,
  VITALS_VIEW_SCAN,
  VITALS_VIEW_SETTINGS
};

enum VitalsScanState {
  SCAN_WAITING_FOR_FINGER = 0,
  SCAN_SETTLING,
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

  // Optical Contact Settling Grace Period (absorbs finger landing jump)
  uint32_t settlingStartTime = 0;
  static const uint32_t SETTLING_DURATION_MS = 1400; // 1.4s settling window

  // Live Optical Sampling & PPG DSP Pipeline (100 Hz continuous FIFO)
  static const int PPG_SWEEP_SIZE = 218; // Exact pixel width of oscilloscope viewport (11 to 228)
  float ppgSweep[PPG_SWEEP_SIZE];
  int sweepPos = 0;
  float smoothSpan = 120.0f;

  // Modern 32-bit Physiological PPG DSP & Hysteresis Contact Tracker
  float dcBaseline = 0.0f;
  float pulseLp1 = 0.0f;
  float pulseSmooth = 0.0f;
  float peakEnv = 100.0f;
  bool aboveThreshold = false;
  int samplesSinceLastBeat = 0;
  uint32_t lastBeatTimestamp = 0;
  float latestRawIr = 0.0f;
  float baselineDrift = 0.0f;
  float prevBaselineSample = 0.0f;
  int sampleCounter = 0;
  int contactPersistence = 0;
  int releasePersistence = 0;

  // Real-time touch feedback state
  enum TouchQuality {
    TOUCH_NO_FINGER = 0,
    TOUCH_OPTIMAL,
    TOUCH_TOO_HARD,
    TOUCH_TOO_LIGHT,
    TOUCH_UNSTABLE
  };
  TouchQuality touchQuality = TOUCH_NO_FINGER;

  // Robust Beat Rate Tracking & Plausibility Averaging
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
  int measureModeIndex = 0; // 0 = SPOT (15s), 1 = CONTINUOUS
  const char* measureModeLabels[2] = {"SPOT SCAN (15s)", "CONTINUOUS"};
  int reminderIndex = 0; // 0=OFF, 1=30m, 2=1h, 3=2h, 4=4h
  const char* reminderLabels[5] = {"OFF", "30 MIN", "1 HOUR", "2 HOURS", "4 HOURS"};
  int alertModeIndex = 0; // 0=AUDIO & LED, 1=LED ONLY, 2=AUDIO ONLY
  const char* alertLabels[3] = {"AUDIO & LED", "LED ONLY", "AUDIO ONLY"};
  int selectedSettingRow = 0; // 0, 1, or 2
  static const int NUM_SETTING_ROWS = 3;

  // Sensor state
  bool sensorAwake = false;

public:
  AppVitals(LGFX* tft) : display(tft) {
    memset(ppgSweep, 0, sizeof(ppgSweep));
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
      selectedSettingRow = (selectedSettingRow + NUM_SETTING_ROWS - 1) % NUM_SETTING_ROWS;
      HAL::buzzPip(2400, 12);
    }
  }

  void handleNavRight() {
    if (currentView == VITALS_VIEW_DASHBOARD) {
      currentView = VITALS_VIEW_SETTINGS;
      selectedSettingRow = 0;
      HAL::buzzPip(2800, 15);
    } else if (currentView == VITALS_VIEW_SETTINGS) {
      selectedSettingRow = (selectedSettingRow + 1) % NUM_SETTING_ROWS;
      HAL::buzzPip(2400, 12);
    }
  }

  void handleNavPush() {
    if (currentView == VITALS_VIEW_DASHBOARD) {
      // Start measurement session
      startScan();
    } else if (currentView == VITALS_VIEW_SCAN) {
      if (scanState == SCAN_FINISHED) {
        if (hasValidData) {
          // Acknowledge and return to dashboard
          currentView = VITALS_VIEW_DASHBOARD;
          stopSensor();
        } else {
          // Retry scan
          startScan();
        }
      } else if (measureModeIndex == 1 && scanState == SCAN_ACTIVE) {
        // In continuous mode, PUSH also saves & returns to dashboard!
        if (beatCount >= 3 && liveBpm >= 45.0f) {
          finalizeScan(sharedState);
        } else {
          stopSensor();
        }
        currentView = VITALS_VIEW_DASHBOARD;
        HAL::buzzPip(4400, 20);
      }
    } else if (currentView == VITALS_VIEW_SETTINGS) {
      // Toggle selected setting
      if (selectedSettingRow == 0) {
        measureModeIndex = (measureModeIndex + 1) % 2;
        HAL::buzzPip(3200, 15);
      } else if (selectedSettingRow == 1) {
        reminderIndex = (reminderIndex + 1) % 5;
        HAL::buzzPip(3200, 15);
      } else if (selectedSettingRow == 2) {
        alertModeIndex = (alertModeIndex + 1) % 3;
        HAL::buzzPip(3200, 15);
      }
    }
  }

  // Handle back button (return true if handled internally, false if should exit to AppMenu)
  bool handleNavBtn() {
    if (currentView == VITALS_VIEW_SCAN) {
      if (measureModeIndex == 1 && scanState == SCAN_ACTIVE && beatCount >= 3 && liveBpm >= 45.0f) {
        finalizeScan(sharedState);
      } else {
        stopSensor();
      }
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

  bool isMeasuring() const {
    return (currentView == VITALS_VIEW_SCAN && (scanState == SCAN_SETTLING || scanState == SCAN_ACTIVE));
  }

  void startScan() {
    currentView = VITALS_VIEW_SCAN;
    scanState = SCAN_WAITING_FOR_FINGER;
    settlingStartTime = 0;
    scanStartTime = 0;
    scanProgress = 0.0f;
    beatCount = 0;
    beatIntervalIndex = 0;
    rateSpot = 0;
    memset(rates, 0, sizeof(rates));
    memset(beatIntervals, 0, sizeof(beatIntervals));
    memset(ppgSweep, 0, sizeof(ppgSweep));
    sweepPos = 0;
    smoothSpan = 120.0f;
    liveBpm = 0.0f;
    liveSpo2 = 98.0f;
    redSum = 0;
    irSum = 0;
    sampleAccumCount = 0;
    dcBaseline = 0.0f;
    pulseLp1 = 0.0f;
    pulseSmooth = 0.0f;
    contactPersistence = 0;
    releasePersistence = 0;
    peakEnv = 60.0f;
    aboveThreshold = false;
    samplesSinceLastBeat = 0;
    latestRawIr = 0.0f;
    baselineDrift = 0.0f;
    prevBaselineSample = 0.0f;
    sampleCounter = 0;
    touchQuality = TOUCH_NO_FINGER;

    wakeSensor();
    HAL::buzzPip(3800, 25);
  }

  void stopSensor() {
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (sensorAwake && HAL::heartRateReady) {
        HAL::heartRateSensor.setPulseAmplitudeRed(0);
        HAL::heartRateSensor.setPulseAmplitudeIR(0);
        HAL::heartRateSensor.shutDown();
        sensorAwake = false;
      }
      xSemaphoreGive(i2cMutex);
    }
    if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.fingerDetected = false;
      sharedState.beatDetected = false;
      xSemaphoreGive(stateMutex);
    }
  }

  void wakeSensor() {
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (HAL::heartRateReady) {
        HAL::heartRateSensor.wakeUp();
        // Setup MAX30102:
        // powerLevel = 0x24 (7.2mA), sampleAverage = 4, ledMode = 2 (Red + IR),
        // sampleRate = 400 (100 Hz output), pulseWidth = 411 (18-bit), adcRange = 16384 (62.5 pA/LSB)
        HAL::heartRateSensor.setup(0x24, 4, 2, 400, 411, 16384);
        HAL::heartRateSensor.setPulseAmplitudeRed(0x24);
        HAL::heartRateSensor.setPulseAmplitudeIR(0x24);
        HAL::heartRateSensor.setPulseAmplitudeGreen(0);
        HAL::heartRateSensor.clearFIFO();
        sensorAwake = true;
      }
      xSemaphoreGive(i2cMutex);
    }
  }

  void processPpgSample(uint32_t ir, uint32_t red, const SensorState& state) {
    latestRawIr = (float)ir;

    // 1. Dual-Threshold Contact Hysteresis & Persistence Filter
    const uint32_t CONTACT_ENTER_IR = 15000;  // Reliable tissue contact threshold on MAX30102
    const uint32_t CONTACT_EXIT_IR  = 8000;   // Clean finger lift release threshold
    const uint32_t CONTACT_SAT_IR   = 255000; // 18-bit ADC saturation ceiling (max 262143)

    // Dynamic Touch & Pressure Quality Estimation
    sampleCounter++;
    if (sampleCounter % 10 == 0) {
      baselineDrift = fabsf(latestRawIr - prevBaselineSample);
      prevBaselineSample = latestRawIr;
    }

    if (ir < CONTACT_EXIT_IR) {
      touchQuality = TOUCH_NO_FINGER;
    } else if (ir >= CONTACT_SAT_IR) {
      touchQuality = TOUCH_TOO_HARD; // ADC near saturation
    } else if (ir < 22000) {
      touchQuality = TOUCH_TOO_LIGHT;
    } else if (baselineDrift > 4500.0f) {
      touchQuality = TOUCH_UNSTABLE;
    } else {
      touchQuality = TOUCH_OPTIMAL;
    }

    if (scanState == SCAN_WAITING_FOR_FINGER) {
      if (ir >= CONTACT_ENTER_IR && ir < CONTACT_SAT_IR) {
        contactPersistence++;
        releasePersistence = 0;
        if (contactPersistence >= 8) { // 80 ms of continuous contact required
          contactPersistence = 0;
          scanState = SCAN_SETTLING;
          settlingStartTime = millis();
          memset(ppgSweep, 0, sizeof(ppgSweep));
          sweepPos = 0;
          dcBaseline = (float)ir;
          pulseLp1 = 0.0f;
          pulseSmooth = 0.0f;
          rateSpot = 0;
          memset(rates, 0, sizeof(rates));
          lastBeatTimestamp = 0;
          liveBpm = 0.0f;
          redSum = 0;
          irSum = 0;
          sampleAccumCount = 0;
          peakEnv = 60.0f;
          aboveThreshold = false;
          samplesSinceLastBeat = 0;
          HAL::buzzPip(3200, 15); // Clean touch acknowledgment
        }
      } else {
        contactPersistence = 0;
      }
      return;
    }

    // While settling or active scan: check if finger lifted
    if (ir < CONTACT_EXIT_IR) {
      releasePersistence++;
      if (releasePersistence >= 10) { // 100 ms of continuous absence required
        releasePersistence = 0;
        contactPersistence = 0;
        scanState = SCAN_WAITING_FOR_FINGER;
        settlingStartTime = 0;
        scanStartTime = 0;
        scanProgress = 0.0f;
        memset(ppgSweep, 0, sizeof(ppgSweep));
        sweepPos = 0;
        pulseLp1 = 0.0f;
        pulseSmooth = 0.0f;
        return;
      }
    } else {
      releasePersistence = 0;
    }

    // 2. DC Baseline Tracking & High-Pass Filtering
    // Fast adaptation during settling, smooth clinical tracking during active scan (~0.4 Hz cutoff)
    if (dcBaseline == 0.0f) {
      dcBaseline = (float)ir;
    } else if (scanState == SCAN_SETTLING) {
      dcBaseline += 0.08f * ((float)ir - dcBaseline);
    } else {
      dcBaseline += 0.025f * ((float)ir - dcBaseline);
    }

    // 3. True Photoplethysmogram (PPG) Extraction (Systolic surge is positive)
    float rawPulse = (float)ir - dcBaseline;

    // 4. Cascaded 2-Pole Low-Pass Smoothing (~4.5 Hz cutoff at 100 Hz output)
    // Strips ambient optical noise, 50/60 Hz flicker & ADC fuzz
    pulseLp1 += 0.22f * (rawPulse - pulseLp1);
    pulseSmooth += 0.22f * (pulseLp1 - pulseSmooth);

    // 5. Store zero-centered organic pulse into CRT sweep buffer
    ppgSweep[sweepPos] = pulseSmooth;
    sweepPos = (sweepPos + 1) % PPG_SWEEP_SIZE;

    // 6. Dynamic Peak Envelope Follower & Adaptive Threshold
    samplesSinceLastBeat++;
    peakEnv *= 0.992f; // Smooth decay towards baseline (~850ms half-life)
    if (peakEnv < 40.0f) peakEnv = 40.0f; // Noise floor clamp
    if (pulseSmooth > peakEnv) {
      float maxRise = peakEnv + 50.0f; // Slew-rate limit to prevent artifact lockout
      peakEnv = (pulseSmooth < maxRise) ? pulseSmooth : maxRise;
    }
    float dynamicThreshold = peakEnv * 0.50f; // 50% peak height threshold

    // 7. Dynamic Schmitt-Trigger Beat Detector with Refractory Lockout
    bool beatDetected = false;
    uint32_t nowMs = millis();
    // Refractory period: at least 28 samples (280ms = max 214 BPM)
    if (!aboveThreshold && pulseSmooth > dynamicThreshold && samplesSinceLastBeat >= 28) {
      aboveThreshold = true;
      samplesSinceLastBeat = 0;
      beatDetected = true;
    } else if (aboveThreshold && pulseSmooth < (dynamicThreshold * 0.35f)) {
      aboveThreshold = false; // Hysteresis release
    }

    if (scanState == SCAN_SETTLING) {
      uint32_t settlingElapsed = nowMs - settlingStartTime;
      if (settlingElapsed >= SETTLING_DURATION_MS) {
        // Optical contact is now stable! Lock baseline and begin measurement
        scanState = SCAN_ACTIVE;
        scanStartTime = millis();
        scanProgress = 0.0f;
        beatCount = 0;
        beatIntervalIndex = 0;
        rateSpot = 0;
        memset(rates, 0, sizeof(rates));
        memset(beatIntervals, 0, sizeof(beatIntervals));
        liveBpm = 0.0f;
        redSum = 0;
        irSum = 0;
        sampleAccumCount = 0;
        lastBeatTimestamp = 0;
        peakEnv = 60.0f;
        aboveThreshold = false;

        HAL::buzzPip(4400, 20); // Acquisition start chime
      }
      return;
    }

    if (scanState == SCAN_ACTIVE) {
      if (measureModeIndex == 0) { // SPOT 15s session
        uint32_t elapsed = nowMs - scanStartTime;
        scanProgress = constrain((float)elapsed / (float)scanDurationMs, 0.0f, 1.0f);
      } else { // CONTINUOUS measurement mode
        scanProgress = 1.0f;
      }

      // 8. Beat Interval Timing & Missed-Beat/Outlier Recovery
      if (beatDetected) {
        if (lastBeatTimestamp > 0) {
          uint32_t delta = nowMs - lastBeatTimestamp;
          if (delta >= 280 && delta <= 1750) {
            float instantBpm = 60000.0f / (float)delta;
            bool acceptForBpm = true;

            if (liveBpm > 45.0f) {
              float expectedInterval = 60000.0f / liveBpm;

              // Dropped Beat Recovery: if delta is ~2x expected interval due to temporary motion
              if (delta >= (uint32_t)(expectedInterval * 1.60f) && delta <= (uint32_t)(expectedInterval * 2.45f)) {
                instantBpm = 60000.0f / ((float)delta / 2.0f);
                beatCount++; // Count recovered beat
              }
              // Wild Outlier Rejection: sudden single delta deviates by >45%
              else if (fabsf((float)delta - expectedInterval) > (expectedInterval * 0.45f)) {
                acceptForBpm = false;
              }
            }

            if (acceptForBpm && instantBpm >= 45.0f && instantBpm <= 195.0f) {
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
            }

            beatCount++;
            beatIntervals[beatIntervalIndex % MAX_BEAT_INTERVALS] = delta;
            beatIntervalIndex++;

            isBeating = true;
            beatAnimTimer = nowMs;

            if (alertModeIndex == 0 || alertModeIndex == 2) {
              HAL::buzzPip(3200, 6);
            }
          }
          lastBeatTimestamp = nowMs;
        } else {
          lastBeatTimestamp = nowMs;
        }
      }

      // 9. SpO2 Accumulation
      redSum += red;
      irSum += ir;
      sampleAccumCount++;

      // SpO2 update every 100 samples (1.0s at 100 Hz output rate)
      if (sampleAccumCount >= 100 && irSum > 0) {
        liveSpo2 = NX_MSF::estimateSpO2(redSum, irSum, sampleAccumCount);
        redSum = 0;
        irSum = 0;
        sampleAccumCount = 0;
      }

      // 10. Continuous Mode updates or Spot Session completion
      if (measureModeIndex == 1) { // CONTINUOUS MODE
        if (beatCount >= 3 && liveBpm >= 45.0f) {
          hasValidData = true;
          lastBpm = liveBpm;
          lastSpo2 = (liveSpo2 >= 94.0f && liveSpo2 <= 100.0f) ? liveSpo2 : 98.0f;
          lastStress = NX_MSF::calculateStress(beatIntervals, MAX_BEAT_INTERVALS, beatCount, beatIntervalIndex, lastBpm, 1.0f);
        }
      } else { // SPOT SCAN (15s)
        if (scanProgress >= 1.0f) {
          scanState = SCAN_FINISHED;
          finalizeScan(state);
          return;
        }
      }

      // Live thread-safe sync to shared state for companion uplink & watchface
      if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (hasValidData) {
          sharedState.heartRate = (liveBpm > 40.0f) ? liveBpm : lastBpm;
          sharedState.spo2 = (liveSpo2 >= 90.0f) ? liveSpo2 : lastSpo2;
        }
        sharedState.fingerDetected = (touchQuality != TOUCH_NO_FINGER);
        sharedState.beatDetected = isBeating;
        xSemaphoreGive(stateMutex);
      }
    }
  }

  void updateSampling(const SensorState& state) {
    if (currentView != VITALS_VIEW_SCAN || !sensorAwake || !HAL::heartRateReady) return;

    // Burst-read all waiting samples directly from MAX30102 hardware FIFO over thread-safe I2C
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      Wire.beginTransmission(0x57);
      Wire.write(0x04); // FIFO_WR_PTR
      if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)0x57, (uint8_t)3) == 3) {
        uint8_t wrPtr = Wire.read();
        Wire.read(); // ovfCounter (0x05)
        uint8_t rdPtr = Wire.read(); // FIFO_RD_PTR (0x06)

        int numSamples = (wrPtr - rdPtr) & 0x1F;
        while (numSamples > 0) {
          int samplesToRead = min(numSamples, 5); // Read in chunks of 5 samples (30 bytes)
          int bytesToRead = samplesToRead * 6;

          Wire.beginTransmission(0x57);
          Wire.write(0x07); // FIFO_DATA
          if (Wire.endTransmission() != 0) break;

          if (Wire.requestFrom((uint8_t)0x57, (uint8_t)bytesToRead) == bytesToRead) {
            for (int s = 0; s < samplesToRead; s++) {
              uint8_t r0 = Wire.read();
              uint8_t r1 = Wire.read();
              uint8_t r2 = Wire.read();
              uint8_t ir0 = Wire.read();
              uint8_t ir1 = Wire.read();
              uint8_t ir2 = Wire.read();

              uint32_t red = (((uint32_t)r0 << 16) | ((uint32_t)r1 << 8) | (uint32_t)r2) & 0x03FFFF;
              uint32_t ir  = (((uint32_t)ir0 << 16) | ((uint32_t)ir1 << 8) | (uint32_t)ir2) & 0x03FFFF;

              processPpgSample(ir, red, state);
            }
          } else {
            break;
          }
          numSamples -= samplesToRead;
        }
      }
      xSemaphoreGive(i2cMutex);
    }
  }

  void finalizeScan(const SensorState& state) {
    if (beatCount >= 3 && liveBpm >= 45.0f && liveBpm <= 195.0f) {
      hasValidData = true;
      lastBpm = liveBpm;
      lastSpo2 = (liveSpo2 >= 94.0f && liveSpo2 <= 100.0f) ? liveSpo2 : 98.0f;

      // Multi-Sensor Fusion (MSF) Stress Calculation via NX-MSF
      lastStress = NX_MSF::calculateStress(beatIntervals, MAX_BEAT_INTERVALS, beatCount, beatIntervalIndex, lastBpm, 1.0f);

      // Save to sharedState
      if (stateMutex && xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        sharedState.heartRate = lastBpm;
        sharedState.spo2 = lastSpo2;
        xSemaphoreGive(stateMutex);
      }

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
    if (measureModeIndex == 1) {
      canvas->drawString("[PUSH] CONT SCAN   [R] SETTINGS", 120, 220);
    } else {
      canvas->drawString("[PUSH] 15s SCAN    [R] SETTINGS", 120, 220);
    }
    canvas->setTextDatum(TL_DATUM);
  }

  void renderPpgWaveform(int waveY, int waveH, int midY) {
    // Dynamic Window Auto-Scaling around Zero Baseline
    float minVal = 0.0f;
    float maxVal = 0.0f;
    for (int i = 0; i < PPG_SWEEP_SIZE; i++) {
      float v = ppgSweep[i];
      if (v > maxVal) maxVal = v;
      if (v < minVal) minVal = v;
    }

    float currentP2P = maxVal - minVal;
    if (currentP2P < 60.0f) currentP2P = 60.0f;
    smoothSpan = (smoothSpan * 0.92f) + (currentP2P * 0.08f);
    float span = max(60.0f, smoothSpan);

    const int ERASE_GAP = 12; // 12-pixel CRT beam erase gap in front of sweep
    int beamX = 11 + sweepPos;
    int beamY = midY;

    int prevX = -1;
    int prevY = -1;

    // Available height above/below baseline to prevent ceiling clipping
    int availableHeight = min(midY - waveY - 4, waveY + waveH - midY - 4);

    for (int x = 0; x < PPG_SWEEP_SIZE; x++) {
      // Calculate distance in front of writing beam cursor
      int distInFront = x - sweepPos;
      if (distInFront < 0) distInFront += PPG_SWEEP_SIZE;

      // In the erase gap: blank out pixels so previous pass trace is cleanly erased
      if (distInFront < ERASE_GAP) {
        prevX = -1;
        continue;
      }

      float val = ppgSweep[x];
      int px = 11 + x;

      // Systolic peak (val > 0) arches UPWARDS (py < midY)
      int py = midY - (int)((val * (float)availableHeight) / (span * 0.55f));
      py = constrain(py, waveY + 2, waveY + waveH - 2);

      if (x == (sweepPos - 1 + PPG_SWEEP_SIZE) % PPG_SWEEP_SIZE) {
        beamY = py; // Capture current writing tip Y
      }

      if (prevX >= 0) {
        // Phosphor glow line
        canvas->drawLine(prevX, prevY + 1, px, py + 1, COLOR_ORANGE_MID);
        // Sharp primary pulse wave line
        canvas->drawLine(prevX, prevY, px, py, COLOR_ORANGE_BRIGHT);
      }
      prevX = px;
      prevY = py;
    }

    // Live CRT sweep scanner beam cursor
    canvas->drawFastVLine(beamX, waveY + 2, waveH - 4, COLOR_ORANGE_DARK);
    canvas->fillCircle(beamX, beamY, 2, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(beamX, beamY, 4, COLOR_ORANGE_MID);
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

    } else if (scanState == SCAN_SETTLING) {
      // Optical contact settling screen: absorbing finger landing jump
      canvas->setTextDatum(TL_DATUM);

      // 1. Settling Progress Bar at Top
      int barX = 14, barY = 36, barW = 212, barH = 6;
      canvas->drawRect(barX, barY, barW, barH, COLOR_ORANGE_DARK);
      float settleFrac = constrain((float)(millis() - settlingStartTime) / (float)SETTLING_DURATION_MS, 0.0f, 1.0f);
      int fillW = (int)((barW - 2) * settleFrac);
      canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_ORANGE_MID);

      // Settling Status Labels
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString("STABILIZING CONTACT...", 14, 46, &fonts::Font2);

      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString("HOLD FINGER STEADY", 14, 66);
      canvas->drawRightString("LOCKING AGC", 220, 66);

      // 2. Real-Time PPG Waveform Oscilloscope Strip during settling
      int waveY = 88;
      int waveH = 100;
      canvas->drawRect(10, waveY, 220, waveH, COLOR_ORANGE_DIM);
      int midY = waveY + (waveH / 2);
      canvas->drawFastHLine(11, midY, 218, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 55, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 110, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 165, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);

      renderPpgWaveform(waveY, waveH, midY);

      // Bottom prompt with live touch guidance
      canvas->setTextDatum(MC_DATUM);
      if (touchQuality == TOUCH_TOO_HARD) {
        canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
        canvas->drawString("! TOO FIRM: RELAX PRESSURE !", 120, 206);
      } else if (touchQuality == TOUCH_TOO_LIGHT) {
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString("TOUCH TOO LIGHT: REST FINGER", 120, 206);
      } else if (touchQuality == TOUCH_UNSTABLE) {
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString("MOTION DETECTED: HOLD STEADY", 120, 206);
      } else {
        canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
        canvas->drawString("STABILIZING OPTICAL BASELINE...", 120, 206);
      }
      canvas->setTextDatum(TL_DATUM);

    } else if (scanState == SCAN_ACTIVE) {
      // Full-Screen Live Telemetry Scanning Screen
      canvas->setTextDatum(TL_DATUM);

      // 1. Live Countdown Bar / Continuous Pulse Indicator at Top
      int barX = 14, barY = 36, barW = 212, barH = 6;
      canvas->drawRect(barX, barY, barW, barH, COLOR_ORANGE_DARK);

      if (measureModeIndex == 0) { // SPOT 15s session
        int fillW = (int)((barW - 2) * scanProgress);
        canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_ORANGE_BRIGHT);

        int secRemaining = (int)ceilf((1.0f - scanProgress) * (scanDurationMs / 1000.0f));
        char timeBuf[24];
        snprintf(timeBuf, sizeof(timeBuf), "CALIBRATING: %ds", secRemaining);
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString(timeBuf, 14, 46);
      } else { // CONTINUOUS MODE
        int sweepW = 36;
        int sweepRange = barW - 2 - sweepW;
        int sweepX = barX + 1 + (int)(fmodf((float)millis() * 0.12f, (float)sweepRange));
        canvas->fillRect(sweepX, barY + 1, sweepW, barH - 2, COLOR_ORANGE_BRIGHT);

        canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
        canvas->drawString("MODE: CONTINUOUS", 14, 46);
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawRightString("LIVE SCAN", 226, 46);
      }

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
      int midY = waveY + (waveH / 2);
      canvas->drawFastHLine(11, midY, 218, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 55, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 110, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);
      canvas->drawFastVLine(10 + 165, waveY + 1, waveH - 2, COLOR_ORANGE_DARK);

      renderPpgWaveform(waveY, waveH, midY);

      // Bottom scan prompt with live touch guidance
      canvas->setTextDatum(MC_DATUM);
      if (touchQuality == TOUCH_TOO_HARD) {
        canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
        canvas->drawString("! TOO FIRM: RELAX PRESSURE !", 120, 206);
      } else if (touchQuality == TOUCH_TOO_LIGHT) {
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString("TOUCH TOO LIGHT: REST FINGER", 120, 206);
      } else if (touchQuality == TOUCH_UNSTABLE) {
        canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas->drawString("MOTION DETECTED: HOLD STEADY", 120, 206);
      } else {
        canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
        if (measureModeIndex == 1) {
          canvas->drawString("[BTN] STOP & SAVE // [PUSH] DONE", 120, 206);
        } else {
          canvas->drawString("KEEP STILL // [BTN] CANCEL", 120, 206);
        }
      }
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
        canvas->drawString("Fewer than 4 clear beats locked", 120, 124);
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
    canvas->drawString("VITALS SETTINGS", 14, 34, &fonts::Font2);

    // Setting 0: Measurement Mode (Spot 15s vs Continuous)
    bool row0 = (selectedSettingRow == 0);
    canvas->drawRoundRect(10, 56, 220, 44, 4, row0 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("MEASUREMENT MODE", 18, 62);
    canvas->setTextColor(row0 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(measureModeLabels[measureModeIndex], 18, 76, &fonts::Font2);

    // Setting 1: Reminder Interval
    bool row1 = (selectedSettingRow == 1);
    canvas->drawRoundRect(10, 106, 220, 44, 4, row1 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("HRM REMINDER", 18, 112);
    canvas->setTextColor(row1 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(reminderLabels[reminderIndex], 18, 126, &fonts::Font2);

    // Setting 2: Alert Mode
    bool row2 = (selectedSettingRow == 2);
    canvas->drawRoundRect(10, 156, 220, 44, 4, row2 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("NOTIFICATION DISPATCH", 18, 162);
    canvas->setTextColor(row2 ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(alertLabels[alertModeIndex], 18, 176, &fonts::Font2);

    // Bottom Navigation Context
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("[L/R] SELECT  [PUSH] TOGGLE  [BTN] BACK", 120, 220);
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
