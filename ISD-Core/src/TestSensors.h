#pragma once
#include <Arduino.h>
#include <Wire.h>

// Libraries
#include <Adafruit_MAX1704X.h>
#include <ClosedCube_OPT3001.h>
#include <bme68xLibrary.h>
#include <MAX30105.h>
#include <heartRate.h>
#include <Adafruit_BNO08x.h>

class FuelGaugeTest {
public:
  Adafruit_MAX17048 sensor;
  bool initialized = false;
  float lastValidVoltage = 3.85f;
  float lastValidPercent = 80.0f;
  float lastValidChangeRate = 0.0f;
  bool hasValidReading = false;

  bool begin() {
    initialized = sensor.begin(&Wire);
    if (initialized) {
      // MAX17048 requires 125ms after reset/quickStart for first ADC conversion
      delay(150);
      float v = sensor.cellVoltage();
      float p = sensor.cellPercent();
      if (!isnan(v) && !isinf(v) && v >= 2.5f && v <= 4.5f) {
        lastValidVoltage = v;
        hasValidReading = true;
      }
      if (!isnan(p) && !isinf(p) && p >= 0.0f && p <= 100.0f) {
        lastValidPercent = p;
      }
    }
    return initialized;
  }

  float getVoltage() {
    if (!initialized) return lastValidVoltage;
    float v = sensor.cellVoltage();
    // 0xFFFFFFFF * 78.125 / 1000000 = ~335544.3V on failed I2C read
    if (isnan(v) || isinf(v) || v < 2.5f || v > 4.5f) {
      return lastValidVoltage;
    }
    lastValidVoltage = v;
    hasValidReading = true;
    return v;
  }

  float getPercent() {
    if (!initialized) return lastValidPercent;
    float p = sensor.cellPercent();
    // 0xFFFFFFFF / 256.0f = 16777216.0f on failed I2C read (-1 from Adafruit BusIO)
    // Filter out 16777216%, NAN, Inf, negative values, and values over 100%
    if (isnan(p) || isinf(p) || p < 0.0f || p > 105.0f || p >= 16000000.0f) {
      return lastValidPercent;
    }
    if (p > 100.0f) p = 100.0f;
    lastValidPercent = p;
    hasValidReading = true;
    return p;
  }

  float getChangeRate() {
    if (!initialized) return lastValidChangeRate;
    float r = sensor.chargeRate();
    if (isnan(r) || isinf(r) || r < -100.0f || r > 100.0f || r >= 1000.0f) {
      return lastValidChangeRate;
    }
    lastValidChangeRate = r;
    return r;
  }
};

class LightSensorTest {
public:
  ClosedCube_OPT3001 sensor;
  bool initialized = false;

  bool begin() {
    sensor.begin(0x44); // OPT3001 default I2C address
    OPT3001_Config config;
    config.RangeNumber = 0b1100; // Automatic full-scale range
    config.ConvertionTime = 0b1; // 800ms
    config.Latch = 0b1;          // Latched window
    config.ModeOfConversionOperation = 0b11; // Continuous conversion
    
    OPT3001_ErrorCode err = sensor.writeConfig(config);
    initialized = (err == NO_ERROR);
    return initialized;
  }

  float getLux() {
    if (!initialized) return 0.0f;
    OPT3001 result = sensor.readResult();
    if (result.error == NO_ERROR) {
      return result.lux;
    }
    return -1.0f;
  }
};

class EnvironmentSensorTest {
public:
  Bme68x sensor;
  bme68xData data;
  bool initialized = false;

  bool begin() {
    // 0x76 is BME690 default I2C address
    sensor.begin(0x76, Wire);
    // Configure for forced mode (one shot reading)
    sensor.setTPH(BME68X_OS_2X, BME68X_OS_16X, BME68X_OS_1X);
    sensor.setHeaterProf(320, 150); // 320C for 150ms
    initialized = true;
    return initialized;
  }

