#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "DisplayConfig.h"
#include "pins.h"
#include "SensorState.h"
#include "HAL.h"
#include "Watchface.h"
#include "AppMenu.h"
#include "apps/tools/UplinkBridge.h"
#include "apps/tools/AppTools.h"
#include "apps/vitals/AppVitals.h"
#include "apps/settings/AppSettings.h"
#include "nx-systems/NX-MSF.h"
#include "storage/StorageManager.h"

// Hardware and UI instances
LGFX tft;
Watchface watchface(&tft);
AppMenu appMenu(&tft);
AppTools appTools(&tft);
AppVitals appVitals(&tft);
AppSettings appSettings(&tft);

enum AppScreenMode {
  SCREEN_WATCHFACE,
  SCREEN_APPMENU,
  SCREEN_APP_TOOLS,
  SCREEN_APP_VITALS,
  SCREEN_APP_SETTINGS
};
AppScreenMode currentScreen = SCREEN_WATCHFACE;
static bool settingsFromQuickpanel = false;

SensorState sharedState;
SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;
TaskHandle_t sensorTaskHandle = NULL;

// Hardware Interrupt Event Flags & Physical State Latching for zero-latency, bounce-free control
volatile bool isrFlagBtn   = false;
volatile bool isrFlagLeft  = false;
volatile bool isrFlagPush  = false;
volatile bool isrFlagRight = false;

volatile bool activeBtn    = false;
volatile bool activeLeft   = false;
volatile bool activePush   = false;
volatile bool activeRight  = false;

volatile uint32_t isrTimeBtn   = 0;
volatile uint32_t isrTimeLeft  = 0;
volatile uint32_t isrTimePush  = 0;
volatile uint32_t isrTimeRight = 0;

void IRAM_ATTR isrBtn() {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if (!activeBtn && (now - isrTimeBtn > 40)) {
    activeBtn = true;
    isrFlagBtn = true;
    isrTimeBtn = now;
  }
}

void IRAM_ATTR isrLeverLeft() {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if (!activeLeft && (now - isrTimeLeft > 40)) {
    activeLeft = true;
    isrFlagLeft = true;
    isrTimeLeft = now;
  }
}

void IRAM_ATTR isrLeverPush() {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if (!activePush && (now - isrTimePush > 40)) {
    activePush = true;
    isrFlagPush = true;
    isrTimePush = now;
  }
}

void IRAM_ATTR isrLeverRight() {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
  if (!activeRight && (now - isrTimeRight > 40)) {
    activeRight = true;
    isrFlagRight = true;
    isrTimeRight = now;
  }
}

