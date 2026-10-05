#include "HAL.h"
#include "DisplayConfig.h"

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
NotificationMode HAL::notifMode = NOTIF_SILENT;
bool HAL::silentMode = true;
bool HAL::lightsEnabled = false;
int HAL::brightnessPercent = 85;
bool HAL::autoDimEnabled = false;
TiltMode HAL::tiltMode = TILT_BALANCED;
int HAL::screenTimeoutSec = 30;
bool HAL::wristCoverSleep = true;
bool HAL::pinsInited = false;

void HAL::setNotificationMode(NotificationMode mode) {
  notifMode = mode;
  silentMode = (mode == NOTIF_SILENT || mode == NOTIF_LIGHTS_ONLY);
  lightsEnabled = (mode == NOTIF_ALL || mode == NOTIF_LIGHTS_ONLY);
}

void HAL::setBrightness(int pct, LGFX* display) {
  brightnessPercent = constrain(pct, 10, 100);
  applyBrightness(display);
}

void HAL::applyBrightness(LGFX* display) {
  if (display) {
    uint8_t pwm = (uint8_t)map(brightnessPercent, 0, 100, 15, 255);
    display->setBrightness(pwm);
  }
}

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
  Wire.setClock(100000); // 100 kHz Standard Mode during sensor bring-up for maximum noise margin
  Wire.setTimeOut(100);
  delay(80); // Allow I2C bus lines and pullups to stabilize

  // Diagnostic I2C bus scan
  Serial.println("[HAL] Scanning I2C bus...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[HAL] I2C device detected at 0x%02X\n", addr);
    }
  }

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
  delay(40);

  // OPT3001 Ambient Light (Address 0x44)
  Wire.beginTransmission(0x44);
  if (Wire.endTransmission() == 0) {
    lightSensor.begin(0x44);
    OPT3001_Config optCfg;
    optCfg.RangeNumber = 0b1100;
    optCfg.ConvertionTime = 0b1;
    optCfg.Latch = 0b1;
    optCfg.ModeOfConversionOperation = 0b11;
    lightSensorReady = (lightSensor.writeConfig(optCfg) == NO_ERROR);
  } else {
    lightSensorReady = false;
  }
  delay(40);
  if (onProgress) onProgress(0.55f);

  envSensor.begin(0x76, Wire);
  envSensorReady = (envSensor.checkStatus() == BME68X_OK);
  if (envSensorReady) {
    envSensor.setTPH(BME68X_OS_2X, BME68X_OS_16X, BME68X_OS_1X);
    envSensor.setHeaterProf(300, 100);
    envSensor.setOpMode(BME68X_FORCED_MODE);
  }
  delay(60);

  // MAX30105 Biometrics (Probe 0x57 before initializing to prevent bus hangs)
  Wire.beginTransmission(0x57);
  if (Wire.endTransmission() == 0) {
    heartRateReady = heartRateSensor.begin(Wire, I2C_SPEED_FAST);
    if (heartRateReady) {
      heartRateSensor.shutDown(); // Keep LEDs off
    }
  } else {
    heartRateReady = false;
  }
  delay(60);
  if (onProgress) onProgress(0.75f);

  // BNO085 9-DOF IMU: retry loop across 0x4A and 0x4B with settling delays
  delay(100);
  for (int attempt = 0; attempt < 5 && !imuReady; attempt++) {
    imuReady = imuSensor.begin_I2C(0x4A, &Wire);
    if (!imuReady) {
      delay(40);
      imuReady = imuSensor.begin_I2C(0x4B, &Wire);
    }
    if (!imuReady) {
      Serial.printf("[HAL] BNO085 init retry %d...\n", attempt + 1);
      delay(80);
    }
  }
  if (imuReady) {
    imuSensor.enableReport(SH2_ROTATION_VECTOR, 20000); // 50 Hz 9-DOF fusion with magnetometer
    Serial.println("[HAL] BNO085 IMU initialized successfully!");
  } else {
    Serial.println("[HAL] BNO085 IMU NOT FOUND");
  }
  delay(120); // Allow SH2 dynamic calibration & sensor hub filter to stabilize
  if (onProgress) onProgress(0.95f);

  // Switch to 400 kHz for high-throughput runtime telemetry
  Wire.setClock(400000);
  delay(40);
  if (onProgress) onProgress(1.0f);
}

void HAL::setMatrixPower(bool on) {
  digitalWrite(PWR_NPM, on ? HIGH : LOW);
}

void HAL::buzzPip(uint16_t freqHz, uint16_t durationMs) {
  if (silentMode || freqHz == 0 || durationMs == 0) return;
  // Subtle soft micro-click: brief 15us pulse every 280us (~3.5kHz)
  uint32_t ms = (durationMs > 8) ? 8 : durationMs;
  uint32_t cycles = (ms * 1000UL) / 300UL;
  for (uint32_t i = 0; i < cycles; i++) {
    digitalWrite(BUZZER, HIGH);
    delayMicroseconds(15); // Low duty cycle produces a gentle soft tick instead of a loud screech
    digitalWrite(BUZZER, LOW);
    delayMicroseconds(285);
  }
  digitalWrite(BUZZER, LOW);
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
