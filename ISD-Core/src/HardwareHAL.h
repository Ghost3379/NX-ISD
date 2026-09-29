#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MAX1704X.h>
#include <ClosedCube_OPT3001.h>
#include <bme68xLibrary.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <Adafruit_BNO08x.h>
#include <RV-3028-C7.h>
#include "pins.h"
#include "SensorState.h"

class HardwareHAL {
public:
  static Adafruit_NeoPixel* neoPixels;
  static Adafruit_MAX17048 fuelGauge;
  static ClosedCube_OPT3001 lightSensor;
  static Bme68x envSensor;
  static MAX30105 heartRateSensor;
  static Adafruit_BNO08x imuSensor;
  static RV3028 rtcClock;

  static bool fuelGaugeReady;
  static bool lightSensorReady;
  static bool envSensorReady;
  static bool heartRateReady;
  static bool imuReady;
  static bool rtcReady;

  static void begin();
  static void setMatrixPower(bool on);
};
