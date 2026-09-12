/*
  ==============================================================================
  NX-ISD v1.0 — Display & 3.3V Mod Diagnostic Test (Arduino IDE)
  ==============================================================================
  Target: ESP32-S3 (NX-ISD v1.0 / v1.3)
  Purpose: Fast, standalone hardware bring-up test for the circular SPI display.
           Tests the 3.3V power mod and backlight circuitry.
           
  ZERO EXTERNAL LIBRARIES REQUIRED! Uses only built-in <SPI.h>.
  
  Pins (from schematic):
    TFT_PWM  (Backlight)  : GPIO 1  (N-MOSFET Q4, Active HIGH)
    TFT_RST  (Reset)      : GPIO 38 (Active LOW)
    TFT_DC   (Data/Cmd RS): GPIO 21 (Data = 1, Command = 0)
    CS_TFT   (Chip Select): GPIO 48 (Active LOW)
    SPI_SCK  (Clock)      : GPIO 40
    SPI_MOSI (Data Out)   : GPIO 42
    SPI_MISO (Data In)    : GPIO 41 (Unused by display)
    BUZZER   (Audio)      : GPIO 10
    BTN / LEVER           : GPIO 13, 14, 15, 16
  ==============================================================================
*/

#include <Arduino.h>
#include <SPI.h>

// ==================== CONFIGURATION ====================
// Select your display controller (uncomment ONE):
#define DRIVER_GC9A01   // Standard 1.28" Round 240x240 LCD (Most common)
//#define DRIVER_ST7789   // Alternative 240x240 controller

// Hardware Pin Definitions
#define PIN_TFT_PWM   1   // Backlight enable via Q4 (Active HIGH)
#define PIN_TFT_RST   38  // Display Reset (Active LOW)
#define PIN_TFT_DC    21  // Data / Command select (RS)
#define PIN_CS_TFT    48  // SPI Chip Select (Active LOW)
#define PIN_SPI_SCK   40  // SPI Clock
#define PIN_SPI_MOSI  42  // SPI MOSI (SDA)
#define PIN_SPI_MISO  41  // SPI MISO

#define PIN_BUZZER    10  // Onboard SMD Buzzer
#define PIN_BTN       13  // Main button (Active LOW)
#define PIN_LEVER_PUSH 15 // Lever push (Active LOW)

#define TFT_WIDTH     240
#define TFT_HEIGHT    240

// 16-bit RGB565 Colors
#define COLOR_BLACK   0x0000
#define COLOR_BLUE    0x001F
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_YELLOW  0xFFE0
#define COLOR_WHITE   0xFFFF
#define COLOR_ORANGE  0xFD20

// Fast SPI settings (20 MHz for stable test)
SPISettings spiSettings(20000000, MSBFIRST, SPI_MODE0);

// ==================== LOW-LEVEL SPI FUNCTIONS ====================

inline void writeCommand(uint8_t cmd) {
  digitalWrite(PIN_TFT_DC, LOW);   // DC = 0 for Command
  digitalWrite(PIN_CS_TFT, LOW);  // Select display
  SPI.transfer(cmd);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect
}

inline void writeData(uint8_t data) {
  digitalWrite(PIN_TFT_DC, HIGH);  // DC = 1 for Data
  digitalWrite(PIN_CS_TFT, LOW);  // Select display
  SPI.transfer(data);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect
}

void writeDataBlock(const uint8_t *data, size_t len) {
  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_CS_TFT, LOW);
  SPI.transferBytes(data, NULL, len);
  digitalWrite(PIN_CS_TFT, HIGH);
}

void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writeCommand(0x2A); // Column addr set
  writeData(x0 >> 8);
  writeData(x0 & 0xFF);
  writeData(x1 >> 8);
  writeData(x1 & 0xFF);

  writeCommand(0x2B); // Row addr set
  writeData(y0 >> 8);
  writeData(y0 & 0xFF);
  writeData(y1 >> 8);
  writeData(y1 & 0xFF);

  writeCommand(0x2C); // Write to RAM
}

