#include <Arduino.h>
#include <Wire.h>
#include "DisplayConfig.h"
#include "pins.h"
#include "SensorState.h"
#include "HardwareHAL.h"

// Hardware instances
LGFX tft;
SensorState sharedState;
SemaphoreHandle_t stateMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL;

void setup() {
  Serial.begin(115200);

  // 1. Initialize Hardware Pins & Buses (Completely Silent)
  HardwareHAL::begin();

  // 2. Initialize ST7789 IPS Display to simple clean black
  tft.init();
  tft.setRotation(3);
  tft.setBrightness(180);
  tft.fillScreen(TFT_BLACK);

  // 3. Create FreeRTOS Mutexes
  stateMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  Serial.println("\n[NX-ISD] Base hardware initialized. Audio muted. Standby.");
}

void loop() {
  // Empty loop in standby while clarifying requirements
  delay(100);
}