// Background Sensor Acquisition Task running on Core 0 (PRO CPU)
void vSensorTask(void* pvParameters) {
  uint32_t lastSlowPoll = 0;

  for (;;) {
    uint32_t now = millis();

    // 1. Power & charging detection (BQ25170 /PG on USB_DETECT and /STAT on BAT_STAT)
    bool curUsb = (digitalRead(USB_DETECT) == LOW);
    bool curChg = (digitalRead(BAT_STAT) == LOW);

    // 2. IMU polling (if BNO085 is present and ready)
    float curRoll = 0.0f, curPitch = 0.0f, curYaw = 0.0f;
    uint8_t curCalib = 0;
    bool imuOk = false;
    if (HAL::imuReady) {
      sh2_SensorValue_t sensorValue;
      while (HAL::imuSensor.getSensorEvent(&sensorValue)) {
        float qr = 1.0f, qi = 0.0f, qj = 0.0f, qk = 0.0f;
        bool validRv = false;

        if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
          qr = sensorValue.un.rotationVector.real;
          qi = sensorValue.un.rotationVector.i;
          qj = sensorValue.un.rotationVector.j;
          qk = sensorValue.un.rotationVector.k;
          curCalib = sensorValue.status & 0x03;
          validRv = true;
        } else if (sensorValue.sensorId == SH2_GEOMAGNETIC_ROTATION_VECTOR) {
          qr = sensorValue.un.geoMagRotationVector.real;
          qi = sensorValue.un.geoMagRotationVector.i;
          qj = sensorValue.un.geoMagRotationVector.j;
          qk = sensorValue.un.geoMagRotationVector.k;
          curCalib = sensorValue.status & 0x03;
          validRv = true;
        } else if (sensorValue.sensorId == SH2_ARVR_STABILIZED_RV) {
          qr = sensorValue.un.arvrStabilizedRV.real;
          qi = sensorValue.un.arvrStabilizedRV.i;
          qj = sensorValue.un.arvrStabilizedRV.j;
          qk = sensorValue.un.arvrStabilizedRV.k;
          curCalib = sensorValue.status & 0x03;
          validRv = true;
        }

        if (validRv) {
          float sqr = qr * qr;
          float sqi = qi * qi;
          float sqj = qj * qj;
          float sqk = qk * qk;
          float denom = sqi + sqj + sqk + sqr;
          if (denom > 0.0001f) {
            float sinP = constrain(-2.0f * (qi * qk - qj * qr) / denom, -1.0f, 1.0f);
            curPitch = asin(sinP) * RAD_TO_DEG;
            curRoll  = atan2(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr)) * RAD_TO_DEG;
            curYaw   = atan2(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr)) * RAD_TO_DEG;
            imuOk = true;
          }
        }
      }
    }

    // 2b. Fast Ambient Light Polling (every 100ms for real-time palm cover & auto-dim)
    static uint32_t lastLightPoll = 0;
    bool doLightPoll = (now - lastLightPoll >= 100);
    float fastLux = 0.0f;
    bool fastLightOk = false;
    if (doLightPoll) {
      lastLightPoll = now;
      if (HAL::lightSensorReady) {
        OPT3001 res = HAL::lightSensor.readResult();
        if (res.error == NO_ERROR) {
          fastLux = res.lux;
          fastLightOk = true;
        }
      }
    }

    // 3. Slow Sensor polling (every 1000ms: Fuel Gauge, RTC, BME680)
    // Executed OUTSIDE the mutex so stateMutex lock time is strictly < 1 microsecond!
    bool doSlowPoll = (now - lastSlowPoll >= 1000);
    float slowVolt = 0.0f, slowPct = 0.0f, slowRate = 0.0f;
    bool slowFuelOk = false;
    char slowTime[16] = "";
    char slowDate[16] = "";
    bool slowRtcOk = false;
    bme68xData slowEnvData;
    bool slowEnvOk = false;

    if (doSlowPoll) {
      lastSlowPoll = now;

      // Battery Fuel Gauge (MAX17048 with hardware I2C fallback)
      slowFuelOk = HAL::readFuelGauge(slowVolt, slowPct, slowRate);

      // RTC Real Time (RV-3028)
      if (HAL::rtcReady) {
        HAL::rtcClock.updateTime();
        snprintf(slowTime, sizeof(slowTime), "%02d:%02d:%02d",
                 HAL::rtcClock.getHours(),
                 HAL::rtcClock.getMinutes(),
                 HAL::rtcClock.getSeconds());
        snprintf(slowDate, sizeof(slowDate), "%04d-%02d-%02d",
                 HAL::rtcClock.getYear(),
                 HAL::rtcClock.getMonth(),
                 HAL::rtcClock.getDate());
        slowRtcOk = true;
      }

      // BME680 Environmental (Temp, Humidity, Pressure, Gas)
      if (HAL::envSensorReady) {
        if (HAL::envSensor.fetchData()) {
          HAL::envSensor.getData(slowEnvData);
          slowEnvOk = true;
        }
        HAL::envSensor.setOpMode(BME68X_FORCED_MODE);
      }
    }

    // 4. Thread-safe state update (< 1 microsecond memory copy)
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.usbConnected = curUsb;
      sharedState.isCharging = curChg;

      if (imuOk) {
        sharedState.roll = curRoll;
        sharedState.pitch = curPitch;
        sharedState.yaw = curYaw;
        sharedState.imuCalib = curCalib;
        sharedState.imuDataReady = true;
      }

      if (fastLightOk) {
        sharedState.lightLux = fastLux;
      }

      if (doSlowPoll) {
        if (slowFuelOk) {
          sharedState.batVoltage = slowVolt;
          sharedState.batPercent = slowPct;
          sharedState.batChangeRate = slowRate;
        }
        if (slowRtcOk) {
          memcpy(sharedState.rtcTime, slowTime, sizeof(sharedState.rtcTime));
          memcpy(sharedState.rtcDate, slowDate, sizeof(sharedState.rtcDate));
        }
        if (slowEnvOk) {
          sharedState.temp = slowEnvData.temperature;
          sharedState.hum = slowEnvData.humidity;
          sharedState.press = slowEnvData.pressure / 100.0f; // Pa to hPa
          sharedState.gas = slowEnvData.gas_resistance;
          sharedState.envDataReady = true;
        }
      }

      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 100 Hz sampling for IMU
  }
}