void fillScreen(uint16_t color) {
  setAddrWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
  
  uint8_t high = color >> 8;
  uint8_t low  = color & 0xFF;

  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_CS_TFT, LOW);

  // Send buffer in chunks of 512 bytes for speed
  const size_t bufSize = 512;
  uint8_t buf[bufSize];
  for (size_t i = 0; i < bufSize; i += 2) {
    buf[i]     = high;
    buf[i + 1] = low;
  }

  size_t totalBytes = (size_t)TFT_WIDTH * TFT_HEIGHT * 2;
  while (totalBytes > 0) {
    size_t chunk = (totalBytes < bufSize) ? totalBytes : bufSize;
    SPI.transferBytes(buf, NULL, chunk);
    totalBytes -= chunk;
  }

  digitalWrite(PIN_CS_TFT, HIGH);
}

// Draw a filled rectangle
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
  if (x + w > TFT_WIDTH)  w = TFT_WIDTH - x;
  if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;

  setAddrWindow(x, y, x + w - 1, y + h - 1);

  uint8_t high = color >> 8;
  uint8_t low  = color & 0xFF;

  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_CS_TFT, LOW);

  size_t totalBytes = (size_t)w * h * 2;
  while (totalBytes--) {
    SPI.transfer(high);
    SPI.transfer(low);
    totalBytes--;
  }

  digitalWrite(PIN_CS_TFT, HIGH);
}

// ==================== DISPLAY INITIALIZATIONS ====================

