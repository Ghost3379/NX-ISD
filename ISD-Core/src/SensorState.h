#pragma once
#include <Arduino.h>

struct SensorState {
  // Fuel Gauge (MAX17048)
  float batVoltage = 0.0f;
  float batPercent = 0.0f;
  float batChangeRate = 0.0f;

  // ALS (OPT3001)
  float lightLux = 0.0f;

  // Env (BME680)
  float temp = 0.0f;
  float hum = 0.0f;
  float press = 0.0f;
  float gas = 0.0f;
  bool envDataReady = false;

  // IMU (BNO085)
  float roll = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  bool imuDataReady = false;

  // BPM (MAX30102)
  uint32_t bpmRed = 0;
  uint32_t bpmIR = 0;
};

// Declared as extern; will be defined in main.cpp
extern SensorState sharedState;
extern SemaphoreHandle_t stateMutex;
