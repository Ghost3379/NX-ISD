#include <Arduino.h>
#include <Wire.h>
#include "DisplayConfig.h"
#include "pins.h"
#include "SensorState.h"
#include "HAL.h"
#include "Watchface.h"

// Hardware and UI instances
LGFX tft;
Watchface watchface(&tft);
SensorState sharedState;
SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;

// Background Sensor & Input Task running on Core 0
void vSensorTask(void* pvParameters) {
  uint32_t lastSlowPoll = 0;
  bool lastBtn   = false;
  bool lastLeft  = false;
  bool lastPush  = false;
  bool lastRight = false;

  for (;;) {
    // 1. Fast polling of User Inputs (Active-LOW with INPUT_PULLUP)
    bool curBtn   = (digitalRead(BTN) == LOW);
    bool curLeft  = (digitalRead(LEVER_LEFT) == LOW);
    bool curPush  = (digitalRead(LEVER_PUSH) == LOW);
    bool curRight = (digitalRead(LEVER_RIGHT) == LOW);

    // Detect single clicks on press transition (Active-LOW: HIGH -> LOW)
    bool clickBtn   = (curBtn && !lastBtn);
    bool clickLeft  = (curLeft && !lastLeft);
    bool clickPush  = (curPush && !lastPush);
    bool clickRight = (curRight && !lastRight);

    if (clickBtn)   Serial.println("[NAV] BTN CLICK");
    if (clickLeft)  Serial.println("[NAV] LEVER LEFT CLICK");
    if (clickPush)  Serial.println("[NAV] LEVER PUSH CLICK");
    if (clickRight) Serial.println("[NAV] LEVER RIGHT CLICK");

    lastBtn   = curBtn;
    lastLeft  = curLeft;
    lastPush  = curPush;
    lastRight = curRight;

    // Power & charging detection (BQ25170 /PG on USB_DETECT and /STAT on BAT_STAT are open-drain, active-LOW)
    bool curUsb = (digitalRead(USB_DETECT) == LOW);
    bool curChg = (digitalRead(BAT_STAT) == LOW);

    // 2. IMU polling (if BNO085 is present and ready)
    float curRoll = 0.0f, curPitch = 0.0f, curYaw = 0.0f;
    bool imuOk = false;
    if (HAL::imuReady) {
      sh2_SensorValue_t sensorValue;
      if (HAL::imuSensor.getSensorEvent(&sensorValue)) {
        if (sensorValue.sensorId == SH2_ARVR_STABILIZED_RV) {
          float qr = sensorValue.un.arvrStabilizedRV.real;
          float qi = sensorValue.un.arvrStabilizedRV.i;
          float qj = sensorValue.un.arvrStabilizedRV.j;
          float qk = sensorValue.un.arvrStabilizedRV.k;
          float sqr = qr * qr;
          float sqi = qi * qi;
          float sqj = qj * qj;
          float sqk = qk * qk;
          curPitch = asin(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr)) * RAD_TO_DEG;
          curRoll  = atan2(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr)) * RAD_TO_DEG;
          curYaw   = atan2(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr)) * RAD_TO_DEG;
          imuOk = true;
        }
      }
    }

    // 3. Slow Sensor polling (every 1000ms: Fuel Gauge, RTC, BME680, OPT3001)
    uint32_t now = millis();
    bool doSlowPoll = (now - lastSlowPoll >= 1000);
    if (doSlowPoll) {
      lastSlowPoll = now;
    }

    // 4. Thread-safe state update
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.inputBtn = curBtn;
      sharedState.inputLeverLeft = curLeft;
      sharedState.inputLeverPush = curPush;
      sharedState.inputLeverRight = curRight;
      if (clickBtn)   sharedState.evtNavBtn = true;
      if (clickLeft)  sharedState.evtNavLeft = true;
      if (clickPush)  sharedState.evtNavPush = true;
      if (clickRight) sharedState.evtNavRight = true;
      sharedState.usbConnected = curUsb;
      sharedState.isCharging = curChg;

      if (imuOk) {
        sharedState.roll = curRoll;
        sharedState.pitch = curPitch;
        sharedState.yaw = curYaw;
        sharedState.imuDataReady = true;
      }

      if (doSlowPoll) {
        // Battery Fuel Gauge (MAX17048 with hardware I2C fallback)
        float v = 0.0f, p = 0.0f, r = 0.0f;
        if (HAL::readFuelGauge(v, p, r)) {
          sharedState.batVoltage = v;
          sharedState.batPercent = p;
          sharedState.batChangeRate = r;
        }

        static uint8_t logCounter = 0;
        if (++logCounter >= 2) {
          logCounter = 0;
          Serial.printf("[PWR] USB:%d CHG:%d | BAT: %.2fV (%.1f%%) | Rate: %.1f%%/h\n",
                        curUsb, curChg, sharedState.batVoltage, sharedState.batPercent,
                        sharedState.batChangeRate);
        }

        // RTC Real Time
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

        // BME680 Environmental (Temp, Humidity, Pressure, Gas)
        if (HAL::envSensorReady) {
          if (HAL::envSensor.fetchData()) {
            bme68xData data;
            if (HAL::envSensor.getData(data)) {
              sharedState.temp = data.temperature;
              sharedState.hum = data.humidity;
              sharedState.press = data.pressure / 100.0f; // Pa to hPa
              sharedState.gas = data.gas_resistance;
              sharedState.envDataReady = true;
            }
          }
          HAL::envSensor.setOpMode(BME68X_FORCED_MODE);
        }

        // Ambient Light
        if (HAL::lightSensorReady) {
          OPT3001 res = HAL::lightSensor.readResult();
          if (res.error == NO_ERROR) {
            sharedState.lightLux = res.lux;
          }
        }
      }

      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz poll rate
  }
}

// ==================== INITIALIZATION & BOOT ====================

void setup() {
  Serial.begin(115200);

  // 1. Initialize Hardware Pins & Buses (Completely Silent, Buzzer Muted)
  HAL::begin();

  // 2. Initialize ST7789 IPS Display
  tft.init();
  tft.setRotation(3);
  tft.setBrightness(220);
  tft.fillScreen(TFT_BLACK);

  // 3. Initialize Double-Buffered Watchface Sprite in PSRAM
  watchface.begin();

  // 4. Play Retro Cyberpunk / TVA Terminal Boot Animation
  watchface.playBootAnimation();

  // 5. Create FreeRTOS Mutexes
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  // 5. Pre-seed initial state so watchface displays immediately without delay
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

  // 6. Spawn Background Sensor Task pinned to Core 0 (PRO CPU)
  xTaskCreatePinnedToCore(
    vSensorTask,
    "SensorTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println("\n[NX-ISD] Watchface active. Double-buffering enabled. Audio muted.");
}

// ==================== MAIN UI LOOP (Core 1) ====================

void loop() {
  SensorState localState;
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

  // Handle panel navigation
  if (navLeft) {
    watchface.handleNavLeft();
  }
  if (navRight) {
    watchface.handleNavRight();
  }
  if (navBtn || navPush) {
    watchface.handleNavSelect();
  }

  // Render active view to offscreen PSRAM sprite and push cleanly to ST7789
  watchface.render(localState);

  // Max hardware SPI throughput during transitions, ~33 FPS when resting to save battery
  delay(watchface.isAnimating() ? 1 : 30);
}