// ==================== INITIALIZATION & BOOT ====================

struct BootSync {
  volatile float hwProgress;
  volatile bool hwDone;
};
static BootSync bootSync = { 0.05f, false };

void vHardwareInitTask(void* pvParameters) {
  HAL::begin([](float progress) {
    bootSync.hwProgress = progress;
  });
  bootSync.hwDone = true;
  vTaskDelete(NULL);
}

void setup() {
  // Release and deinit any pins held during deep sleep
  rtc_gpio_hold_dis((gpio_num_t)BTN);
  rtc_gpio_hold_dis((gpio_num_t)LEVER_PUSH);
  rtc_gpio_deinit((gpio_num_t)BTN);
  rtc_gpio_deinit((gpio_num_t)LEVER_PUSH);
  gpio_deep_sleep_hold_dis();

  Serial.begin(115200);

  // Check if waking up from Deep Sleep via BTN (GPIO 13) or other trigger
  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
  if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT0 || 
      wakeup_cause == ESP_SLEEP_WAKEUP_EXT1 || 
      wakeup_cause == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("[PWR] Waking from Deep Sleep via User Input...");
  }

  // 1. Initialize Hardware Pins & Safety Gating (Muted buzzer, CS lines high, user inputs configured)
  HAL::initPins();

  // Attach zero-latency hardware interrupts for controls
  attachInterrupt(digitalPinToInterrupt(BTN), isrBtn, FALLING);
  attachInterrupt(digitalPinToInterrupt(LEVER_LEFT), isrLeverLeft, FALLING);
  attachInterrupt(digitalPinToInterrupt(LEVER_PUSH), isrLeverPush, FALLING);
  attachInterrupt(digitalPinToInterrupt(LEVER_RIGHT), isrLeverRight, FALLING);

  // 2. Initialize ST7789 IPS Display immediately
  tft.init();
  tft.setRotation(3);
  tft.setBrightness(220);
  tft.fillScreen(TFT_BLACK);

  // 3. Initialize Double-Buffered Watchface Sprite in PSRAM
  watchface.begin();

  // 3b. Initialize AppMenu sharing zero-copy PSRAM canvas and palette with Watchface
  appMenu.init(
    watchface.getCanvas(),
    watchface.getColorBg(),
    watchface.getColorOrangeBright(),
    watchface.getColorOrangeMid(),
    watchface.getColorOrangeDim(),
    watchface.getColorOrangeDark()
  );

  // 3c. Initialize AppTools sharing zero-copy PSRAM canvas and palette
  appTools.init(
    watchface.getCanvas(),
    watchface.getColorBg(),
    watchface.getColorOrangeBright(),
    watchface.getColorOrangeMid(),
    watchface.getColorOrangeDim(),
    watchface.getColorOrangeDark()
  );

  // 3d. Initialize AppVitals sharing zero-copy PSRAM canvas and palette
  appVitals.init(
    watchface.getCanvas(),
    watchface.getColorBg(),
    watchface.getColorOrangeBright(),
    watchface.getColorOrangeMid(),
    watchface.getColorOrangeDim(),
    watchface.getColorOrangeDark()
  );

  // 3e. Initialize AppSettings sharing zero-copy PSRAM canvas and palette
  appSettings.init(
    watchface.getCanvas(),
    watchface.getColorBg(),
    watchface.getColorOrangeBright(),
    watchface.getColorOrangeMid(),
    watchface.getColorOrangeDim(),
    watchface.getColorOrangeDark()
  );

  // 4. Render initial boot frame instantly
  watchface.renderBootFrame(0.02f);

  // 5. Launch Hardware Bring-up in Real Background Task on Core 0
  xTaskCreatePinnedToCore(
    vHardwareInitTask,
    "HwInitTask",
    6144,
    NULL,
    1,
    NULL,
    0
  );

  // 6. Play Smooth Boot Animation on Core 1 tracking real background hardware progress (~1.8s)
  watchface.playBootSequence(&bootSync.hwProgress, &bootSync.hwDone, 1750);

  // 5. Create FreeRTOS Mutexes
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  // 6. Pre-seed initial state so watchface displays immediately without delay
  sharedState.usbConnected = (digitalRead(USB_DETECT) == LOW);
  sharedState.isCharging = (digitalRead(BAT_STAT) == LOW);
  float initV = 0.0f, initP = 0.0f, initR = 0.0f;
  if (HAL::readFuelGauge(initV, initP, initR)) {
    sharedState.batVoltage = initV;
    sharedState.batPercent = initP;
    sharedState.batChangeRate = initR;
  }
  if (HAL::rtcReady) {
    HAL::rtcClock.updateTime();
    snprintf(sharedState.rtcTime, sizeof(sharedState.rtcTime), "%02d:%02d:%02d",
             HAL::rtcClock.getHours(),
             HAL::rtcClock.getMinutes(),
             HAL::rtcClock.getSeconds());
    snprintf(sharedState.rtcDate, sizeof(sharedState.rtcDate), "%04d-%02d-%02d",
             HAL::rtcClock.getYear(),
             HAL::rtcClock.getMonth(),
             HAL::rtcClock.getDate());
  }

  // 7. Spawn Background Sensor Task pinned to Core 0 (PRO CPU)
  xTaskCreatePinnedToCore(
    vSensorTask,
    "SensorTask",
    4096,
    NULL,
    1,
    &sensorTaskHandle,
    0
  );

  // 8. Initialize NX-Uplink Companion Bridge and NeoPixel Matrix
  UplinkBridge::begin();

  // 9. Initialize Storage Manager & load persistent configuration from ZDSD NAND
  StorageManager::begin(&tft);

  Serial.println("\n[NX-ISD] Watchface active. Double-buffering enabled. Audio muted.");
}

