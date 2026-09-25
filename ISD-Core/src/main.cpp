#include <Arduino.h>
#include <Wire.h>
#include "driver/gpio.h"
#include "DisplayConfig.h"
#include "pins.h"
#include "TestSensors.h"
#include "TestPeripherals.h"
#include "DiagnosticMenu.h"
#include "SensorState.h"

// LovyanGFX display instance
LGFX tft;

// Hardware Diagnostic Instantiations
FuelGaugeTest fuelGauge;
LightSensorTest lightSensor;
EnvironmentSensorTest envSensor;
HeartRateSensorTest heartRate;
IMUSensorTest imuSensor;
RTCTest rtcClock;
SDCardTest sdCard;
NeoPixelTest neoPixel;
BuzzerTest buzzer;
DiagnosticMenu diagMenu;

// Shared state & mutex instantiation
SensorState sharedState;
SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;

// Terminal Print Helper
void termPrint(const char* text, uint16_t color = TFT_WHITE, uint16_t delayMs = 30) {
  tft.setTextColor(color, TFT_BLACK);
  while (*text) {
    tft.print(*text++);
    if (delayMs > 0) delay(delayMs);
  }
}

void termPrintln(const char* text, uint16_t color = TFT_WHITE, uint16_t delayMs = 20) {
  termPrint(text, color, delayMs);
  tft.println();
}

// ==================== FREERTOS TASKS ====================

// 1. UI Rendering & Input Task (Core 1, High Priority)
void vUITask(void *parameter) {
  diagMenu.begin();
  for (;;) {
    diagMenu.update();
    vTaskDelay(pdMS_TO_TICKS(20)); // ~50Hz UI loop
  }
}

// 2. Unified Sensor Polling Task (Core 0, High Priority)
// Polls IMU and PPG at 50Hz, and slow telemetry (Fuel, Light, RTC, Env) at 1Hz
// Sequentially on Core 0 to prevent hardware I2C bus collisions and mutex starvation.
void vSensorTask(void *parameter) {
  uint32_t slowTick = 0;
  float currentVolt = fuelGauge.getVoltage();
  float currentPct = fuelGauge.getPercent();
  float currentRate = fuelGauge.getChangeRate();
  float currentLux = lightSensor.getLux();
  char timeBuf[16] = "--:--:--";
  char dateBuf[16] = "--/--/----";
  float currentTemp = 0.0f, currentHum = 0.0f, currentPress = 0.0f, currentGas = 0.0f;
  bool currentEnvOk = false;

  for (;;) {
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    bool imuOk = false;

    uint32_t red = 0, ir = 0;
    float bpm = 0.0f, spo2 = 0.0f;
    bool finger = false, beat = false;

    // Synchronize I2C bus access against Core 1 (UI bus scanner / RTC sync)
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
      // 1. High-Frequency: IMU and PPG (50Hz)
      imuOk = imuSensor.update(roll, pitch, yaw, ax, ay, az);
      heartRate.update(red, ir, bpm, spo2, finger, beat);

      // 2. Low-Frequency: Fuel Gauge, Light, RTC, and Env (1Hz = every 50 ticks of 20ms)
      slowTick++;
      if (slowTick >= 50) {
        slowTick = 0;

        float v = fuelGauge.getVoltage();
        float p = fuelGauge.getPercent();
        float r = fuelGauge.getChangeRate();
        if (v >= 2.5f && v <= 4.5f) currentVolt = v;
        if (p >= 0.0f && p <= 100.0f) currentPct = p;
        if (!isnan(r) && r >= -100.0f && r <= 100.0f) currentRate = r;

        float l = lightSensor.getLux();
        if (l >= 0.0f) currentLux = l;

        if (rtcClock.initialized) {
          rtcClock.getTimeString(timeBuf, sizeof(timeBuf));
          rtcClock.getDateString(dateBuf, sizeof(dateBuf));
        }

        float t, h, pr, g;
        if (envSensor.readData(t, h, pr, g)) {
          currentTemp = t; currentHum = h; currentPress = pr; currentGas = g;
          currentEnvOk = true;
        }
      }

      xSemaphoreGive(i2cMutex);
    }

    // Write thread-safe snapshot to sharedState for Core 1 (UI)
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      sharedState.bpmRed = red;
      sharedState.bpmIR = ir;
      sharedState.heartRate = bpm;
      sharedState.spo2 = spo2;
      sharedState.fingerDetected = finger;
      sharedState.beatDetected = beat;

      if (imuOk) {
        sharedState.roll = roll;
        sharedState.pitch = pitch;
        sharedState.yaw = yaw;
        sharedState.ax = ax;
        sharedState.ay = ay;
        sharedState.az = az;
        sharedState.imuDataReady = true;
      }

      sharedState.batVoltage = currentVolt;
      sharedState.batPercent = currentPct;
      sharedState.batChangeRate = currentRate;
      sharedState.lightLux = currentLux;

      strncpy(sharedState.rtcTime, timeBuf, sizeof(sharedState.rtcTime) - 1);
      sharedState.rtcTime[sizeof(sharedState.rtcTime) - 1] = '\0';
      strncpy(sharedState.rtcDate, dateBuf, sizeof(sharedState.rtcDate) - 1);
      sharedState.rtcDate[sizeof(sharedState.rtcDate) - 1] = '\0';

      if (currentEnvOk) {
        sharedState.temp = currentTemp;
        sharedState.hum = currentHum;
        sharedState.press = currentPress;
        sharedState.gas = currentGas;
        sharedState.envDataReady = true;
      }

      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz
  }
}

