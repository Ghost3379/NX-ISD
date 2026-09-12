/*
  ==============================================================================
  NX-ISD v1.0 — 1.54" Rectangular ST7789V2 Display Test (3.3V Mod)
  ==============================================================================
  Display Specs:
    Type        : 1.54" Rectangular IPS TFT
    Resolution  : 240 x 240 pixels
    Controller  : Sitronix ST7789V2 / ST7789V
    Interface   : 4-Wire SPI
    Logic/Power : 3.3V (VCC modded from cut 2.8V rail)

  Pinout (from schematic):
    TFT_PWM  (Backlight)  : GPIO 1  (N-MOSFET Q4, Active HIGH)
    TFT_RST  (Reset)      : GPIO 38 (Active LOW hardware reset)
    TFT_DC   (Data/Cmd RS): GPIO 21 (Data = 1, Command = 0)
    CS_TFT   (Chip Select): GPIO 48 (Active LOW)
    SPI_SCK  (Clock)      : GPIO 40 (Hardware SPI SCK)
    SPI_MOSI (Data Out)   : GPIO 42 (Hardware SPI MOSI)
    SPI_MISO (Data In)    : GPIO 41 (Unused by LCD)
    BUZZER   (Audio)      : GPIO 10
    BTN / LEVER           : GPIO 13, 14, 15, 16

  ZERO EXTERNAL LIBRARIES REQUIRED! Uses only standard <Arduino.h> & <SPI.h>.
  ==============================================================================
*/

#include <Arduino.h>
#include <SPI.h>

// Hardware Pin Definitions
#define PIN_TFT_PWM     1   // Backlight enable via Q4 (Active HIGH)
#define PIN_TFT_RST     38  // Hardware Reset (Active LOW)
#define PIN_TFT_DC      21  // Data / Command select (RS)
#define PIN_CS_TFT      48  // SPI Chip Select (Active LOW)
#define PIN_SPI_SCK     40  // SPI Clock
#define PIN_SPI_MOSI    42  // SPI MOSI (SDA)
#define PIN_SPI_MISO    41  // SPI MISO

#define PIN_BUZZER      10  // Onboard SMD Buzzer
#define PIN_BTN         13  // Main button
#define PIN_LEVER_LEFT  14  // Lever left
#define PIN_LEVER_PUSH  15  // Lever push
#define PIN_LEVER_RIGHT 16  // Lever right

#define SCREEN_W        240
#define SCREEN_H        240

// 16-bit RGB565 Color Definitions
#define COLOR_BLACK     0x0000
#define COLOR_NAVY      0x000F
#define COLOR_BLUE      0x001F
#define COLOR_GREEN     0x07E0
#define COLOR_CYAN      0x07FF
#define COLOR_RED       0xF800
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_WHITE     0xFFFF
#define COLOR_ORANGE    0xFD20
#define COLOR_DARKGREY  0x39E7

// ST7789 framebuffers are 240x320 internally.
// On 240x240 1.54" glass, the window offset is either Y=0 or Y=80 depending on FPC bonding.
uint16_t xOffset = 0;
uint16_t yOffset = 0; // Toggleable between 0 and 80 via lever
bool invertColor = true; // ST7789 IPS panels usually require Inversion ON

// SPI Settings: 20MHz for reliable hardware testing
SPISettings spiSettings(20000000, MSBFIRST, SPI_MODE0);

// ==================== LOW-LEVEL SPI FUNCTIONS ====================

inline void writeCommand(uint8_t cmd) {
  digitalWrite(PIN_TFT_DC, LOW);   // Command mode
  digitalWrite(PIN_CS_TFT, LOW);  // Select
  SPI.transfer(cmd);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect
}

inline void writeData(uint8_t data) {
  digitalWrite(PIN_TFT_DC, HIGH);  // Data mode
  digitalWrite(PIN_CS_TFT, LOW);  // Select
  SPI.transfer(data);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect
}

void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  x0 += xOffset;
  x1 += xOffset;
  y0 += yOffset;
  y1 += yOffset;

  writeCommand(0x2A); // Column Address Set (CASET)
  writeData(x0 >> 8);
  writeData(x0 & 0xFF);
  writeData(x1 >> 8);
  writeData(x1 & 0xFF);

  writeCommand(0x2B); // Row Address Set (RASET)
  writeData(y0 >> 8);
  writeData(y0 & 0xFF);
  writeData(y1 >> 8);
  writeData(y1 & 0xFF);

  writeCommand(0x2C); // Memory Write (RAMWR)
}