// Helper: Checks if the watch is held/tilted in an active viewing orientation
static inline bool isViewingAngle(float roll, float pitch, TiltMode mode) {
  float absR = fabsf(roll);
  float absP = fabsf(pitch);

  switch (mode) {
    case TILT_SENSITIVE:
      // Broad viewing window: 12° to 80° on either axis with orthogonal kept under 60°
      return ((absR >= 12.0f && absR <= 80.0f && absP <= 60.0f) ||
              (absP >= 12.0f && absP <= 80.0f && absR <= 60.0f));

    case TILT_BALANCED:
      // Natural wrist glance: 22° to 72° on either axis with orthogonal kept under 48°
      return ((absR >= 22.0f && absR <= 72.0f && absP <= 48.0f) ||
              (absP >= 22.0f && absP <= 72.0f && absR <= 48.0f));

    case TILT_SLUGGISH:
      // Steep deliberate glance: 35° to 65° with orthogonal strictly level (< 35°)
      return ((absR >= 35.0f && absR <= 65.0f && absP <= 35.0f) ||
              (absP >= 35.0f && absP <= 65.0f && absR <= 35.0f));

    case TILT_OFF:
    default:
      return false;
  }
}

// Helper: Checks if the watch is resting (arm hanging vertically down at the side)
static inline bool isRestingAngle(float roll, float pitch) {
  float absR = fabsf(roll);
  float absP = fabsf(pitch);
  // Arm hanging straight down (> 75° on primary axis)
  // Flat on desk (facing upwards) is active viewing, NEVER sleep!
  return (absR >= 75.0f) || (absP >= 75.0f);
}

// ==================== MAIN UI LOOP (Core 1) ====================