  bool triggerMeasurement() {
    if (!initialized) return false;
    sensor.setOpMode(BME68X_FORCED_MODE);
    return true;
  }

  uint32_t getWaitMs() {
    if (!initialized) return 50;
    return (sensor.getMeasDur() / 1000 + 10);
  }

  bool collectData(float &temp, float &hum, float &press, float &gas) {
    if (!initialized) return false;
    uint8_t nFields = sensor.fetchData();
    if (nFields > 0) {
      sensor.getData(data);
      temp = data.temperature;
      hum = data.humidity;
      press = data.pressure / 100.0f; // Pa to hPa
      gas = data.gas_resistance;
      return true;
    }
    return false;
  }

  bool readData(float &temp, float &hum, float &press, float &gas) {
    if (!triggerMeasurement()) return false;
    delay(getWaitMs());
    return collectData(temp, hum, press, gas);
  }
};

class HeartRateSensorTest {
public:
  MAX30105 sensor;
  bool initialized = false;
  bool isAwake = false;

  // Pulse & SpO2 state tracking
  unsigned long lastBeat = 0;
  float avgBpm = 0.0f;
  float currentSpO2 = 0.0f;
  float rates[4] = {0, 0, 0, 0};
  uint8_t rateSpot = 0;

  // AC/DC calculation for SpO2
  uint32_t irMin = 0xFFFFFFFF, irMax = 0;
  uint32_t redMin = 0xFFFFFFFF, redMax = 0;
  uint64_t irSum = 0, redSum = 0;
  uint32_t sampleCount = 0;

  bool begin() {
    initialized = sensor.begin(Wire, I2C_SPEED_FAST);
    if (initialized) {
      sensor.setup(0, 4, 2, 400, 411, 4096); // 0mA LED power
      sensor.setPulseAmplitudeRed(0);
      sensor.setPulseAmplitudeIR(0);
      sensor.shutDown(); // Hardware low-power shutdown
      isAwake = false;
    }
    return initialized;
  }

  void sleep() {
    if (!initialized) return;
    sensor.setPulseAmplitudeRed(0);
    sensor.setPulseAmplitudeIR(0);
    sensor.shutDown();
    isAwake = false;
  }

  void wake() {
    if (!initialized || isAwake) return;
    sensor.wakeUp();
    sensor.setup(0x24, 4, 2, 400, 411, 4096);
    sensor.setPulseAmplitudeRed(0x24);
    sensor.setPulseAmplitudeIR(0x24);
    isAwake = true;
    sampleCount = 0;
    irMin = 0xFFFFFFFF; irMax = 0;
    redMin = 0xFFFFFFFF; redMax = 0;
    irSum = 0; redSum = 0;
    avgBpm = 0.0f;
    currentSpO2 = 0.0f;
  }

