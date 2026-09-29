#include <Arduino.h>
#include <Wire.h>
#include "DisplayConfig.h"
#include "pins.h"
#include "SensorState.h"
#include "HardwareHAL.h"
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

    // Serial telemetry logs for lever & button events
    if (curBtn != lastBtn) {
      if (curBtn) Serial.println("[INPUT] MAIN BTN PRESSED");
      lastBtn = curBtn;
    }
    if (curLeft != lastLeft) {
      if (curLeft) Serial.println("[INPUT] LEVER LEFT PRESSED");
      lastLeft = curLeft;
    }
    if (curPush != lastPush) {
      if (curPush) Serial.println("[INPUT] LEVER PUSH PRESSED");
      lastPush = curPush;
    }
    if (curRight != lastRight) {
      if (curRight) Serial.println("[INPUT] LEVER RIGHT PRESSED");
      lastRight = curRight;
    }

    // Power & charging detection
    bool curUsb = (digitalRead(USB_DETECT) == LOW);
    bool curChg = (digitalRead(BAT_STAT) == LOW);

    // 2. IMU polling (if BNO085 is present and ready)
    float curRoll = 0.0f, curPitch = 0.0f, curYaw = 0.0f;
    bool imuOk = false;
    if (HardwareHAL::imuReady) {
      sh2_SensorValue_t sensorValue;
      if (HardwareHAL::imuSensor.getSensorEvent(&sensorValue)) {
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
      sharedState.usbConnected = curUsb;
      sharedState.isCharging = curChg;

      if (imuOk) {
        sharedState.roll = curRoll;
        sharedState.pitch = curPitch;
        sharedState.yaw = curYaw;
        sharedState.imuDataReady = true;
      }

      if (doSlowPoll) {
        // Battery Fuel Gauge
        if (HardwareHAL::fuelGaugeReady) {
          sharedState.batVoltage = HardwareHAL::fuelGauge.cellVoltage();
          sharedState.batPercent = HardwareHAL::fuelGauge.cellPercent();
          sharedState.batChangeRate = HardwareHAL::fuelGauge.chargeRate();
        }

        // RTC Real Time
        if (HardwareHAL::rtcReady) {
          HardwareHAL::rtcClock.updateTime();
          snprintf(sharedState.rtcTime, sizeof(sharedState.rtcTime), "%02d:%02d:%02d",
                   HardwareHAL::rtcClock.getHours(),
                   HardwareHAL::rtcClock.getMinutes(),
                   HardwareHAL::rtcClock.getSeconds());
          snprintf(sharedState.rtcDate, sizeof(sharedState.rtcDate), "%04d-%02d-%02d",
                   HardwareHAL::rtcClock.getYear(),
                   HardwareHAL::rtcClock.getMonth(),
                   HardwareHAL::rtcClock.getDate());
        }

        // BME680 Environmental (Temp, Humidity, Pressure, Gas)
        if (HardwareHAL::envSensorReady) {
          if (HardwareHAL::envSensor.fetchData()) {
            bme68xData data;
            if (HardwareHAL::envSensor.getData(data)) {
              sharedState.temp = data.temperature;
              sharedState.hum = data.humidity;
              sharedState.press = data.pressure / 100.0f; // Pa to hPa
              sharedState.gas = data.gas_resistance;
              sharedState.envDataReady = true;
            }
          }
          HardwareHAL::envSensor.setOpMode(BME68X_FORCED_MODE);
        }

        // Ambient Light
        if (HardwareHAL::lightSensorReady) {
          OPT3001 res = HardwareHAL::lightSensor.readResult();
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
  HardwareHAL::begin();

  // 2. Initialize ST7789 IPS Display
  tft.init();
  tft.setRotation(3);
  tft.setBrightness(220);
  tft.fillScreen(TFT_BLACK);

  // 3. Initialize Double-Buffered Watchface Sprite in PSRAM
  watchface.begin();

  // 4. Create FreeRTOS Mutexes
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  // 5. Pre-seed initial state so watchface displays immediately without delay
  if (HardwareHAL::fuelGaugeReady) {
    sharedState.batVoltage = HardwareHAL::fuelGauge.cellVoltage();
    sharedState.batPercent = HardwareHAL::fuelGauge.cellPercent();
    sharedState.batChangeRate = HardwareHAL::fuelGauge.chargeRate();
  }
  if (HardwareHAL::rtcReady) {
    HardwareHAL::rtcClock.updateTime();
    snprintf(sharedState.rtcTime, sizeof(sharedState.rtcTime), "%02d:%02d:%02d",
             HardwareHAL::rtcClock.getHours(),
             HardwareHAL::rtcClock.getMinutes(),
             HardwareHAL::rtcClock.getSeconds());
    snprintf(sharedState.rtcDate, sizeof(sharedState.rtcDate), "%04d-%02d-%02d",
             HardwareHAL::rtcClock.getYear(),
             HardwareHAL::rtcClock.getMonth(),
             HardwareHAL::rtcClock.getDate());
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
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    localState = sharedState;
    xSemaphoreGive(stateMutex);
  }

  // Render to offscreen PSRAM sprite and push cleanly to ST7789 (Zero flicker)
  watchface.render(localState);

  delay(30); // ~33 FPS smooth display update
}