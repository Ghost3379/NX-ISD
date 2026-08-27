#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "pins.h"
#include "TestSensors.h"
#include "TestPeripherals.h"
#include "SensorState.h"

// External instances defined in main.cpp
extern FuelGaugeTest fuelGauge;
extern LightSensorTest lightSensor;
extern EnvironmentSensorTest envSensor;
extern HeartRateSensorTest heartRate;
extern IMUSensorTest imuSensor;
extern RTCTest rtcClock;
extern SDCardTest sdCard;
extern NeoPixelTest neoPixel;
extern BuzzerTest buzzer;
extern TFT_eSPI tft;

enum DiagnosticState {
  STATE_MENU,
  STATE_I2C,
  STATE_LED,
  STATE_RTC,
  STATE_POWER,
  STATE_ENV,
  STATE_IMU,
  STATE_BIO,
  STATE_SD_SOUND
};

class DiagnosticMenu {
private:
  DiagnosticState state = STATE_MENU;
  int menuIndex = 0;
  const int menuCount = 8;
  const char* menuItems[8] = {
    "1. I2C BUS SCANNER",
    "2. DISPLAY & LEDS",
    "3. REAL-TIME CLOCK",
    "4. FUEL & CHARGING",
    "5. ENVIRONMENT & ALS",
    "6. IMU 9-DOF MOTION",
    "7. PULSE BIOMETRICS",
    "8. NAND-SD & BUZZER"
  };

  // Button debouncing states
  bool lastBtnState = HIGH;
  bool lastLeftState = HIGH;
  bool lastRightState = HIGH;
  bool lastPushState = HIGH;

  // Redraw controllers
  bool menuNeedsRedraw = true;
  bool subscreenInit = false;
  bool actionTriggered = false;
  unsigned long lastUpdate = 0;

  // LED page state
  uint8_t ledColorIdx = 0;
  uint8_t ledRainbowStep = 0;

  // Bio graph variables
  static const int graphWidth = 180;
  uint32_t ppgHistory[graphWidth];
  int ppgIdx = 0;

  // Helper to fetch a thread-safe snapshot of the sensor telemetry
  SensorState getLocalState() {
    SensorState localState;
    if (stateMutex != NULL) {
      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        localState = sharedState;
        xSemaphoreGive(stateMutex);
      }
    }
    return localState;
  }

public:
  void begin() {
    state = STATE_MENU;
    menuIndex = 0;
    menuNeedsRedraw = true;
    
    // Clear Bio graph history
    for (int i = 0; i < graphWidth; i++) ppgHistory[i] = 0;
    
    pinMode(BTN, INPUT_PULLUP);
    pinMode(LEVER_LEFT, INPUT_PULLUP);
    pinMode(LEVER_PUSH, INPUT_PULLUP);
    pinMode(LEVER_RIGHT, INPUT_PULLUP);
    pinMode(USB_DETECT, INPUT_PULLUP);
    pinMode(BAT_STAT, INPUT_PULLUP);
  }

  void handleInput() {
    bool btn = digitalRead(BTN);
    bool lft = digitalRead(LEVER_LEFT);
    bool rgt = digitalRead(LEVER_RIGHT);
    bool psh = digitalRead(LEVER_PUSH);

    // Main BTN (active low) exits back to main menu
    if (btn == LOW && lastBtnState == HIGH) {
      buzzer.playClick();
      if (state != STATE_MENU) {
        state = STATE_MENU;
        tft.fillScreen(TFT_BLACK);
        menuNeedsRedraw = true;
        neoPixel.powerDown(); // Shut off NeoPixels when leaving LED page
      }
      delay(150);
    }

    // Lever Left (Up in menu)
    if (lft == LOW && lastLeftState == HIGH) {
      buzzer.playClick();
      if (state == STATE_MENU) {
        menuIndex = (menuIndex - 1 + menuCount) % menuCount;
        menuNeedsRedraw = true;
      }
      delay(150);
    }

    // Lever Right (Down in menu)
    if (rgt == LOW && lastRightState == HIGH) {
      buzzer.playClick();
      if (state == STATE_MENU) {
        menuIndex = (menuIndex + 1) % menuCount;
        menuNeedsRedraw = true;
      }
      delay(150);
    }

    // Lever Push (Select or trigger action)
    if (psh == LOW && lastPushState == HIGH) {
      buzzer.playClick();
      if (state == STATE_MENU) {
        state = (DiagnosticState)(menuIndex + 1);
        tft.fillScreen(TFT_BLACK);
        subscreenInit = true;
      } else {
        actionTriggered = true;
      }
      delay(150);
    }

    lastBtnState = btn;
    lastLeftState = lft;
    lastRightState = rgt;
    lastPushState = psh;
  }

  void update() {
    handleInput();

    unsigned long now = millis();
    bool periodicUpdate = (now - lastUpdate > 100); // 10Hz tick

    if (state == STATE_MENU) {
      if (menuNeedsRedraw) {
        drawMenu();
        menuNeedsRedraw = false;
      }
    } else {
      drawSubscreen(periodicUpdate);
      if (periodicUpdate) {
        lastUpdate = now;
      }
    }
  }

