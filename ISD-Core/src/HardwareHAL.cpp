#include "HardwareHAL.h"

Adafruit_NeoPixel* HardwareHAL::neoPixels = nullptr;
Adafruit_MAX17048 HardwareHAL::fuelGauge;
ClosedCube_OPT3001 HardwareHAL::lightSensor;
Bme68x HardwareHAL::envSensor;
MAX30105 HardwareHAL::heartRateSensor;
Adafruit_BNO08x HardwareHAL::imuSensor;
RV3028 HardwareHAL::rtcClock;

bool HardwareHAL::fuelGaugeReady = false;
bool HardwareHAL::lightSensorReady = false;
bool HardwareHAL::envSensorReady = false;
bool HardwareHAL::heartRateReady = false;
bool HardwareHAL::imuReady = false;
bool HardwareHAL::rtcReady = false;

void HardwareHAL::begin() {
  // 1. Power Gate for NeoPixels (Keep OFF for silence/power save)
  pinMode(PWR_NPM, OUTPUT);
  digitalWrite(PWR_NPM, LOW);

  // 2. SPI Chip Selects
  pinMode(CS_SD, OUTPUT);
  digitalWrite(CS_SD, HIGH);
  pinMode(CS_TFT, OUTPUT);
  digitalWrite(CS_TFT, HIGH);

  // 3. User Controls
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LEVER_LEFT, INPUT_PULLUP);
  pinMode(LEVER_PUSH, INPUT_PULLUP);
  pinMode(LEVER_RIGHT, INPUT_PULLUP);

  // 4. Power & Charge Detection
  pinMode(USB_DETECT, INPUT_PULLUP);
  pinMode(BAT_STAT, INPUT_PULLUP);

  // 5. Buzzer: Ensure completely OFF
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // 6. I2C Bus Bring-up
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  // 7. Probe sensors safely
  fuelGaugeReady = fuelGauge.begin(&Wire);
  rtcReady = rtcClock.begin();
  if (rtcReady) rtcClock.set24Hour();

  lightSensor.begin(0x44);
  OPT3001_Config optCfg;
  optCfg.RangeNumber = 0b1100;
  optCfg.ConvertionTime = 0b1;
  optCfg.Latch = 0b1;
  optCfg.ModeOfConversionOperation = 0b11;
  lightSensorReady = (lightSensor.writeConfig(optCfg) == NO_ERROR);

  envSensor.begin(0x76, Wire);
  envSensorReady = (envSensor.checkStatus() == BME68X_OK);

  heartRateReady = heartRateSensor.begin(Wire, I2C_SPEED_FAST);
  if (heartRateReady) {
    heartRateSensor.shutDown(); // Keep LEDs off
  }

  imuReady = imuSensor.begin_I2C(0x4A, &Wire);
}

void HardwareHAL::setMatrixPower(bool on) {
  digitalWrite(PWR_NPM, on ? HIGH : LOW);
}