void fillScreen(uint16_t color) {
  setAddrWindow(0, 0, SCREEN_W - 1, SCREEN_H - 1);

  uint8_t high = color >> 8;
  uint8_t low  = color & 0xFF;

  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_CS_TFT, LOW);

  // Transfer in fast 512-byte blocks
  const size_t bufSize = 512;
  uint8_t buf[bufSize];
  for (size_t i = 0; i < bufSize; i += 2) {
    buf[i]     = high;
    buf[i + 1] = low;
  }

  size_t totalBytes = (size_t)SCREEN_W * SCREEN_H * 2;
  while (totalBytes > 0) {
    size_t chunk = (totalBytes < bufSize) ? totalBytes : bufSize;
    SPI.transferBytes(buf, NULL, chunk);
    totalBytes -= chunk;
  }

  digitalWrite(PIN_CS_TFT, HIGH);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x >= SCREEN_W || y >= SCREEN_H) return;
  if (x + w > SCREEN_W)  w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;

  setAddrWindow(x, y, x + w - 1, y + h - 1);

  uint8_t high = color >> 8;
  uint8_t low  = color & 0xFF;

  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_CS_TFT, LOW);

  size_t totalPixels = (size_t)w * h;
  while (totalPixels--) {
    SPI.transfer(high);
    SPI.transfer(low);
  }

  digitalWrite(PIN_CS_TFT, HIGH);
}

void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x + w - 1, y, h, color);
}

// ==================== ST7789V2 INITIALIZATION ====================

void initST7789V2() {
  Serial.println("[ST7789V2] Sending initialization sequence...");

  writeCommand(0x01); // Software Reset
  delay(150);

  writeCommand(0x11); // Sleep Out
  delay(120);

  // Memory Data Access Control (MADCTL)
  // 0x00: Normal, RGB
  // 0x08: BGR color filter
  writeCommand(0x36);
  writeData(0x00);

  // Interface Pixel Format (COLMOD): 16-bit RGB565
  writeCommand(0x3A);
  writeData(0x55);

  // Porch Setting (PORCTRL)
  writeCommand(0xB2);
  writeData(0x0C);
  writeData(0x0C);
  writeData(0x00);
  writeData(0x33);
  writeData(0x33);

  // Gate Control (GCTRL)
  writeCommand(0xB7);
  writeData(0x35); // Vgh=13.26V, Vgl=-10.43V

  // VCOM Setting (VCOMS)
  writeCommand(0xBB);
  writeData(0x19); // ~1.35V

  // LCM Control
  writeCommand(0xC0);
  writeData(0x2C);

  // VDV and VRH Command Enable
  writeCommand(0xC2);
  writeData(0x01);

  // VRH Set
  writeCommand(0xC3);
  writeData(0x12);

  // VDV Set
  writeCommand(0xC4);
  writeData(0x20);

  // Frame Rate Control in Normal Mode (FRCTRL2): 60Hz
  writeCommand(0xC6);
  writeData(0x0F);

  // Power Control 1 (PWCTRL1)
  writeCommand(0xD0);
  writeData(0xA4);
  writeData(0xA1);

  // Positive Voltage Gamma Control
  writeCommand(0xE0);
  writeData(0xD0); writeData(0x04); writeData(0x0D); writeData(0x11);
  writeData(0x13); writeData(0x2B); writeData(0x3F); writeData(0x54);
  writeData(0x4C); writeData(0x18); writeData(0x0D); writeData(0x0B);
  writeData(0x1F); writeData(0x23);

  // Negative Voltage Gamma Control
  writeCommand(0xE1);
  writeData(0xD0); writeData(0x04); writeData(0x0C); writeData(0x11);
  writeData(0x13); writeData(0x2C); writeData(0x3F); writeData(0x44);
  writeData(0x51); writeData(0x2F); writeData(0x1F); writeData(0x1F);
  writeData(0x20); writeData(0x23);

  // Display Inversion ON (Required for ST7789 IPS panels)
  writeCommand(invertColor ? 0x21 : 0x20);
  delay(10);

  // Display ON
  writeCommand(0x29);
  delay(50);

  Serial.println("[ST7789V2] Init complete! Display is ON.");
}

