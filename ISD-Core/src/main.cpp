#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "TestSensors.h"
#include "TestPeripherals.h"
#include "DiagnosticMenu.h"
#include "SensorState.h"

// TFT display instance
TFT_eSPI tft = TFT_eSPI();

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

// ==================== FREERTOS TASKS ====================

// 1. UI Rendering & Input Task (Core 1, High Priority)
void vUITask(void *parameter) {
  diagMenu.begin();
  for (;;) {
    diagMenu.update();
    vTaskDelay(pdMS_TO_TICKS(20)); // Yield to run at ~50Hz
  }
}

// 2. High-Frequency Sensor Polling (Core 0, High Priority)
// Reads BNO085 IMU and MAX30102 PPG
void vFastSensorTask(void *parameter) {
  for (;;) {
    // Read IMU
    float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    bool imuOk = imuSensor.update(roll, pitch, yaw, ax, ay, az);

    // Read Pulse Sensor
    uint32_t red = 0, ir = 0;
    heartRate.getSample(red, ir);

    // Write to shared state safely under mutex lock
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      sharedState.bpmRed = red;
      sharedState.bpmIR = ir;
      if (imuOk) {
        sharedState.roll = roll;
        sharedState.pitch = pitch;
        sharedState.yaw = yaw;
        sharedState.ax = ax;
        sharedState.ay = ay;
        sharedState.az = az;
        sharedState.imuDataReady = true;
      }
      xSemaphoreGive(stateMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(20)); // Poll at 50Hz
  }
}

// 3. Low-Frequency Sensor Polling (Core 0, Low Priority)
// Reads BME680 Env, OPT3001 Light, and MAX17048 Fuel Gauge
void vSlowSensorTask(void *parameter) {
  for (;;) {
    // Read Fuel Gauge
    float volt = fuelGauge.getVoltage();
    float pct = fuelGauge.getPercent();
    float rate = fuelGauge.getChangeRate();

    // Read Light Sensor
    float lux = lightSensor.getLux();

    // Read Environmental Sensor (Forced mode takes time, but internal delay yields task CPU)
    float temp = 0.0f, hum = 0.0f, press = 0.0f, gas = 0.0f;
    bool envOk = envSensor.readData(temp, hum, press, gas);

    // Write to shared state safely under mutex lock
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.batVoltage = volt;
      sharedState.batPercent = pct;
      sharedState.batChangeRate = rate;
      sharedState.lightLux = lux;
      if (envOk) {
        sharedState.temp = temp;
        sharedState.hum = hum;
        sharedState.press = press;
        sharedState.gas = gas;
        sharedState.envDataReady = true;
      }
      xSemaphoreGive(stateMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Poll once per second
  }
}

// ==================== INITIALIZATION & BOOT ====================

void setup() {
  Serial.begin(115200);
  
  // Power Domains & Backlight
  pinMode(PWR_NPM, OUTPUT);
  digitalWrite(PWR_NPM, HIGH); // Enable NeoPixel power domain
  pinMode(TFT_PWM, OUTPUT);
  digitalWrite(TFT_PWM, HIGH); // Backlight HIGH
  
  // Initialize internal pullups for inputs
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LEVER_LEFT, INPUT_PULLUP);
  pinMode(LEVER_PUSH, INPUT_PULLUP);
  pinMode(LEVER_RIGHT, INPUT_PULLUP);
  pinMode(ALERT, INPUT_PULLUP);
  
  delay(100);

  // Initialize I2C and Display
  Wire.begin(I2C_SDA, I2C_SCL);
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(2);

  // Original Loading Animation
  const char spinner[] = {'|', '/', '-', '\\'};
  tft.setCursor(10, 10);
  tft.print("Loading ");
  int cursorX = tft.getCursorX();
  int cursorY = tft.getCursorY();

  for (int i = 0; i < 15; i++) {
    tft.setCursor(cursorX, cursorY);
    tft.print(spinner[i % 4]);
    delay(200);
  }

  // Original Self-Check Routine Style
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.println("Self-Check:\n");
  
  tft.setTextSize(1);
  
  // 1. Fuel Gauge (0x36)
  tft.print("Fuel (MAX17048) : ");
  if (fuelGauge.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 2. ALS (0x44)
  tft.print("ALS (OPT3001)   : ");
  if (lightSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 3. Env (0x76)
  tft.print("Env (BME690)    : ");
  if (envSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 4. BPM (0x57)
  tft.print("BPM (MAX30102)  : ");
  if (heartRate.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 5. RTC (0x52)
  tft.print("RTC (RV-3028)   : ");
  if (rtcClock.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 6. 9-DOF (0x4A)
  tft.print("9-DOF (BNO085)  : ");
  if (imuSensor.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // 7. NAND-SD Card (CS_SD=47)
  tft.print("NAND-SD Storage : ");
  if (sdCard.begin()) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.println("OK");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK); tft.println("FAIL");
  }
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  delay(150);

  // Initialize Buzzer & play happy sound
  buzzer.begin();
  buzzer.playStartupMelody();

  // Create state synchronization mutex
  stateMutex = xSemaphoreCreateMutex();
  if (stateMutex == NULL) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("MUTEX ERR - HALT");
    while(1);
  }

  // Prompt user to enter diagnostics menu
  tft.println("\n------------------------");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Self-Check Done.");
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.println("Press LEVER PUSH to enter");
  tft.println("Diagnostics Console...");

  // Wait for button press (Lever Push or main BTN)
  while (digitalRead(LEVER_PUSH) == HIGH && digitalRead(BTN) == HIGH) {
    delay(20);
  }
  
  buzzer.playSuccessBeep();

  // Spawn UI thread on Core 1 (APP CPU)
  xTaskCreatePinnedToCore(
    vUITask,      // Task function
    "UITask",     // Task name
    4096,         // Stack size
    NULL,         // Parameter
    2,            // Priority (High)
    NULL,         // Task handle
    1             // Pinned to Core 1
  );

  // Spawn Fast Sensor polling thread on Core 0 (PRO CPU)
  xTaskCreatePinnedToCore(
    vFastSensorTask,
    "FastSensorTask",
    4096,
    NULL,
    2,
    NULL,
    0             // Pinned to Core 0
  );

  // Spawn Slow Sensor polling thread on Core 0 (PRO CPU)
  xTaskCreatePinnedToCore(
    vSlowSensorTask,
    "SlowSensorTask",
    4096,
    NULL,
    1,
    NULL,
    0             // Pinned to Core 0
  );
}

void loop() {
  // Delete the loopTask (Core 1) to free memory, since UI and sensors run in custom tasks
  vTaskDelete(NULL);
}