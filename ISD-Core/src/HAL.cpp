#include "HAL.h"

Adafruit_NeoPixel* HAL::neoPixels = nullptr;
Adafruit_MAX17048 HAL::fuelGauge;
ClosedCube_OPT3001 HAL::lightSensor;
Bme68x HAL::envSensor;
MAX30105 HAL::heartRateSensor;
Adafruit_BNO08x HAL::imuSensor;
RV3028 HAL::rtcClock;

bool HAL::fuelGaugeReady = false;
bool HAL::lightSensorReady = false;
bool HAL::envSensorReady = false;
bool HAL::heartRateReady = false;
bool HAL::imuReady = false;
bool HAL::rtcReady = false;
bool HAL::pinsInited = false;

void HAL::initPins() {
  if (pinsInited) return;
  pinsInited = true;

  // 1. Power Gate for NeoPixels (Keep OFF for silence/power save)
  pinMode(PWR_NPM, OUTPUT);
  digitalWrite(PWR_NPM, LOW);

  // 2. SPI Chip Selects (Deselect SD and TFT prior to display driver init)
  pinMode(CS_SD, OUTPUT);
  digitalWrite(CS_SD, HIGH);
  pinMode(CS_TFT, OUTPUT);
  digitalWrite(CS_TFT, HIGH);

  // 3. User Controls
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LEVER_LEFT, INPUT_PULLUP);
  pinMode(LEVER_PUSH, INPUT_PULLUP);
  pinMode(LEVER_RIGHT, INPUT_PULLUP);

  // 4. Power & Charge Detection (BQ25170 /PG and /STAT are open-drain, active-LOW)
  pinMode(USB_DETECT, INPUT_PULLUP);
  pinMode(BAT_STAT, INPUT_PULLUP);

  // 5. Buzzer: Ensure completely OFF
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void HAL::begin(void (*onProgress)(float progress)) {
  initPins();
  if (onProgress) onProgress(0.10f);

  // 6. I2C Bus Bring-up
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400 kHz Fast Mode for high-throughput sensor telemetry
  delay(80); // Allow I2C bus lines and pullups to stabilize

  // 7. Probe sensors safely
  // Test if MAX17048 acknowledges at address 0x36
  Wire.beginTransmission(0x36);
  bool fuelAck = (Wire.endTransmission() == 0);

  if (fuelAck) {
    fuelGaugeReady = fuelGauge.begin(&Wire);
    if (!fuelGaugeReady) {
      delay(40);
      fuelGaugeReady = fuelGauge.isDeviceReady();
    }
    if (fuelGaugeReady) {
      delay(180); // MAX17048 requires 125-175ms for first ADC conversion
    }
  } else {
    fuelGaugeReady = false;
  }
  if (onProgress) onProgress(0.35f);

  rtcReady = rtcClock.begin(Wire);
  if (rtcReady) {
    rtcClock.set24Hour();
    rtcClock.updateTime();
    if (rtcClock.getYear() < 2024) {
      rtcClock.setToCompilerTime();
    }
  }
  delay(60);

  lightSensor.begin(0x44);
  OPT3001_Config optCfg;
  optCfg.RangeNumber = 0b1100;
  optCfg.ConvertionTime = 0b1;
  optCfg.Latch = 0b1;
  optCfg.ModeOfConversionOperation = 0b11;
  lightSensorReady = (lightSensor.writeConfig(optCfg) == NO_ERROR);
  delay(60);
  if (onProgress) onProgress(0.55f);

  envSensor.begin(0x76, Wire);
  envSensorReady = (envSensor.checkStatus() == BME68X_OK);
  if (envSensorReady) {
    envSensor.setTPH(BME68X_OS_2X, BME68X_OS_16X, BME68X_OS_1X);
    envSensor.setHeaterProf(300, 100);
    envSensor.setOpMode(BME68X_FORCED_MODE);
  }
  delay(100);

  heartRateReady = heartRateSensor.begin(Wire, I2C_SPEED_FAST);
  if (heartRateReady) {
    heartRateSensor.shutDown(); // Keep LEDs off
  }
  delay(80);
  if (onProgress) onProgress(0.75f);

  imuReady = imuSensor.begin_I2C(0x4A, &Wire);
  if (imuReady) {
    imuSensor.enableReport(SH2_ROTATION_VECTOR, 20000); // 50 Hz 9-DOF fusion with magnetometer
  }
  delay(150); // Allow SH2 dynamic calibration & sensor hub filter to stabilize
  if (onProgress) onProgress(0.95f);

  delay(80);
  if (onProgress) onProgress(1.0f);
}

void HAL::setMatrixPower(bool on) {
  digitalWrite(PWR_NPM, on ? HIGH : LOW);
}

bool HAL::readFuelGauge(float &outVolt, float &outPct, float &outRate) {
  // Method 1: Adafruit Driver (if initialized and responding)
  if (fuelGaugeReady) {
    float v = fuelGauge.cellVoltage();
    float p = fuelGauge.cellPercent();
    float r = fuelGauge.chargeRate();
    if (!isnan(v) && !isinf(v) && v >= 2.5f && v <= 4.5f) {
      outVolt = v;
      if (!isnan(p) && !isinf(p) && p >= 0.0f && p <= 125.0f) {
        outPct = (p > 100.0f) ? 100.0f : p;
      }
      if (!isnan(r) && !isinf(r)) {
        outRate = r;
      }
      return true;
    }
  }

  // Method 2: Direct I2C fallback to MAX17048 registers (address 0x36)
  Wire.beginTransmission(0x36);
  Wire.write(0x02); // VCELL register (2 bytes MSB first)
  if (Wire.endTransmission() == 0) {
    if (Wire.requestFrom((uint8_t)0x36, (uint8_t)2) == 2) {
      uint16_t vRaw = ((uint16_t)Wire.read() << 8) | Wire.read();
      float v = (float)vRaw * 78.125f / 1000000.0f;
      if (v >= 2.5f && v <= 4.5f) {
        outVolt = v;

        // Read SOC (register 0x04, 2 bytes MSB first)
        Wire.beginTransmission(0x36);
        Wire.write(0x04);
        if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)0x36, (uint8_t)2) == 2) {
          uint16_t pRaw = ((uint16_t)Wire.read() << 8) | Wire.read();
          float p = (float)pRaw / 256.0f;
          if (p >= 0.0f && p <= 125.0f) {
            outPct = (p > 100.0f) ? 100.0f : p;
          }
        }

        // Read CRATE (register 0x16, 2 bytes signed MSB first)
        Wire.beginTransmission(0x36);
        Wire.write(0x16);
        if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)0x36, (uint8_t)2) == 2) {
          int16_t rRaw = (int16_t)(((uint16_t)Wire.read() << 8) | Wire.read());
          outRate = (float)rRaw * 0.208f;
        }
        return true;
      }
    }
  }
  return false;
}