// ==================== TEST PATTERNS ====================

// Draws a full calibration grid showing exact rectangular screen borders and corners
void drawRectangularAlignmentPattern() {
  fillScreen(COLOR_BLACK);

  // 1. Draw 1-pixel outer boundary (testing full 240x240 frame)
  drawRect(0, 0, SCREEN_W, SCREEN_H, COLOR_WHITE);
  drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, COLOR_CYAN);

  // 2. Corner identification blocks (20x20 pixels each)
  fillRect(0, 0, 20, 20, COLOR_RED);                         // Top-Left: RED
  fillRect(SCREEN_W - 20, 0, 20, 20, COLOR_GREEN);           // Top-Right: GREEN
  fillRect(0, SCREEN_H - 20, 20, 20, COLOR_BLUE);            // Bottom-Left: BLUE
  fillRect(SCREEN_W - 20, SCREEN_H - 20, 20, 20, COLOR_YELLOW); // Bottom-Right: YELLOW

  // 3. Center Crosshairs
  drawFastHLine(0, 120, SCREEN_W, COLOR_DARKGREY);
  drawFastVLine(120, 0, SCREEN_H, COLOR_DARKGREY);

  // 4. Center color swatch box
  fillRect(60, 60, 120, 120, COLOR_NAVY);
  drawRect(60, 60, 120, 120, COLOR_WHITE);
  fillRect(80, 80, 80, 80, COLOR_ORANGE);
  fillRect(100, 100, 40, 40, COLOR_WHITE);

  // 5. Diagonal corner-to-corner guide lines
  for (int i = 0; i < 40; i++) {
    fillRect(20 + i, 20 + i, 2, 2, COLOR_MAGENTA);
    fillRect(SCREEN_W - 22 - i, 20 + i, 2, 2, COLOR_CYAN);
  }

  Serial.println("[TFT] Alignment pattern drawn:");
  Serial.printf("      Offset: Y=%d | Invert: %s\n", yOffset, invertColor ? "ON" : "OFF");
  Serial.println("      Top-Left=RED, Top-Right=GREEN, Bot-Left=BLUE, Bot-Right=YELLOW");
}

void drawColorBars() {
  uint16_t bars[] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE,
    COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA,
    COLOR_WHITE, COLOR_BLACK
  };
  int count = sizeof(bars) / sizeof(bars[0]);
  int barWidth = SCREEN_W / count;

  for (int i = 0; i < count; i++) {
    fillRect(i * barWidth, 0, barWidth, SCREEN_H, bars[i]);
  }

  // Draw 2-pixel white border around entire panel
  drawRect(0, 0, SCREEN_W, SCREEN_H, COLOR_WHITE);
  drawRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, COLOR_WHITE);
}

// ==================== SETUP & LOOP ====================