  bool update(uint32_t &outRed, uint32_t &outIR, float &outBPM, float &outSpO2, bool &fingerOn, bool &beat) {
    beat = false;
    if (!initialized || !isAwake) {
      outRed = 0;
      outIR = 0;
      outBPM = 0;
      outSpO2 = 0;
      fingerOn = false;
      return false;
    }

    outRed = sensor.getRed();
    outIR = sensor.getIR();

    // Check if finger is placed on sensor
    if (outIR < 20000) {
      fingerOn = false;
      outBPM = 0;
      outSpO2 = 0;
      sampleCount = 0;
      return false;
    }

    fingerOn = true;

    // Heart beat detection using SparkFun algorithm
    if (checkForBeat((int32_t)outIR)) {
      beat = true;
      unsigned long now = millis();
      unsigned long delta = now - lastBeat;
      lastBeat = now;

      if (delta > 250 && delta < 2000) {
        float instantBpm = 60000.0f / (float)delta;
        if (instantBpm >= 45.0f && instantBpm <= 185.0f) {
          rates[rateSpot++] = instantBpm;
          rateSpot %= 4;

          float sum = 0;
          int count = 0;
          for (int i = 0; i < 4; i++) {
            if (rates[i] > 0) {
              sum += rates[i];
              count++;
            }
          }
          if (count > 0) {
            avgBpm = sum / count;
          }
        }
      }
    }

    // AC/DC measurement for SpO2 calculation
    sampleCount++;
    if (outIR < irMin) irMin = outIR;
    if (outIR > irMax) irMax = outIR;
    if (outRed < redMin) redMin = outRed;
    if (outRed > redMax) redMax = outRed;
    irSum += outIR;
    redSum += outRed;

    // Calculate SpO2 every ~80 samples (~1.6 seconds)
    if (sampleCount >= 80) {
      float irAC = (float)(irMax - irMin);
      float redAC = (float)(redMax - redMin);
      float irDC = (float)(irSum / sampleCount);
      float redDC = (float)(redSum / sampleCount);

      if (irDC > 0 && redDC > 0 && irAC > 0) {
        float ratio = (redAC / redDC) / (irAC / irDC);
        // Standard empirical SpO2 ratio formula
        float calcSpO2 = 110.0f - (25.0f * ratio);
        if (calcSpO2 >= 88.0f && calcSpO2 <= 100.0f) {
          if (currentSpO2 == 0.0f) currentSpO2 = calcSpO2;
          else currentSpO2 = (currentSpO2 * 0.7f) + (calcSpO2 * 0.3f); // Exponential filter
        }
      }

      sampleCount = 0;
      irMin = 0xFFFFFFFF; irMax = 0;
      redMin = 0xFFFFFFFF; redMax = 0;
      irSum = 0; redSum = 0;
    }

    outBPM = avgBpm;
    outSpO2 = currentSpO2;
    return true;
  }
};

class IMUSensorTest {
public:
  Adafruit_BNO08x sensor;
  sh2_SensorValue_t sensorValue;
  bool initialized = false;

  bool begin() {
    // Initialize without reset pin since reset is handled via software/I2C
    initialized = sensor.begin_I2C(0x4A, &Wire);
    if (initialized) {
      // Enable rotation vector and linear acceleration
      sensor.enableReport(SH2_ROTATION_VECTOR, 50000); // 50ms report interval
      sensor.enableReport(SH2_LINEAR_ACCELERATION, 50000);
    }
    return initialized;
  }

  bool update(float &roll, float &pitch, float &yaw, float &ax, float &ay, float &az) {
    if (!initialized) return false;

    if (sensor.getSensorEvent(&sensorValue)) {
      switch (sensorValue.sensorId) {
        case SH2_ROTATION_VECTOR: {
          // Convert quaternion to Euler angles (Roll, Pitch, Yaw)
          float q_i = sensorValue.un.rotationVector.i;
          float q_j = sensorValue.un.rotationVector.j;
          float q_k = sensorValue.un.rotationVector.k;
          float q_r = sensorValue.un.rotationVector.real;

          // Yaw
          float siny_cosp = 2.0f * (q_r * q_k + q_i * q_j);
          float cosy_cosp = 1.0f - 2.0f * (q_j * q_j + q_k * q_k);
          yaw = atan2(siny_cosp, cosy_cosp) * 57.2957795f;

          // Pitch
          float sinp = 2.0f * (q_r * q_j - q_k * q_i);
          if (abs(sinp) >= 1.0f) {
            pitch = copysign(3.14159265f / 2.0f, sinp) * 57.2957795f;
          } else {
            pitch = asin(sinp) * 57.2957795f;
          }

          // Roll
          float sinr_cosp = 2.0f * (q_r * q_i + q_j * q_k);
          float cosr_cosp = 1.0f - 2.0f * (q_i * q_i + q_j * q_j);
          roll = atan2(sinr_cosp, cosr_cosp) * 57.2957795f;
          return true;
        }
        case SH2_LINEAR_ACCELERATION: {
          ax = sensorValue.un.linearAcceleration.x;
          ay = sensorValue.un.linearAcceleration.y;
          az = sensorValue.un.linearAcceleration.z;
          return true;
        }
      }
    }
    return false;
  }
};