#ifdef DRIVER_GC9A01
void initDisplay() {
  Serial.println("[TFT] Sending GC9A01 initialization sequence...");

  writeCommand(0xEF);
  writeCommand(0xEB); writeData(0x14);
  writeCommand(0xFE);
  writeCommand(0xEF);
  writeCommand(0xEB); writeData(0x14);
  writeCommand(0x84); writeData(0x40);
  writeCommand(0x85); writeData(0xFF);
  writeCommand(0x86); writeData(0xFF);
  writeCommand(0x87); writeData(0xFF);
  writeCommand(0x88); writeData(0x0A);
  writeCommand(0x89); writeData(0x21);
  writeCommand(0x8A); writeData(0x00);
  writeCommand(0x8B); writeData(0x80);
  writeCommand(0x8C); writeData(0x01);
  writeCommand(0x8D); writeData(0x01);
  writeCommand(0x8E); writeData(0xFF);
  writeCommand(0x8F); writeData(0xFF);

  writeCommand(0xB6); writeData(0x00); writeData(0x00);
  writeCommand(0x36); writeData(0x00); // Orientation (MADCTL)
  writeCommand(0x3A); writeData(0x05); // 16-bit RGB565

  writeCommand(0x90);
  writeData(0x08); writeData(0x08); writeData(0x08); writeData(0x08);
  writeCommand(0xBD); writeData(0x06);
  writeCommand(0xBC); writeData(0x00);
  writeCommand(0xFF); writeData(0x60); writeData(0x01); writeData(0x04);

  writeCommand(0xC3); writeData(0x13);
  writeCommand(0xC4); writeData(0x13);
  writeCommand(0xC9); writeData(0x22);
  writeCommand(0xBE); writeData(0x11);
  writeCommand(0xE1); writeData(0x10); writeData(0x0E);
  writeCommand(0xDF); writeData(0x21); writeData(0x0c); writeData(0x02);

  // Gamma settings
  writeCommand(0xF0);
  writeData(0x45); writeData(0x09); writeData(0x08); writeData(0x08); writeData(0x26); writeData(0x2A);
  writeCommand(0xF1);
  writeData(0x43); writeData(0x70); writeData(0x72); writeData(0x36); writeData(0x37); writeData(0x6F);
  writeCommand(0xF2);
  writeData(0x45); writeData(0x09); writeData(0x08); writeData(0x08); writeData(0x26); writeData(0x2A);
  writeCommand(0xF3);
  writeData(0x43); writeData(0x70); writeData(0x72); writeData(0x36); writeData(0x37); writeData(0x6F);

  writeCommand(0xED); writeData(0x1B); writeData(0x0B);
  writeCommand(0xAE); writeData(0x77);
  writeCommand(0xCD); writeData(0x63);
  writeCommand(0x70);
  writeData(0x07); writeData(0x07); writeData(0x04); writeData(0x0E); writeData(0x0F); writeData(0x09); writeData(0x07); writeData(0x08); writeData(0x03);

  writeCommand(0xE8); writeData(0x34);
  writeCommand(0x62);
  writeData(0x18); writeData(0x0D); writeData(0x71); writeData(0xED); writeData(0x70); writeData(0x70);
  writeData(0x18); writeData(0x0F); writeData(0x71); writeData(0xEF); writeData(0x70); writeData(0x70);

  writeCommand(0x63);
  writeData(0x18); writeData(0x11); writeData(0x71); writeData(0xF1); writeData(0x70); writeData(0x70);
  writeData(0x18); writeData(0x13); writeData(0x71); writeData(0xF3); writeData(0x70); writeData(0x70);

  writeCommand(0x64);
  writeData(0x28); writeData(0x29); writeData(0xF1); writeData(0x01); writeData(0xF1); writeData(0x00); writeData(0x07);

  writeCommand(0x66);
  writeData(0x3C); writeData(0x00); writeData(0xCD); writeData(0x67); writeData(0x45); writeData(0x45);
  writeData(0x10); writeData(0x00); writeData(0x00); writeData(0x00);

  writeCommand(0x67);
  writeData(0x00); writeData(0x3C); writeData(0x00); writeData(0x00); writeData(0x00); writeData(0x01);
  writeData(0x54); writeData(0x10); writeData(0x32); writeData(0x98);

  writeCommand(0x74);
  writeData(0x10); writeData(0x85); writeData(0x80); writeData(0x00); writeData(0x00); writeData(0x4E); writeData(0x00);

  writeCommand(0x98); writeData(0x3e); writeData(0x07);
  writeCommand(0x35); // Tearing effect line on
  writeCommand(0x21); // Display inversion ON (required for GC9A01)
  writeCommand(0x11); // Sleep out
  delay(120);

  writeCommand(0x29); // Display ON
  delay(20);

  Serial.println("[TFT] GC9A01 init complete!");
}
#endif

#ifdef DRIVER_ST7789
void initDisplay() {
  Serial.println("[TFT] Sending ST7789 initialization sequence...");

  writeCommand(0x01); // Software reset
  delay(150);

  writeCommand(0x11); // Sleep out
  delay(120);

  writeCommand(0x36); writeData(0x00); // MADCTL
  writeCommand(0x3A); writeData(0x05); // 16-bit RGB565
  writeCommand(0xB2); // Porch setting
  writeData(0x0C); writeData(0x0C); writeData(0x00); writeData(0x33); writeData(0x33);

  writeCommand(0xB7); writeData(0x35); // Gate control
  writeCommand(0xBB); writeData(0x19); // VCOM setting
  writeCommand(0xC0); writeData(0x2C); // LCM control
  writeCommand(0xC2); writeData(0x01); // VDV and VRH enable
  writeCommand(0xC3); writeData(0x12); // VRH set
  writeCommand(0xC4); writeData(0x20); // VDV set
  writeCommand(0xC6); writeData(0x0F); // Frame rate (60Hz)
  writeCommand(0xD0); writeData(0xA4); writeData(0xA1); // Power control

  writeCommand(0x21); // Inversion ON
  delay(10);

  writeCommand(0x29); // Display ON
  delay(50);

  Serial.println("[TFT] ST7789 init complete!");
}
#endif

// ==================== SETUP & LOOP ====================

