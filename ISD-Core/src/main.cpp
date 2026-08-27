#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "pins.h"

TFT_eSPI tft = TFT_eSPI();

// Default I2C Addresses for the sensor stack
#define ADDR_BNO085   0x4A
#define ADDR_BME690   0x76 
#define ADDR_OPT3001  0x44
#define ADDR_MAX30102 0x57
#define ADDR_RV3028   0x52
#define ADDR_MAX17048 0x36

void checkSensor(const char* name, uint8_t addr) {
  Wire.beginTransmission(addr);
  byte error = Wire.endTransmission();
  tft.print(name);
  if (error == 0) {
    tft.println(": OK");
  } else {
    tft.println(": FAIL");
  }
  delay(150);
}

void setup() {
  Serial.begin(115200);
  
  // Power Domains & Backlight
  pinMode(PWR_NPM, OUTPUT);
  digitalWrite(PWR_NPM, HIGH);
  pinMode(TFT_PWM, OUTPUT);
  digitalWrite(TFT_PWM, HIGH); 
  
  // Initialize Buttons
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LEVER_LEFT, INPUT_PULLUP);
  pinMode(LEVER_PUSH, INPUT_PULLUP);
  pinMode(LEVER_RIGHT, INPUT_PULLUP);
  
  delay(100);

  // Initialize I2C and Display
  Wire.begin(I2C_SDA, I2C_SCL);
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(2);

  // Loading Animation
  const char spinner[] = {'|', '/', '-', '\\'};
  tft.setCursor(10, 10);
  tft.print("Loading ");
  int cursorX = tft.getCursorX();
  int cursorY = tft.getCursorY();

  for (int i = 0; i < 15; i++) {
    tft.setCursor(cursorX, cursorY);
    tft.print(spinner[i % 4]);
    delay(200);
  }

  // Self-Check Routine
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.println("Self-Check:\n");
  
  tft.setTextSize(1);
  checkSensor("9-DOF (BNO085)", ADDR_BNO085);
  checkSensor("Env (BME690)", ADDR_BME690);
  checkSensor("ALS (OPT3001)", ADDR_OPT3001);
  checkSensor("BPM (MAX30102)", ADDR_MAX30102);
  checkSensor("RTC (RV-3028)", ADDR_RV3028);
  checkSensor("Fuel (MAX17048)", ADDR_MAX17048);
  
  tft.println("\nButton Test Active");
}

void loop() {
  // Overwrite the same area of the screen to prevent flickering
  tft.setCursor(0, 100); 
  
  tft.print("BTN:   ");
  tft.println(digitalRead(BTN) == LOW ? "[ PRESSED ]" : "[   UP    ]");
  
  tft.print("L_LFT: ");
  tft.println(digitalRead(LEVER_LEFT) == LOW ? "[ PRESSED ]" : "[   UP    ]");
  
  tft.print("L_PSH: ");
  tft.println(digitalRead(LEVER_PUSH) == LOW ? "[ PRESSED ]" : "[   UP    ]");
  
  tft.print("L_RGT: ");
  tft.println(digitalRead(LEVER_RIGHT) == LOW ? "[ PRESSED ]" : "[   UP    ]");

  delay(50);
}