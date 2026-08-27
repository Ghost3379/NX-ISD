#pragma once
#include <Arduino.h>
#include <Wire.h>

// Libraries
#include <Adafruit_MAX1704X.h>
#include <ClosedCube_OPT3001.h>
#include <bme68xLibrary.h>
#include <MAX30105.h>
#include <Adafruit_BNO08x.h>

class FuelGaugeTest {
public:
  Adafruit_MAX17048 sensor;
  bool initialized = false;

  bool begin() {
    initialized = sensor.begin(&Wire);
    return initialized;
  }

  float getVoltage() {
    return initialized ? sensor.cellVoltage() : 0.0f;
  }

  float getPercent() {
    return initialized ? sensor.cellPercent() : 0.0f;
  }

  float getChangeRate() {
    return initialized ? sensor.chargeRate() : 0.0f;
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
    // 0x76 is BME680 default I2C address
    sensor.begin(0x76, Wire);
    // Configure for forced mode (one shot reading)
    sensor.setTPH(BME68X_OS_2X, BME68X_OS_16X, BME68X_OS_1X);
    sensor.setHeaterProf(320, 150); // 320C for 150ms
    initialized = true;
    return initialized;
  }

  bool readData(float &temp, float &hum, float &press, float &gas) {
    if (!initialized) return false;
    
    sensor.setOpMode(BME68X_FORCED_MODE);
    delay(sensor.getMeasDur() / 1000 + 10); // Wait for measurement to complete
    
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
};

class HeartRateSensorTest {
public:
  MAX30105 sensor;
  bool initialized = false;

  bool begin() {
    // MAX30102 shares same address (0x57) and register set with MAX30105
    initialized = sensor.begin(Wire, I2C_SPEED_FAST);
    if (initialized) {
      // Default setup: LED Power = 12.4mA (0x24), Sample Rate = 400, Led Mode = 2 (Red+IR)
      sensor.setup(0x24, 4, 2, 400, 411, 4096);
    }
    return initialized;
  }

  void getSample(uint32_t &red, uint32_t &ir) {
    if (!initialized) {
      red = 0;
      ir = 0;
      return;
    }
    // Get raw sensor readings
    red = sensor.getRed();
    ir = sensor.getIR();
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