void setup() {
  Serial.begin(115200);
  delay(400);

  Serial.println("\n==================================================");
  Serial.println("   NX-ISD v1.0 Standalone Display Test (Arduino)  ");
  Serial.println("==================================================");
  Serial.println("Pinout:");
  Serial.printf("  TFT_PWM  (Backlight) : GPIO %d (Active HIGH)\n", PIN_TFT_PWM);
  Serial.printf("  TFT_RST  (Reset)     : GPIO %d (Active LOW)\n", PIN_TFT_RST);
  Serial.printf("  TFT_DC   (Data/Cmd)  : GPIO %d\n", PIN_TFT_DC);
  Serial.printf("  CS_TFT   (Chip Sel)  : GPIO %d\n", PIN_CS_TFT);
  Serial.printf("  SPI_SCK  (Clock)     : GPIO %d\n", PIN_SPI_SCK);
  Serial.printf("  SPI_MOSI (Data)      : GPIO %d\n", PIN_SPI_MOSI);
  Serial.println("==================================================");

  // 1. Buzzer & Buttons
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  tone(PIN_BUZZER, 2000, 50); // Beep to indicate MCU booted!

  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LEVER_PUSH, INPUT_PULLUP);

  // 2. Drive Backlight Full ON
  Serial.println("[STEP 1] Driving Backlight (GPIO 1) HIGH...");
  pinMode(PIN_TFT_PWM, OUTPUT);
  digitalWrite(PIN_TFT_PWM, HIGH); // Turns Q4 ON 100%

  // 3. Setup SPI Control Pins
  pinMode(PIN_CS_TFT, OUTPUT);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect display

  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);

  // 4. Hardware Reset Pulse on TFT_RST (GPIO 38)
  Serial.println("[STEP 2] Pulsing Hardware Reset (GPIO 38)...");
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(10);
  digitalWrite(PIN_TFT_RST, LOW);  // Active LOW
  delay(50);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(120); // Wait for internal power supply stabilization

  // 5. Initialize Hardware SPI Bus on ESP32-S3
  Serial.println("[STEP 3] Starting SPI Bus...");
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_CS_TFT);
  SPI.beginTransaction(spiSettings);

  // 6. Send Controller Init Sequence
  initDisplay();

  Serial.println("[STEP 4] Display Initialized! Starting Color Loop...");
  tone(PIN_BUZZER, 2600, 100);
}

void loop() {
  // Cycle solid primary colors so you can instantly verify pixels & backlight
  Serial.println(">> Screen: RED");
  fillScreen(COLOR_RED);
  delay(1200);

  Serial.println(">> Screen: GREEN");
  fillScreen(COLOR_GREEN);
  delay(1200);

  Serial.println(">> Screen: BLUE");
  fillScreen(COLOR_BLUE);
  delay(1200);

  Serial.println(">> Screen: WHITE");
  fillScreen(COLOR_WHITE);
  delay(1200);

  Serial.println(">> Screen: BLACK");
  fillScreen(COLOR_BLACK);
  delay(1200);

  // Draw a multi-color test pattern with bars
  Serial.println(">> Screen: COLOR BARS TEST PATTERN");
  uint16_t barColors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE, COLOR_ORANGE};
  int numBars = sizeof(barColors) / sizeof(barColors[0]);
  int barWidth = TFT_WIDTH / numBars;

  for (int i = 0; i < numBars; i++) {
    fillRect(i * barWidth, 0, barWidth, TFT_HEIGHT, barColors[i]);
  }
  
  // Center black square target
  fillRect(60, 60, 120, 120, COLOR_BLACK);
  fillRect(80, 80, 80, 80, COLOR_WHITE);
  fillRect(100, 100, 40, 40, COLOR_RED);

  delay(2500);

  // If button/lever is clicked, chirp buzzer
  if (digitalRead(PIN_BTN) == LOW || digitalRead(PIN_LEVER_PUSH) == LOW) {
    tone(PIN_BUZZER, 1800, 40);
  }
}
