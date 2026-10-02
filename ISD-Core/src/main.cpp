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

// Hardware and UI instances
LGFX tft;
Watchface watchface(&tft);
AppMenu appMenu(&tft);

enum AppScreenMode {
  SCREEN_WATCHFACE,
  SCREEN_APPMENU
};
AppScreenMode currentScreen = SCREEN_WATCHFACE;

SensorState sharedState;
SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;
TaskHandle_t sensorTaskHandle = NULL;

// Background Sensor & Input Task running on Core 0
void vSensorTask(void* pvParameters) {
  uint32_t lastSlowPoll = 0;
  bool lastBtn   = false;
  bool lastLeft  = false;
  bool lastPush  = false;
  bool lastRight = false;

  uint32_t leftHoldStart = 0;
  uint32_t lastLeftRepeat = 0;
  uint32_t rightHoldStart = 0;
  uint32_t lastRightRepeat = 0;

  for (;;) {
    uint32_t now = millis();

    // 1. Fast polling of User Inputs (Active-LOW with INPUT_PULLUP)
    bool curBtn   = (digitalRead(BTN) == LOW);
    bool curLeft  = (digitalRead(LEVER_LEFT) == LOW);
    bool curPush  = (digitalRead(LEVER_PUSH) == LOW);
    bool curRight = (digitalRead(LEVER_RIGHT) == LOW);

    // Detect single clicks on press transition (Active-LOW: HIGH -> LOW)
    bool clickBtn  = (curBtn && !lastBtn);
    bool clickPush = (curPush && !lastPush);

    // Left lever hold-to-repeat: immediate initial click, 350ms hold delay, 80ms repeat
    bool trigLeft = false;
    if (curLeft) {
      if (!lastLeft) {
        trigLeft = true;
        leftHoldStart = now;
        lastLeftRepeat = now;
        Serial.println("[NAV] LEVER LEFT CLICK");
      } else if ((now - leftHoldStart >= 350) && (now - lastLeftRepeat >= 80)) {
        trigLeft = true;
        lastLeftRepeat = now;
      }
    }

    // Right lever hold-to-repeat: immediate initial click, 350ms hold delay, 80ms repeat
    bool trigRight = false;
    if (curRight) {
      if (!lastRight) {
        trigRight = true;
        rightHoldStart = now;
        lastRightRepeat = now;
        Serial.println("[NAV] LEVER RIGHT CLICK");
      } else if ((now - rightHoldStart >= 350) && (now - lastRightRepeat >= 80)) {
        trigRight = true;
        lastRightRepeat = now;
      }
    }

    if (clickBtn)  Serial.println("[NAV] BTN CLICK");
    if (clickPush) Serial.println("[NAV] LEVER PUSH CLICK");

    lastBtn   = curBtn;
    lastLeft  = curLeft;
    lastPush  = curPush;
    lastRight = curRight;

    // Power & charging detection (BQ25170 /PG on USB_DETECT and /STAT on BAT_STAT are open-drain, active-LOW)
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

    // 3. Slow Sensor polling (every 1000ms: Fuel Gauge, RTC, BME680, OPT3001)
    // Executed OUTSIDE the mutex so stateMutex lock time is strictly < 1 microsecond!
    bool doSlowPoll = (now - lastSlowPoll >= 1000);
    float slowVolt = 0.0f, slowPct = 0.0f, slowRate = 0.0f;
    bool slowFuelOk = false;
    char slowTime[16] = "";
    char slowDate[16] = "";
    bool slowRtcOk = false;
    bme68xData slowEnvData;
    bool slowEnvOk = false;
    float slowLux = 0.0f;
    bool slowLightOk = false;

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

      // Ambient Light (OPT3001)
      if (HAL::lightSensorReady) {
        OPT3001 res = HAL::lightSensor.readResult();
        if (res.error == NO_ERROR) {
          slowLux = res.lux;
          slowLightOk = true;
        }
      }

      static uint8_t logCounter = 0;
      if (++logCounter >= 2) {
        logCounter = 0;
        Serial.printf("[PWR] USB:%d CHG:%d | BAT: %.2fV (%.1f%%) | Rate: %.1f%%/h\n",
                      curUsb, curChg, slowVolt, slowPct, slowRate);
        Serial.printf("[IMU] Yaw: %.1f deg | Roll: %.1f | Pitch: %.1f | Calib: %d/3\n",
                      curYaw, curRoll, curPitch, curCalib);
        if (slowEnvOk) {
          Serial.printf("[ENV] T: %.1f C | H: %.0f %% | P: %.0f hPa\n",
                        slowEnvData.temperature, slowEnvData.humidity, slowEnvData.pressure / 100.0f);
        }
      }
    }

    // 4. Thread-safe state update (< 1 microsecond memory copy)
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.inputBtn = curBtn;
      sharedState.inputLeverLeft = curLeft;
      sharedState.inputLeverPush = curPush;
      sharedState.inputLeverRight = curRight;
      if (clickBtn)  sharedState.evtNavBtn = true;
      if (trigLeft)  sharedState.evtNavLeft = true;
      if (clickPush) sharedState.evtNavPush = true;
      if (trigRight) sharedState.evtNavRight = true;
      sharedState.usbConnected = curUsb;
      sharedState.isCharging = curChg;

      if (imuOk) {
        sharedState.roll = curRoll;
        sharedState.pitch = curPitch;
        sharedState.yaw = curYaw;
        sharedState.imuCalib = curCalib;
        sharedState.imuDataReady = true;
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
        if (slowLightOk) {
          sharedState.lightLux = slowLux;
        }
      }

      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz poll rate
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

  Serial.println("\n[NX-ISD] Watchface active. Double-buffering enabled. Audio muted.");
}