void loop() {
  static SensorState localState;
  static uint32_t leftHoldStart = 0;
  static uint32_t lastLeftRepeat = 0;
  static uint32_t rightHoldStart = 0;
  static uint32_t lastRightRepeat = 0;

  uint32_t now = millis();

  // Instant direct hardware pin reads
  bool curLeft  = (digitalRead(LEVER_LEFT) == LOW);
  bool curRight = (digitalRead(LEVER_RIGHT) == LOW);
  bool curPush  = (digitalRead(LEVER_PUSH) == LOW);
  bool curBtn   = (digitalRead(BTN) == LOW);

  // Release state reset: unlatch ISR only when the pin is physically HIGH (released)
  // and at least 50ms has elapsed since the press event (suppresses release bounce)
  if (!curLeft && (now - isrTimeLeft >= 50)) {
    activeLeft = false;
    leftHoldStart = 0;
  }
  if (!curRight && (now - isrTimeRight >= 50)) {
    activeRight = false;
    rightHoldStart = 0;
  }
  if (!curPush && (now - isrTimePush >= 50)) {
    activePush = false;
  }
  if (!curBtn && (now - isrTimeBtn >= 50)) {
    activeBtn = false;
  }

  bool navLeft  = false;
  bool navRight = false;
  bool navPush  = false;
  bool navBtn   = false;

  // 1. Consume hardware-interrupt clicks with hold-to-repeat
  if (isrFlagLeft) {
    isrFlagLeft = false;
    navLeft = true;
    leftHoldStart = now;
    lastLeftRepeat = now;
    Serial.println("[NAV] LEVER LEFT CLICK (ISR)");
  } else if (curLeft && (leftHoldStart > 0) && (now - leftHoldStart >= 475) && (now - lastLeftRepeat >= 160)) {
    navLeft = true;
    lastLeftRepeat = now;
  }

  if (isrFlagRight) {
    isrFlagRight = false;
    navRight = true;
    rightHoldStart = now;
    lastRightRepeat = now;
    Serial.println("[NAV] LEVER RIGHT CLICK (ISR)");
  } else if (curRight && (rightHoldStart > 0) && (now - rightHoldStart >= 475) && (now - lastRightRepeat >= 160)) {
    navRight = true;
    lastRightRepeat = now;
  }

  if (isrFlagPush) {
    isrFlagPush = false;
    navPush = true;
    Serial.println("[NAV] LEVER PUSH CLICK (ISR)");
  }

  if (isrFlagBtn) {
    isrFlagBtn = false;
    navBtn = true;
    Serial.println("[NAV] BTN CLICK (ISR)");
  }

  // 2. Fetch fresh sensor telemetry from thread-safe bridge (< 1 microsecond)
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    localState = sharedState;
    xSemaphoreGive(stateMutex);
  }

  // Pass real-time hardware hold state for compass charge gesture and active UI
  localState.inputLeverPush = curPush;
  localState.inputBtn = curBtn;
  localState.inputLeverLeft = curLeft;
  localState.inputLeverRight = curRight;

  // 1. Handle Standby (Display Off) State
  static bool prevUsb = false;
  static bool wasViewing = false;
  static uint32_t lastActivityMs = millis();
  static uint32_t wakeTimeMs = millis();
  static uint32_t tiltSleepStartMs = 0;
  static uint32_t palmCoverStartMs = 0;
  static float ambientLuxBaseline = 30.0f;

  if (watchface.isInStandby()) {
    bool usbPlugged = (localState.usbConnected && !prevUsb);
    prevUsb = localState.usbConnected;

    // Check Tilt to Wake gesture based on active profile
    bool tiltWake = false;
    if (HAL::tiltMode != TILT_OFF && localState.imuDataReady) {
      bool currentlyViewing = isViewingAngle(localState.roll, localState.pitch, HAL::tiltMode);
      bool currentlyResting = isRestingAngle(localState.roll, localState.pitch);
      if (currentlyResting) {
        wasViewing = false;
      }
      if (!wasViewing && currentlyViewing) {
        tiltWake = true;
        wasViewing = true;
      }
    }

    if (navBtn || navLeft || navPush || navRight || usbPlugged || tiltWake) {
      if (tiltWake) {
        Serial.println("[PWR] Waking from Standby via Tilt-to-Wake Gesture!");
      } else {
        Serial.println("[PWR] Waking from Standby via User Input...");
      }
      watchface.wakeFromStandby();
      currentScreen = SCREEN_WATCHFACE;
      lastActivityMs = millis();
      wakeTimeMs = millis();
      tiltSleepStartMs = 0;
      palmCoverStartMs = 0;
      // Swallow the wake input so it doesn't trigger unexpected screen actions
      navBtn = false;
      navLeft = false;
      navPush = false;
      navRight = false;
    } else {
      delay(40);
      return; // Skip rendering frames to save CPU and power
    }
  } else {
    prevUsb = localState.usbConnected;

    // A. Tilt Back to Sleep Check (when arm returns to resting or hanging down)
    if (HAL::tiltMode != TILT_OFF && localState.imuDataReady) {
      bool currentlyViewing = isViewingAngle(localState.roll, localState.pitch, HAL::tiltMode);
      bool currentlyResting = isRestingAngle(localState.roll, localState.pitch);

      if (currentlyViewing) {
        wasViewing = true;
        tiltSleepStartMs = 0;
      } else if (wasViewing && currentlyResting && (millis() - wakeTimeMs >= 1500)) {
        if (tiltSleepStartMs == 0) {
          tiltSleepStartMs = millis();
        } else if (millis() - tiltSleepStartMs >= 400) {
          Serial.println("[PWR] Tilt Back to Sleep -> Entering Standby");
          watchface.enterStandby();
          wasViewing = false;
          tiltSleepStartMs = 0;
          return;
        }
      } else {
        tiltSleepStartMs = 0;
      }
    }

    // B. Wrist Cover to Sleep Check (covering OPT3001 and screen with palm/wrist)
    if (HAL::wristCoverSleep) {
      if (localState.lightLux > 4.0f) {
        ambientLuxBaseline = (ambientLuxBaseline * 0.90f) + (localState.lightLux * 0.10f);
      }
      if (HAL::lightSensorReady && localState.lightLux <= 1.5f && ambientLuxBaseline >= 4.0f && (millis() - wakeTimeMs >= 700)) {
        if (palmCoverStartMs == 0) {
          palmCoverStartMs = millis();
        } else if (millis() - palmCoverStartMs >= 250) {
          Serial.println("[PWR] Wrist Cover to Sleep detected -> Entering Standby");
          HAL::buzzPip(2400, 12);
          watchface.enterStandby();
          palmCoverStartMs = 0;
          return;
        }
      } else {
        palmCoverStartMs = 0;
      }
    } else {
      palmCoverStartMs = 0;
    }

    // C. Inactivity Standby Timeout Check (0 = NEVER)
    if (navBtn || navLeft || navPush || navRight || curPush || curBtn || curLeft || curRight || localState.fingerDetected) {
      lastActivityMs = millis();
    } else if (HAL::screenTimeoutSec > 0) {
      if (millis() - lastActivityMs >= (uint32_t)HAL::screenTimeoutSec * 1000UL) {
        Serial.println("[PWR] Inactivity timeout reached -> Entering Standby");
        watchface.enterStandby();
        return;
      }
    }
  }

  // 1b. Update NX-Uplink Companion Bridge (serial telemetry & commands & LED matrix)
  UplinkBridge::update(localState);

  // 1c. Automatic Ambient Light Dimming (NX-MSF Logarithmic Weber-Fechner Curve)
  if (HAL::autoDimEnabled && localState.lightLux >= 0.0f) {
    static uint32_t lastAutoDimMs = 0;
    static float smoothedLux = 50.0f;
    if (millis() - lastAutoDimMs >= 150) {
      lastAutoDimMs = millis();
      int autoPct = NX_MSF::calculateAutoDim(localState.lightLux, smoothedLux);
      if (abs(autoPct - HAL::brightnessPercent) >= 2) {
        HAL::setBrightness(autoPct, &tft);
      }
    }
  }

  // 2. Dispatch UI by Current Screen
  if (currentScreen == SCREEN_WATCHFACE) {
    // Process Watchface Navigation
    if (navLeft) {
      watchface.handleNavLeft();
    }
    if (navRight) {
      watchface.handleNavRight();
    }
    if (navPush) {
      watchface.handleNavPush();
    }
    if (navBtn) {
      watchface.handleNavBack();
    }

    // Process Requested Power Actions from Quickpanel Power Menu
    PowerAction pwrAct = watchface.getRequestedPowerAction();
    if (pwrAct != PWR_ACT_NONE) {
      if (pwrAct == PWR_ACT_STANDBY) {
        Serial.println("[PWR] Entering Standby mode (Display off)...");
        watchface.enterStandby();
        return;
      } else if (pwrAct == PWR_ACT_RESTART) {
        Serial.println("[PWR] Restarting system...");
        watchface.renderPowerMessage("RESTARTING", "SYSTEM REBOOT IN PROGRESS", 2);
        delay(800);
        ESP.restart();
      } else if (pwrAct == PWR_ACT_SHUTDOWN) {
        Serial.println("[PWR] Shutdown disabled (pending dedicated hardware pull-up resistor).");
      }
    }

    // Render active watchface view to offscreen PSRAM sprite and push cleanly to ST7789
    watchface.render(localState);

    // Check if Quick Panel Settings tile was triggered
    if (watchface.checkAndClearSettingsTrigger()) {
      Serial.println("[NAV] Quick Panel Settings Triggered -> Launching Settings!");
      settingsFromQuickpanel = true;
      currentScreen = SCREEN_APP_SETTINGS;
      appSettings.onEnter();
      return;
    }

    // Check if lever hold triggered the App Menu entrance
    if (watchface.checkAndClearAppMenuTrigger()) {
      Serial.println("[NAV] 1.2s Lever Charge Complete -> Triggering Cyberpunk Transition into App Menu!");
      watchface.playCyberTransition(true, localState);
      appMenu.reset();
      currentScreen = SCREEN_APPMENU;
      return;
    }

    // Non-blocking yield: prevents CPU starvation while maintaining maximum responsiveness
    delay(1);

  } else if (currentScreen == SCREEN_APPMENU) {
    // App Menu Navigation
    if (navBtn) {
      // Exit App Menu back to Watchface Home
      Serial.println("[NAV] Exiting App Menu back to Home Watchface");
      watchface.playCyberTransition(false, localState);
      currentScreen = SCREEN_WATCHFACE;
      return;
    }
    if (navLeft) {
      appMenu.handleNavLeft();
    }
    if (navRight) {
      appMenu.handleNavRight();
    }
    if (navPush) {
      appMenu.handleNavPush();
    }

    // Check if an app was launched
    AppId launchApp;
    if (appMenu.checkAndClearLaunch(launchApp)) {
      if (launchApp == APP_TOOLS) {
        Serial.println("[NAV] Entering TOOLS App View!");
        currentScreen = SCREEN_APP_TOOLS;
        appTools.onEnter();
        return;
      } else if (launchApp == APP_VITALS) {
        Serial.println("[NAV] Entering VITALS App View!");
        currentScreen = SCREEN_APP_VITALS;
        appVitals.onEnter();
        return;
      } else if (launchApp == APP_SETTINGS) {
        Serial.println("[NAV] Entering SETTINGS App View!");
        settingsFromQuickpanel = false;
        currentScreen = SCREEN_APP_SETTINGS;
        appSettings.onEnter();
        return;
      }
    }

    // Update 3D carousel physics
    appMenu.update();

    // Render 3D app deck to offscreen PSRAM sprite and push cleanly to ST7789
    appMenu.render(localState);

    // Non-blocking yield: prevents CPU starvation while maintaining maximum responsiveness
    delay(1);

  } else if (currentScreen == SCREEN_APP_TOOLS) {
    // Tools Navigation
    if (navBtn) {
      // Exit Tools back to App Menu
      Serial.println("[NAV] Exiting TOOLS App back to App Menu");
      currentScreen = SCREEN_APPMENU;
      return;
    }
    if (navLeft) {
      appTools.handleNavLeft();
    }
    if (navRight) {
      appTools.handleNavRight();
    }
    if (navPush) {
      appTools.handleNavPush();
    }

    // Render Tools screen to offscreen PSRAM sprite and push cleanly to ST7789
    appTools.render(localState);

    // Non-blocking yield
    delay(1);

  } else if (currentScreen == SCREEN_APP_VITALS) {
    // Vitals Navigation
    if (navBtn) {
      if (!appVitals.handleNavBtn()) {
        // Exit Vitals back to App Menu
        Serial.println("[NAV] Exiting VITALS App back to App Menu");
        appVitals.onExit();
        currentScreen = SCREEN_APPMENU;
        return;
      }
    }
    if (navLeft) {
      appVitals.handleNavLeft();
    }
    if (navRight) {
      appVitals.handleNavRight();
    }
    if (navPush) {
      appVitals.handleNavPush();
    }

    // Render Vitals screen to offscreen PSRAM sprite and push cleanly to ST7789
    appVitals.render(localState);

    // Non-blocking yield
    delay(1);

  } else if (currentScreen == SCREEN_APP_SETTINGS) {
    // Settings Navigation
    if (navBtn) {
      if (!appSettings.handleNavBtn()) {
        if (settingsFromQuickpanel) {
          Serial.println("[NAV] Exiting SETTINGS App back to Home Watchface");
          settingsFromQuickpanel = false;
          watchface.setView(VIEW_HOME);
          currentScreen = SCREEN_WATCHFACE;
        } else {
          Serial.println("[NAV] Exiting SETTINGS App back to App Menu");
          currentScreen = SCREEN_APPMENU;
        }
        return;
      }
    }
    if (navLeft) {
      appSettings.handleNavLeft();
    }
    if (navRight) {
      appSettings.handleNavRight();
    }
    if (navPush) {
      appSettings.handleNavPush();
    }

    // Render Settings screen to offscreen PSRAM sprite and push cleanly to ST7789
    appSettings.render(localState);

    // Non-blocking yield
    delay(1);
  }
}