void setup() {
  Serial.begin(115200);
  delay(400);

  Serial.println("\n==================================================");
  Serial.println("   NX-ISD v1.0 — 1.54\" ST7789V2 240x240 Display   ");
  Serial.println("==================================================");
  Serial.printf("  BL (Backlight)  : GPIO %d (Active HIGH)\n", PIN_TFT_PWM);
  Serial.printf("  RST (Reset)     : GPIO %d (Active LOW)\n", PIN_TFT_RST);
  Serial.printf("  DC (Data/Cmd)   : GPIO %d (RS)\n", PIN_TFT_DC);
  Serial.printf("  CS (Chip Sel)   : GPIO %d\n", PIN_CS_TFT);
  Serial.printf("  SCK (Clock)     : GPIO %d\n", PIN_SPI_SCK);
  Serial.printf("  MOSI (Data)     : GPIO %d\n", PIN_SPI_MOSI);
  Serial.printf("  BUZZER          : GPIO %d\n", PIN_BUZZER);
  Serial.println("==================================================");

  // 1. Audio Confirmation (Buzzer)
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  tone(PIN_BUZZER, 2400, 80); // Beep on boot

  // 2. Buttons & Lever Inputs (Active LOW)
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LEVER_LEFT, INPUT_PULLUP);
  pinMode(PIN_LEVER_PUSH, INPUT_PULLUP);
  pinMode(PIN_LEVER_RIGHT, INPUT_PULLUP);

  // 3. Drive Backlight Full HIGH via Q4
  Serial.println("[1/4] Powering Backlight (GPIO 1) HIGH (100% duty)...");
  pinMode(PIN_TFT_PWM, OUTPUT);
  digitalWrite(PIN_TFT_PWM, HIGH);

  // 4. SPI Control Pins
  pinMode(PIN_CS_TFT, OUTPUT);
  digitalWrite(PIN_CS_TFT, HIGH); // Deselect display initially

  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_DC, HIGH);

  // 5. Hardware Reset Pulse on GPIO 38
  Serial.println("[2/4] Executing Hardware Reset on GPIO 38...");
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(10);
  digitalWrite(PIN_TFT_RST, LOW);  // Active LOW reset
  delay(50);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(150); // Stabilization delay

  // 6. Start SPI Bus
  Serial.println("[3/4] Initializing SPI bus at 20MHz...");
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_CS_TFT);
  SPI.beginTransaction(spiSettings);

  // 7. Initialize ST7789V2
  Serial.println("[4/4] Sending ST7789V2 configuration...");
  initST7789V2();

  tone(PIN_BUZZER, 3000, 100);
  Serial.println("\n[READY] Display initialized. Starting diagnostic pattern loop.");
  Serial.println("        Controls:");
  Serial.println("          LEVER PUSH / BTN : Toggle Offset Y (0 <-> 80px)");
  Serial.println("          LEVER LEFT       : Toggle Invert (ON <-> OFF)");
  Serial.println("          LEVER RIGHT      : Toggle Backlight (ON <-> OFF)\n");
}

void loop() {
  static uint8_t testStep = 0;
  static uint32_t lastStepTime = 0;

  // --- Button & Lever Handling ---
  static bool prevPush = HIGH;
  static bool prevLeft = HIGH;
  static bool prevRight = HIGH;

  bool curPush  = digitalRead(PIN_LEVER_PUSH) && digitalRead(PIN_BTN);
  bool curLeft  = digitalRead(PIN_LEVER_LEFT);
  bool curRight = digitalRead(PIN_LEVER_RIGHT);

  // LEVER PUSH / BTN: Toggle Y-Offset (0 vs 80)
  if (curPush == LOW && prevPush == HIGH) {
    tone(PIN_BUZZER, 2200, 30);
    yOffset = (yOffset == 0) ? 80 : 0;
    Serial.printf("[CONFIG] Y-Offset changed to: %d px\n", yOffset);
    drawRectangularAlignmentPattern();
    lastStepTime = millis();
  }

  // LEVER LEFT: Toggle Inversion
  if (curLeft == LOW && prevLeft == HIGH) {
    tone(PIN_BUZZER, 1800, 30);
    invertColor = !invertColor;
    writeCommand(invertColor ? 0x21 : 0x20);
    Serial.printf("[CONFIG] Color Invert: %s\n", invertColor ? "ON" : "OFF");
  }

  // LEVER RIGHT: Toggle Backlight
  if (curRight == LOW && prevRight == HIGH) {
    static bool blState = true;
    blState = !blState;
    digitalWrite(PIN_TFT_PWM, blState ? HIGH : LOW);
    tone(PIN_BUZZER, 2600, 30);
    Serial.printf("[CONFIG] Backlight: %s\n", blState ? "ON" : "OFF");
  }

  prevPush  = curPush;
  prevLeft  = curLeft;
  prevRight = curRight;

  // --- Auto Pattern Sequence ---
  if (millis() - lastStepTime > 2000) {
    lastStepTime = millis();
    testStep = (testStep + 1) % 6;

    switch (testStep) {
      case 0:
        Serial.println(">> Screen: Solid RED");
        fillScreen(COLOR_RED);
        break;
      case 1:
        Serial.println(">> Screen: Solid GREEN");
        fillScreen(COLOR_GREEN);
        break;
      case 2:
        Serial.println(">> Screen: Solid BLUE");
        fillScreen(COLOR_BLUE);
        break;
      case 3:
        Serial.println(">> Screen: Solid WHITE");
        fillScreen(COLOR_WHITE);
        break;
      case 4:
        Serial.println(">> Screen: Color Bars Test");
        drawColorBars();
        break;
      case 5:
        Serial.println(">> Screen: 240x240 Rectangular Alignment Pattern");
        drawRectangularAlignmentPattern();
        break;
    }
  }

  delay(20);
}