// ==================== MAIN UI LOOP (Core 1) ====================

void loop() {
  static SensorState localState;
  bool navLeft  = false;
  bool navRight = false;
  bool navPush  = false;
  bool navBtn   = false;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    localState = sharedState;
    navLeft  = sharedState.evtNavLeft;
    navRight = sharedState.evtNavRight;
    navPush  = sharedState.evtNavPush;
    navBtn   = sharedState.evtNavBtn;
    sharedState.evtNavLeft  = false;
    sharedState.evtNavRight = false;
    sharedState.evtNavPush  = false;
    sharedState.evtNavBtn   = false;
    xSemaphoreGive(stateMutex);
  }

  // 1. Handle Standby (Display Off) State
  static bool prevUsb = false;
  if (watchface.isInStandby()) {
    bool usbPlugged = (localState.usbConnected && !prevUsb);
    prevUsb = localState.usbConnected;
    if (navBtn || navLeft || navPush || navRight || usbPlugged) {
      Serial.println("[PWR] Waking from Standby via User Input...");
      watchface.wakeFromStandby();
      // Swallow the wake input so it doesn't trigger unexpected screen actions
      navBtn = false;
      navLeft = false;
      navPush = false;
      navRight = false;
    } else {
      delay(50);
      return; // Skip rendering frames to save CPU and power
    }
  } else {
    prevUsb = localState.usbConnected;
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

    // Check if lever hold triggered the App Menu entrance
    if (watchface.checkAndClearAppMenuTrigger()) {
      Serial.println("[NAV] 1.2s Lever Charge Complete -> Triggering Cyberpunk Transition into App Menu!");
      watchface.playCyberTransition(true, localState);
      appMenu.reset();
      currentScreen = SCREEN_APPMENU;
      return;
    }

    // Max hardware SPI throughput during transitions, ~33 FPS when resting to save battery
    delay(watchface.isAnimating() ? 1 : 30);

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

    // Update 3D carousel physics
    appMenu.update();

    // Render 3D app deck to offscreen PSRAM sprite and push cleanly to ST7789
    appMenu.render(localState);

    // Dynamic framerate: 50+ FPS during 3D carousel sliding, 33 FPS at rest
    delay(appMenu.isAnimating() ? 1 : 30);
  }
}