private:
  void drawHeader(const char* title, const SensorState &stateData) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextSize(1);
    
    // Draw Title
    tft.drawString(title, 5, 5);

    // Draw battery fuel gauge reading
    char batStr[24];
    if (fuelGauge.initialized) {
      snprintf(batStr, sizeof(batStr), "BAT: %02d%% %.2fV", 
               (int)stateData.batPercent, stateData.batVoltage);
    } else {
      snprintf(batStr, sizeof(batStr), "BAT: FAIL");
    }
    tft.drawString(batStr, 135, 5);

    // Top border line
    tft.drawFastHLine(0, 18, 240, TFT_ORANGE);
  }

  void drawMenu() {
    SensorState stateData = getLocalState();
    tft.fillScreen(TFT_BLACK);
    drawHeader("NX-ISD DIAGNOSE", stateData);

    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    
    // Instructions at bottom
    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.drawString("Lever L/R: Navigate  Push: Select", 10, 225);

    // Draw vertical scroll menu list
    for (int i = 0; i < menuCount; i++) {
      int yPos = 35 + (i * 22);
      
      if (i == menuIndex) {
        // Highlighted selection box
        tft.fillRect(5, yPos - 3, 230, 20, TFT_ORANGE);
        tft.setTextColor(TFT_BLACK, TFT_ORANGE);
        tft.drawString(menuItems[i], 12, yPos);
      } else {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(menuItems[i], 12, yPos);
      }
    }
  }

  void drawSubscreen(bool periodicUpdate) {
    SensorState stateData = getLocalState();

    if (subscreenInit) {
      tft.fillScreen(TFT_BLACK);
      subscreenInit = false;
      actionTriggered = false;
      ledColorIdx = 0;
      ppgIdx = 0;
      for (int i = 0; i < graphWidth; i++) ppgHistory[i] = 0;
    }

    switch (state) {
      case STATE_I2C:
        drawI2CScanner(periodicUpdate, stateData);
        break;
      case STATE_LED:
        drawLEDTest(periodicUpdate, stateData);
        break;
      case STATE_RTC:
        drawRTCTestPage(periodicUpdate, stateData);
        break;
      case STATE_POWER:
        drawPowerTest(periodicUpdate, stateData);
        break;
      case STATE_ENV:
        drawEnvTest(periodicUpdate, stateData);
        break;
      case STATE_IMU:
        drawIMUTest(periodicUpdate, stateData);
        break;
      case STATE_BIO:
        drawBioTest(periodicUpdate, stateData);
        break;
      case STATE_SD_SOUND:
        drawSDSoundTest(periodicUpdate, stateData);
        break;
      default:
        break;
    }
  }

  // ================== DIAGNOSTIC SCREENS ==================

  // 1. I2C Scanner
  void drawI2CScanner(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("1. I2C BUS ADDR SCAN", stateData);
    tft.setTextSize(1);
    
    // Static text labels on first entry
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Target I2C Addresses:", 5, 25);

    // List target devices
    auto drawStatus = [](const char* name, bool ok, int y) {
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.drawString(name, 10, y);
      if (ok) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("OK  (0x)", 115, y);
      } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("FAIL(0x)", 115, y);
      }
    };
    
    drawStatus("BNO085 (9-DOF):", imuSensor.initialized, 40);
    tft.drawString("4A", 160, 40);

    drawStatus("BME690 (Env)  :", envSensor.initialized, 55);
    tft.drawString("76", 160, 55);

    drawStatus("OPT3001 (ALS) :", lightSensor.initialized, 70);
    tft.drawString("44", 160, 70);

    drawStatus("MAX30102 (BPM):", heartRate.initialized, 85);
    tft.drawString("57", 160, 85);

    drawStatus("RV-3028 (RTC) :", rtcClock.initialized, 100);
    tft.drawString("52", 160, 100);

    drawStatus("MAX17048(Fuel):", fuelGauge.initialized, 115);
    tft.drawString("36", 160, 115);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawFastHLine(0, 130, 240, TFT_ORANGE);
    tft.drawString("Active Bus Scan Matrix:", 5, 135);

    // Draw active address grid
    if (periodicUpdate) {
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 16; c++) {
          uint8_t addr = r * 16 + c;
          int x = 10 + (c * 13);
          int y = 150 + (r * 8);
          
          if (addr < 0x08 || addr > 0x77) {
            tft.setTextColor(tft.color565(80, 80, 80), TFT_BLACK);
            tft.drawString(".", x, y);
            continue;
          }

          Wire.beginTransmission(addr);
          if (Wire.endTransmission() == 0) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString("X", x, y);
          } else {
            tft.setTextColor(tft.color565(60, 30, 0), TFT_BLACK);
            tft.drawString(".", x, y);
          }
        }
      }
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Press BTN to exit", 10, 225);
  }

  // 2. Display & LEDs
  void drawLEDTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("2. DISPLAY & NEOPixels", stateData);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Test Screen Color & WS2812B Matrix", 5, 25);
    
    if (actionTriggered) {
      actionTriggered = false;
      ledColorIdx = (ledColorIdx + 1) % 6;
      buzzer.playClick();
      
      uint16_t colors[6] = {TFT_BLACK, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
      tft.fillScreen(colors[ledColorIdx]);
      if (colors[ledColorIdx] != TFT_BLACK) {
        tft.setTextColor(TFT_BLACK, colors[ledColorIdx]);
        tft.drawString("PUSH LEVER TO CYCLE", 60, 110);
        tft.drawString("BTN TO RETURN", 75, 130);
        delay(150);
        return;
      }
    }

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("1. NeoPixel matrix power PWR_NPM = ON", 10, 50);
    tft.drawString("2. Toggling NeoPixel colors (RGB Wheel)", 10, 65);
    tft.drawString("3. Push Lever to cycle screen test colors:", 10, 80);

    const char* colorNames[6] = {"Normal Black", "Red Fill", "Green Fill", "Blue Fill", "White Fill", "Normal Black"};
    tft.drawString("   Current: ", 10, 105);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(colorNames[ledColorIdx], 80, 105);

    if (!neoPixel.powered) {
      neoPixel.begin();
    }
    
    ledRainbowStep += 4;
    neoPixel.runColorCycle(ledRainbowStep);

    tft.drawRect(20, 135, 200, 70, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("WS2812B NeoPixel Grid (9 LEDs):", 30, 145);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Powered (PWR_NPM Pin 17 = HIGH)", 30, 165);
    tft.drawString("Colors cycling...", 30, 180);

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Push: Test Screen  BTN: Exit", 10, 225);
  }

  // 3. Real-Time Clock
  void drawRTCTestPage(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("3. RV-3028 RTC TEST", stateData);
    tft.setTextSize(1);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Onboard Real-Time Clock Subsystem", 5, 25);

    char timeStr[32];
    char dateStr[32];
    rtcClock.getTimeString(timeStr, sizeof(timeStr));
    rtcClock.getDateString(dateStr, sizeof(dateStr));

    tft.drawRect(15, 45, 210, 80, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("CURRENT DATE:", 25, 55);
    tft.drawString("CURRENT TIME:", 25, 85);

    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(dateStr, 25, 68);
    tft.drawString(timeStr, 25, 98);

    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("RTC Chip Address: 0x52", 15, 140);
    tft.drawString("Interrupt Line  : INT_RTC (GPIO6)", 15, 155);

    tft.drawRect(15, 175, 210, 35, tft.color565(120, 80, 0));
    tft.drawString("Push Lever to Sync clock to compile time", 20, 180);
    tft.drawString("via rtcClock.setToCompilerTime()", 20, 195);

    if (actionTriggered) {
      actionTriggered = false;
      rtcClock.setDummyTime();
      buzzer.playSuccessBeep();
      tft.fillRect(16, 176, 208, 33, TFT_GREEN);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.drawString("CLOCK SYNC SUCCESSFUL!", 40, 187);
      delay(500);
      subscreenInit = true;
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Push: Set Compile Time  BTN: Exit", 10, 225);
  }

  // 4. Fuel & Charging
  void drawPowerTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("4. POWER & FUEL GAUGE", stateData);
    tft.setTextSize(1);

    bool usbPlugged = (digitalRead(USB_DETECT) == LOW);
    bool isCharging = (digitalRead(BAT_STAT) == LOW);

    float volt = stateData.batVoltage;
    float pct = stateData.batPercent;
    float rate = stateData.batChangeRate;

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Battery & USB Charging Status", 5, 25);

    tft.drawRect(10, 40, 105, 80, TFT_ORANGE);
    tft.drawString("CHARGER STATE", 15, 45);
    
    tft.drawString("USB In:", 15, 65);
    tft.setTextColor(usbPlugged ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(usbPlugged ? "CONNECTED" : "UNPLUGGED", 60, 65);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Charge:", 15, 80);
    tft.setTextColor(isCharging ? TFT_GREEN : tft.color565(180, 180, 180), TFT_BLACK);
    tft.drawString(isCharging ? "CHARGING" : "STANDBY/OFF", 60, 80);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("TS (NTC): 10K OK", 15, 95);

    tft.drawRect(125, 40, 105, 80, TFT_ORANGE);
    tft.drawString("MAX17048 GAUGE", 130, 45);
    
    char valStr[16];
    tft.drawString("Volt :", 130, 65);
    snprintf(valStr, sizeof(valStr), "%.3f V", volt);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(valStr, 170, 65);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Level:", 130, 80);
    snprintf(valStr, sizeof(valStr), "%.1f %%", pct);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(valStr, 170, 80);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Rate :", 130, 95);
    snprintf(valStr, sizeof(valStr), "%.1f %%/h", rate);
    tft.setTextColor(rate >= 0 ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(valStr, 170, 95);

    tft.drawRect(20, 140, 200, 30, TFT_ORANGE);
    tft.drawRect(220, 148, 5, 14, TFT_ORANGE);

    int fillWidth = (int)(196.0f * (pct > 100.0f ? 100.0f : (pct < 0.0f ? 0.0f : pct)) / 100.0f);
    if (fillWidth > 0) {
      tft.fillRect(22, 142, fillWidth, 26, isCharging ? TFT_GREEN : TFT_ORANGE);
    }

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("USB_DETECT: GPIO12   BAT_STAT: GPIO11", 10, 185);
    tft.drawString("Fuel Gauge I2C Address: 0x36", 10, 200);

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.drawString("Press BTN to exit", 10, 225);
  }

  // 5. Environment & ALS
  void drawEnvTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("5. BME690 & OPT3001", stateData);
    tft.setTextSize(1);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Environment & Ambient Light", 5, 25);

    float temp = stateData.temp;
    float hum = stateData.hum;
    float press = stateData.press;
    float gas = stateData.gas;
    bool bmeOk = stateData.envDataReady;
    float lux = stateData.lightLux;

    tft.drawRect(10, 40, 220, 95, TFT_ORANGE);
    tft.drawString("BME690 SENSOR DATA (0x76)", 15, 45);
    
    char str[32];
    tft.drawString("Temperature :", 20, 65);
    if (bmeOk) {
      snprintf(str, sizeof(str), "%.2f C", temp);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(str, 120, 65);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No Data", 120, 65);
    }

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Humidity    :", 20, 80);
    if (bmeOk) {
      snprintf(str, sizeof(str), "%.2f %%", hum);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(str, 120, 80);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No Data", 120, 80);
    }

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Pressure    :", 20, 95);
    if (bmeOk) {
      snprintf(str, sizeof(str), "%.1f hPa", press);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(str, 120, 95);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No Data", 120, 95);
    }

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Gas Resist. :", 20, 110);
    if (bmeOk) {
      snprintf(str, sizeof(str), "%.1f K Ohm", gas / 1000.0f);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(str, 120, 110);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No Data", 120, 110);
    }

    tft.drawRect(10, 145, 220, 65, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("OPT3001 AMBIENT LIGHT (0x44)", 15, 150);

    tft.drawString("Light Level :", 20, 175);
    if (lightSensor.initialized && lux >= 0.0f) {
      snprintf(str, sizeof(str), "%.2f Lux", lux);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString(str, 120, 175);
      
      int luxBar = (int)(80.0f * (lux > 1000.0f ? 1000.0f : lux) / 1000.0f);
      tft.drawRect(20, 192, 200, 10, TFT_ORANGE);
      if (luxBar > 0) {
        tft.fillRect(22, 194, luxBar * 2 + 2, 6, TFT_GREEN);
      }
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("No Sensor / Error", 120, 175);
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Press BTN to exit", 10, 225);
  }

  // 6. IMU Motion
  void drawIMUTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("6. BNO085 9-DOF MOTION", stateData);
    tft.setTextSize(1);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Orientation & Accelerometer", 5, 25);

    float roll = stateData.roll;
    float pitch = stateData.pitch;
    float yaw = stateData.yaw;
    float ax = stateData.ax;
    float ay = stateData.ay;
    float az = stateData.az;
    bool updateOk = stateData.imuDataReady;

    tft.drawRect(10, 40, 105, 95, TFT_ORANGE);
    tft.drawString("EULER ANGLES", 15, 45);
    tft.drawString("Roll :", 15, 65);
    tft.drawString("Pitch:", 15, 85);
    tft.drawString("Yaw  :", 15, 105);

    tft.drawRect(125, 40, 105, 95, TFT_ORANGE);
    tft.drawString("ACCEL (m/s^2)", 130, 45);
    tft.drawString("X:", 130, 65);
    tft.drawString("Y:", 130, 85);
    tft.drawString("Z:", 130, 105);

    char val[16];
    if (imuSensor.initialized && updateOk) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      snprintf(val, sizeof(val), "%6.1f", roll);
      tft.drawString(val, 55, 65);
      snprintf(val, sizeof(val), "%6.1f", pitch);
      tft.drawString(val, 55, 85);
      snprintf(val, sizeof(val), "%6.1f", yaw);
      tft.drawString(val, 55, 105);

      snprintf(val, sizeof(val), "%+5.2f", ax);
      tft.drawString(val, 155, 65);
      snprintf(val, sizeof(val), "%+5.2f", ay);
      tft.drawString(val, 155, 85);
      snprintf(val, sizeof(val), "%+5.2f", az);
      tft.drawString(val, 155, 105);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("OFFLINE", 35, 85);
      tft.drawString("OFFLINE", 150, 85);
    }

    int centerX = 120;
    int centerY = 175;
    int radius = 30;

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawCircle(centerX, centerY, radius, TFT_ORANGE);
    tft.drawCircle(centerX, centerY, 5, tft.color565(100, 60, 0));
    tft.drawFastHLine(centerX - radius - 5, centerY, (radius + 5) * 2, tft.color565(100, 60, 0));
    tft.drawFastVLine(centerX, centerY - radius - 5, (radius + 5) * 2, tft.color565(100, 60, 0));

    if (imuSensor.initialized && updateOk) {
      int dx = (int)(roll * 0.7f);
      int dy = (int)(-pitch * 0.7f);

      float distance = sqrt(dx*dx + dy*dy);
      if (distance > radius - 3) {
        dx = (int)(dx * (radius - 3) / distance);
        dy = (int)(dy * (radius - 3) / distance);
      }

      tft.fillCircle(centerX + dx, centerY + dy, 4, TFT_GREEN);
    } else {
      tft.fillCircle(centerX, centerY, 4, TFT_RED);
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Tilt watch to test accelerometer/gyro", 10, 225);
  }

  // 7. Biometrics
  void drawBioTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("7. PULSE & BPM SENSOR", stateData);
    tft.setTextSize(1);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("MAX30102 Photoplethysmogram (PPG)", 5, 25);

    uint32_t red = stateData.bpmRed;
    uint32_t ir = stateData.bpmIR;

    tft.drawRect(10, 40, 220, 50, TFT_ORANGE);
    tft.drawString("MAX30102 CHIP TELEMETRY (0x57)", 15, 45);

    char valStr[32];
    tft.drawString("Red Intensity:", 20, 65);
    snprintf(valStr, sizeof(valStr), "%lu", red);
    tft.setTextColor(heartRate.initialized ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(valStr, 130, 65);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("IR Intensity :", 20, 75);
    snprintf(valStr, sizeof(valStr), "%lu", ir);
    tft.setTextColor(heartRate.initialized ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.drawString(valStr, 130, 75);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawRect(10, 100, 220, 110, TFT_ORANGE);
    tft.drawString("Live PPG Graph (Place Finger on Sensor):", 15, 105);

    if (heartRate.initialized && ir > 20000) {
      ppgHistory[ppgIdx] = ir;
      ppgIdx = (ppgIdx + 1) % graphWidth;

      uint32_t minVal = 0xFFFFFFFF;
      uint32_t maxVal = 0;
      for (int i = 0; i < graphWidth; i++) {
        if (ppgHistory[i] == 0) continue;
        if (ppgHistory[i] < minVal) minVal = ppgHistory[i];
        if (ppgHistory[i] > maxVal) maxVal = ppgHistory[i];
      }

      if (maxVal == minVal) {
        maxVal = minVal + 1;
      }

      int graphX = 30;
      int graphY = 120;
      int graphH = 80;

      tft.fillRect(12, 115, 216, 92, TFT_BLACK);
      tft.drawString("PPG Pulse Wave:", 15, 105);

      for (int x = 0; x < graphWidth - 1; x++) {
        int idx1 = (ppgIdx + x) % graphWidth;
        int idx2 = (ppgIdx + x + 1) % graphWidth;

        uint32_t val1 = ppgHistory[idx1];
        uint32_t val2 = ppgHistory[idx2];

        if (val1 == 0 || val2 == 0) continue;

        int y1 = graphY + graphH - (int)((val1 - minVal) * graphH / (maxVal - minVal));
        int y2 = graphY + graphH - (int)((val2 - minVal) * graphH / (maxVal - minVal));

        tft.drawLine(graphX + x, y1, graphX + x + 1, y2, TFT_GREEN);
      }
    } else {
      tft.fillRect(12, 115, 216, 92, TFT_BLACK);
      tft.drawString("Live PPG Graph (Place Finger on Sensor):", 15, 105);
      tft.setTextColor(tft.color565(120, 80, 0), TFT_BLACK);
      if (heartRate.initialized) {
        tft.drawString("PLACE FINGER ON SENSOR", 50, 150);
        tft.drawString("(Telemetries are updating live)", 30, 170);
      } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("BIOMETRICS SENSOR OFFLINE", 40, 150);
      }
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Press BTN to exit", 10, 225);
  }

  // 8. NAND-SD & Sound
  void drawSDSoundTest(bool periodicUpdate, const SensorState &stateData) {
    drawHeader("8. NAND-SD & BUZZER", stateData);
    tft.setTextSize(1);

    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("NAND Flash Storage & Audio Beeps", 5, 25);

    tft.drawRect(10, 45, 220, 75, TFT_ORANGE);
    tft.drawString("NAND-SD FLASH DRIVER (CS: 47)", 15, 50);

    tft.drawString("Mount Status:", 20, 70);
    if (sdCard.initialized) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("MOUNTED SUCCESS", 110, 70);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("MOUNT FAILED", 110, 70);
    }

    tft.drawString("Read/Write  :", 20, 85);
    tft.drawString("Press Push to test I/O", 110, 85);

    if (actionTriggered) {
      actionTriggered = false;
      buzzer.playClick();
      
      tft.fillRect(110, 85, 115, 12, TFT_BLACK);
      tft.setTextColor(tft.color565(180, 180, 180), TFT_BLACK);
      tft.drawString("TESTING...", 110, 85);
      
      bool ok = sdCard.testReadWrite();
      tft.fillRect(110, 85, 115, 12, TFT_BLACK);
      if (ok) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("FILE I/O SUCCESS", 110, 85);
        buzzer.playSuccessBeep();
      } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("I/O FAIL/NO CARD", 110, 85);
        buzzer.playFailureBeep();
      }
    }

    tft.drawRect(10, 135, 220, 75, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("SMD BUZZER & LED FLASH", 15, 140);
    tft.drawString("Buzzer GPIO : GPIO10", 20, 160);
    tft.drawString("Buzzer Driver: N-MOS Q1 (AO3400A)", 20, 175);
    tft.drawString("Press L/R   : Play Melody / Alarm", 20, 190);

    bool lft = (digitalRead(LEVER_LEFT) == LOW);
    bool rgt = (digitalRead(LEVER_RIGHT) == LOW);
    if (lft) {
      buzzer.playStartupMelody();
      delay(150);
    }
    if (rgt) {
      buzzer.playBeep(2000, 50);
      buzzer.playBeep(1000, 50);
      buzzer.playBeep(2000, 50);
      delay(150);
    }

    tft.drawFastHLine(0, 220, 240, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("Push: Run SD Test  L/R: Buzz  BTN: Exit", 10, 225);
  }
};