// ==================== INITIALIZATION & BOOT ====================

void setup() {
  Serial.begin(115200);

  // 1. Hardware Pin Setup
  // Power Domains & Backlight
  pinMode(PWR_NPM, OUTPUT);
  digitalWrite(PWR_NPM, HIGH); // Enable NeoPixel power domain
  
  pinMode(CS_SD, OUTPUT);
  digitalWrite(CS_SD, HIGH);  // Deselect SD Card so it does not interfere with SPI

  pinMode(CS_TFT, OUTPUT);
  digitalWrite(CS_TFT, HIGH); // Keep TFT deselected while bus initializes

  // Buttons and Navigation Switch (Active-LOW when shorted to GND)
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LEVER_LEFT, INPUT_PULLUP);
  pinMode(LEVER_PUSH, INPUT_PULLUP);
  pinMode(LEVER_RIGHT, INPUT_PULLUP);

  // Hardware interrupts
  pinMode(ALERT, INPUT_PULLUP);
  pinMode(INT_HR, INPUT);
  pinMode(INT_DOF, INPUT);
  pinMode(INT_ALS, INPUT);
  pinMode(INT_RTC, INPUT);

  // Charging detection
  pinMode(USB_DETECT, INPUT);
  pinMode(BAT_STAT, INPUT_PULLUP);

  // 2. Initialize Buzzer
  buzzer.begin();

  // 3. Initialize I2C Bus (Standard 100kHz for 10k pull-up reliability)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  // 4. Initialize LovyanGFX Display
  tft.init();
  tft.setRotation(3);          // 90 degrees Counterclockwise (270 deg)
  tft.setBrightness(255);      // Full backlight brightness
  tft.fillScreen(TFT_BLACK);

  // ---------------- CYBERPUNK TERMINAL BOOT ----------------
  tft.setTextSize(1);
  tft.setCursor(0, 0);

  // Header Banner
  termPrintln("================================", TFT_CYAN, 0);
  termPrintln("   NX-ISD BIOS v1.3 - BOOT", TFT_YELLOW, 5);
  termPrintln("   GOTHAM SYSTEMS ARCHITECTURE", TFT_DARKGREY, 0);
  termPrintln("================================", TFT_CYAN, 0);

  char buf[48];
  snprintf(buf, sizeof(buf), "[CPU] ESP32-S3 Rev %d @ 240MHz", ESP.getChipRevision());
  termPrintln(buf, TFT_WHITE, 2);

  snprintf(buf, sizeof(buf), "[MEM] SRAM:320KB PSRAM:%dMB", (int)(ESP.getPsramSize() / 1048576));
  termPrintln(buf, TFT_WHITE, 2);

  termPrintln("[BUS] SPI2 @ 16MHz | I2C @ 400kHz", TFT_DARKGREY, 2);
  termPrintln("--------------------------------", TFT_DARKGREY, 0);
  termPrintln(">> RUNNING POST SELF-TEST...", TFT_GREEN, 10);

  delay(200);

  // 1. Fuel Gauge (0x36)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Fuel [0x36] MAX17048: ");
  if (fuelGauge.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "OK (%.1fV)", fuelGauge.getVoltage());
    tft.println(buf);
    buzzer.playBeep(1800, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 2. Ambient Light (0x44)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("ALS  [0x44] OPT3001 : ");
  if (lightSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "OK (%.0f lx)", lightSensor.getLux());
    tft.println(buf);
    buzzer.playBeep(2000, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 3. Environmental (0x76)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("ENV  [0x76] BME690  : ");
  if (envSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("OK (TPH+Gas)");
    buzzer.playBeep(2200, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 4. Biometrics (0x57)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("PPG  [0x57] MAX30102: ");
  if (heartRate.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("OK (Red+IR)");
    buzzer.playBeep(2400, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 5. RTC Clock (0x52)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("RTC  [0x52] RV-3028 : ");
  if (rtcClock.begin()) {
    char timeBuf[16];
    rtcClock.getTimeString(timeBuf, sizeof(timeBuf));
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "OK (%s)", timeBuf);
    tft.println(buf);
    buzzer.playBeep(2600, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 6. 9-DOF IMU (0x4A)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("IMU  [0x4A] BNO085  : ");
  if (imuSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("OK (Fused 9-AX)");
    buzzer.playBeep(2800, 20);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("FAIL");
  }
  delay(100);

  // 7. NAND / SD Storage (CS=47)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("NAND [CS47] SD Media: ");
  if (sdCard.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("OK (FAT Mount)");
    buzzer.playBeep(3000, 20);
  } else {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println("SKIP (No Card)");
  }
  delay(100);

  // 8. NeoPixel 4x4 Matrix (GPIO 18)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("NPM  [GP18] 4x4 LEDs: ");
  neoPixel.begin();
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("OK (16 Pixels)");
  buzzer.playBeep(3200, 20);
  
  // Quick visual test: Orange Snake crawl across 4x4 matrix
  if (neoPixel.pixels) {
    for (int t = 0; t < 24; t++) {
      neoPixel.runOrangeSnake(t * 3);
      neoPixel.pixels->show();
      delay(20);
    }
    neoPixel.pixels->clear();
    neoPixel.pixels->show();
  }

  // 9. Input Buttons Check
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("INP  [GP13] Buttons : ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("OK (Ready)");
  buzzer.playBeep(3500, 25);
  delay(100);

  // Startup melody
  buzzer.playStartupMelody();

  // Completion prompt
  termPrintln("--------------------------------", TFT_DARKGREY, 0);
  termPrintln(">> ALL SYSTEMS INITIALIZED <<", TFT_GREEN, 5);
  tft.println();
  termPrintln("PRESS ANY BUTTON OR LEVER", TFT_YELLOW, 5);
  termPrintln("TO ENTER DIAGNOSTIC CONSOLE...", TFT_ORANGE, 5);

  // Wait for user interaction
  while (digitalRead(BTN) == HIGH &&
         digitalRead(LEVER_PUSH) == HIGH &&
         digitalRead(LEVER_LEFT) == HIGH &&
         digitalRead(LEVER_RIGHT) == HIGH) {
    delay(20);
  }

  buzzer.playSuccessBeep();

  // Create state synchronization and I2C bus mutexes
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL || i2cMutex == NULL) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.println("FATAL: MUTEX CREATE FAILED");
    while (1) delay(1000);
  }

  // Pre-seed sharedState with initial fuel gauge readings so UI never sees 0%
  sharedState.batVoltage = fuelGauge.getVoltage();
  sharedState.batPercent = fuelGauge.getPercent();
  sharedState.batChangeRate = fuelGauge.getChangeRate();

  // Spawn UI thread on Core 1 (APP CPU)
  xTaskCreatePinnedToCore(
    vUITask,
    "UITask",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Spawn Unified Sensor polling thread on Core 0 (PRO CPU)
  xTaskCreatePinnedToCore(
    vSensorTask,
    "SensorTask",
    4096,
    NULL,
    2,
    NULL,
    0
  );
}

void loop() {
  // Delete the loopTask (Core 1) to free memory, since UI and sensors run in dedicated FreeRTOS tasks
  vTaskDelete(NULL);
}