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

class LGFX;

enum NotificationMode {
  NOTIF_SILENT      = 0,
  NOTIF_ALL         = 1,
  NOTIF_SOUND_ONLY  = 2,
  NOTIF_LIGHTS_ONLY = 3
};

enum TiltMode {
  TILT_OFF       = 0,
  TILT_SENSITIVE = 1,
  TILT_BALANCED  = 2,
  TILT_SLUGGISH  = 3
};

class HAL {
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

  static NotificationMode notifMode;
  static bool silentMode;
  static bool lightsEnabled;
  static uint8_t buzzerVolumePercent;
  static uint8_t buzzerTickDurationMs;
  static uint16_t buzzerBaseFreqHz;
  static void setNotificationMode(NotificationMode mode);

  static int brightnessPercent;
  static bool autoDimEnabled;
  static TiltMode tiltMode;
  static int screenTimeoutSec;
  static bool wristCoverSleep;
  static void setBrightness(int pct, LGFX* display = nullptr);
  static void applyBrightness(LGFX* display);
  static uint16_t backlightFadeMs;
  static void fadeOutBacklight(LGFX* display);
  static void fadeInBacklight(LGFX* display);

  static bool matrixLedAlerts;
  static uint8_t hrmReminderIdx;
  static bool ecoMode;
  static bool autoStandby;
  static bool sensorSleep;
  static bool nxAisCoProc;
  static bool adaptiveSensing;
  static uint8_t npmBrightnessPercent;
  static uint8_t npmPatternIdx;

  static bool pinsInited;
  static void initPins();
  static void begin(void (*onProgress)(float progress) = nullptr);
  static void setMatrixPower(bool on);
  static void buzzPip(uint16_t freqHz = 3000, uint16_t durationMs = 15);
  static bool readFuelGauge(float &outVolt, float &outPct, float &outRate);
};
