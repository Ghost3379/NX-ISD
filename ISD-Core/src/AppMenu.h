#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"

enum AppId {
  APP_VITALS = 0,
  APP_ENVIRONMENT,
  APP_CLOCK,
  APP_DEVICE,
  APP_TOOLS,
  APP_SETTINGS,
  APP_COUNT
};

class AppMenu {
private:
  LGFX_Sprite* canvas = nullptr;
  LGFX* display = nullptr;

  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

  int selectedIndex = 0;          // 0 .. APP_COUNT - 1
  float scrollPos = 0.0f;         // Continuous float position
  float targetScrollPos = 0.0f;   // Target position

  const char* appNames[APP_COUNT] = {
    "VITALS",
    "ENVIRONMENT",
    "CLOCK",
    "DEVICE",
    "TOOLS",
    "SETTINGS"
  };

  const char* appTags[APP_COUNT] = {
    "VIT",
    "ENV",
    "CLK",
    "DEV",
    "TLS",
    "SET"
  };

public:
  AppMenu(LGFX* tft) : display(tft) {}

  void init(LGFX_Sprite* spr, uint16_t bg, uint16_t bright, uint16_t mid, uint16_t dim, uint16_t dark) {
    canvas = spr;
    COLOR_BG = bg;
    COLOR_ORANGE_BRIGHT = bright;
    COLOR_ORANGE_MID = mid;
    COLOR_ORANGE_DIM = dim;
    COLOR_ORANGE_DARK = dark;
  }

  void reset() {
    selectedIndex = 0;
    scrollPos = 0.0f;
    targetScrollPos = 0.0f;
  }

  void handleNavLeft() {
    if (selectedIndex > 0) {
      selectedIndex--;
      targetScrollPos = (float)selectedIndex;
    }
  }

  void handleNavRight() {
    if (selectedIndex < APP_COUNT - 1) {
      selectedIndex++;
      targetScrollPos = (float)selectedIndex;
    }
  }

  void handleNavPush() {
    Serial.printf("[APPMENU] Selected App: %s (Index %d)\n", appNames[selectedIndex], selectedIndex);
  }

  int getSelectedIndex() const {
    return selectedIndex;
  }

  bool isAnimating() const {
    return fabsf(targetScrollPos - scrollPos) > 0.008f;
  }

  void update() {
    float diff = targetScrollPos - scrollPos;
    if (fabsf(diff) > 0.003f) {
      scrollPos += diff * 0.48f; // Crisp, instant spring response
    } else {
      scrollPos = targetScrollPos;
    }
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    // 1. Clear offscreen PSRAM canvas to Pure Black
    canvas->fillScreen(COLOR_BG);

    // 2. Viewport Clipping (Y: 29 to 232)
    canvas->setClipRect(4, 29, 232, 203);

    // 3. Render 3D Perspective Floor Grid
    renderFloorGrid();

    // 4. Render 3D App Cards (Side cards first, Hero Center card last for proper Z-ordering)
    // Pass 1: Background perspective neighbor cards (|d| >= 0.35)
    for (int i = 0; i < APP_COUNT; i++) {
      float d = (float)i - scrollPos;
      if (fabsf(d) >= 0.35f && fabsf(d) <= 1.45f) {
        renderCard(i, d);
      }
    }

    // Pass 2: Foreground center hero card (|d| < 0.35)
    for (int i = 0; i < APP_COUNT; i++) {
      float d = (float)i - scrollPos;
      if (fabsf(d) < 0.35f) {
        renderCard(i, d);
      }
    }


    canvas->clearClipRect();

    // 5. Header Section (Matched 1:1 with Watchface)
    renderHeader(state);

    // 6. Technical Border Framing
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    // 7. Flush PSRAM Double-Buffer to ST7789 IPS Display
    canvas->pushSprite(0, 0);
  }

private:
  void renderHeader(const SensorState& state) {
    canvas->setTextSize(1);

    // Static Prefix: 'ISD-Core // '
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);

    // Dynamic Title: 'APP MENU'
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("APP MENU", 82, 10);

    // Battery Readout (Rock-solid, identical to Watchface)
    char batBuf[16];
    if (state.batPercent > 0.0f) {
      int displayPct = (int)constrain(roundf(state.batPercent), 0.0f, 100.0f);
      if (state.usbConnected || state.isCharging) {
        snprintf(batBuf, sizeof(batBuf), "%d%% [CHG]", displayPct);
      } else {
        snprintf(batBuf, sizeof(batBuf), "%d%%", displayPct);
      }
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(batBuf, sizeof(batBuf), "--%%");
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas->drawRightString(batBuf, 230, 10);
  }

  void renderFloorGrid() {
    int xLeftOuter  = 22;
    int xLeftInner  = 36;
    int xRightInner = 204;
    int xRightOuter = 218;

    /* Top Cradle Tray: \ --------------------------------- / */
    int yTopOuter = 38;
    int yTopInner = 46;
    // Left backslash '\'
    canvas->drawLine(xLeftOuter, yTopOuter, xLeftInner, yTopInner, COLOR_ORANGE_DIM);
    // Center horizontal rail '------------------'
    canvas->drawFastHLine(xLeftInner, yTopInner, xRightInner - xLeftInner, COLOR_ORANGE_DIM);
    // Right slash '/'
    canvas->drawLine(xRightInner, yTopInner, xRightOuter, yTopOuter, COLOR_ORANGE_DIM);
    // Subtle vertex accent pixels
    canvas->drawPixel(xLeftInner, yTopInner, COLOR_ORANGE_MID);
    canvas->drawPixel(xRightInner, yTopInner, COLOR_ORANGE_MID);

    /* Bottom Cradle Tray: / --------------------------------- \ */
    int yBottomInner = 218;
    int yBottomOuter = 226;
    // Left slash '/'
    canvas->drawLine(xLeftOuter, yBottomOuter, xLeftInner, yBottomInner, COLOR_ORANGE_DIM);
    // Center horizontal rail '------------------'
    canvas->drawFastHLine(xLeftInner, yBottomInner, xRightInner - xLeftInner, COLOR_ORANGE_DIM);
    // Right backslash '\'
    canvas->drawLine(xRightInner, yBottomInner, xRightOuter, yBottomOuter, COLOR_ORANGE_DIM);
    // Subtle vertex accent pixels
    canvas->drawPixel(xLeftInner, yBottomInner, COLOR_ORANGE_MID);
    canvas->drawPixel(xRightInner, yBottomInner, COLOR_ORANGE_MID);
  }

  void renderCard(int index, float d) {
    int cx = 120 + (int)roundf(d * 96.0f);
    int cy = 132; // Exact mathematical center between Top Header Line (Y:28) and Bottom Frame (Y:236)
    float absD = fabsf(d);

    if (absD < 0.35f) {
      // ==================== CENTER HERO CARD ====================
      int w = 104;
      int h = 128;
      int x1 = cx - 52;
      int y1 = cy - 64;

      // Solid background fill to occlude floor grid
      canvas->fillRoundRect(x1, y1, w, h, 6, COLOR_BG);

      // Cybernetic Double Border
      canvas->drawRoundRect(x1, y1, w, h, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawRoundRect(x1 + 2, y1 + 2, w - 4, h - 4, 4, COLOR_ORANGE_DIM);

      // Glowing HUD Corner Brackets
      canvas->drawFastHLine(x1 - 3, y1 - 3, 10, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(x1 - 3, y1 - 3, 10, COLOR_ORANGE_BRIGHT);

      canvas->drawFastHLine(x1 + w - 7, y1 - 3, 10, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(x1 + w + 2, y1 - 3, 10, COLOR_ORANGE_BRIGHT);

      canvas->drawFastHLine(x1 - 3, y1 + h + 2, 10, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(x1 - 3, y1 + h - 7, 10, COLOR_ORANGE_BRIGHT);

      canvas->drawFastHLine(x1 + w - 7, y1 + h + 2, 10, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(x1 + w + 2, y1 + h - 7, 10, COLOR_ORANGE_BRIGHT);

      // Logo Area Box (60x60 px)
      canvas->drawRect(cx - 30, cy - 50, 60, 60, COLOR_ORANGE_DIM);
      canvas->drawFastHLine(cx - 30, cy - 50, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(cx - 30, cy - 50, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastHLine(cx + 24, cy - 50, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(cx + 29, cy - 50, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastHLine(cx - 30, cy + 9, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(cx - 30, cy + 4, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastHLine(cx + 24, cy + 9, 6, COLOR_ORANGE_BRIGHT);
      canvas->drawFastVLine(cx + 29, cy + 4, 6, COLOR_ORANGE_BRIGHT);

      // Draw Full-Scale Geometric Vector Logo inside
      drawGeometricLogo(index, cx, cy - 20, 1.0f, COLOR_ORANGE_BRIGHT);

      // Divider Line between Logo & App Name
      canvas->drawFastHLine(x1 + 14, cy + 20, w - 28, COLOR_ORANGE_DIM);
      canvas->drawFastHLine(cx - 10, cy + 20, 20, COLOR_ORANGE_MID);

      // App Name Inside the Card (Flanked by cyber-ticks)
      char nameBuf[32];
      snprintf(nameBuf, sizeof(nameBuf), "- %s -", appNames[index]);
      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawCenterString(nameBuf, cx, cy + 34);

    } else {
      // ==================== 3D PERSPECTIVE NEIGHBOR CARD ====================
      bool isRight = (d > 0.0f);
      float t = constrain(absD, 0.35f, 1.0f);

      // Perspective Interpolation
      float halfW = 52.0f - (t * 30.0f);      // ~22px at t=1 (Width: 44px)
      float innerH = 64.0f - (t * 16.0f);     // ~48px at t=1 (Height: 96px)
      float outerH = 64.0f - (t * 30.0f);     // ~34px at t=1 (Height: 68px)

      int x1 = cx - (int)halfW;
      int x2 = cx + (int)halfW;
      int y1t, y1b, y2t, y2b;

      if (isRight) {
        // Right neighbor tilts inward to the left (inner edge is taller)
        y1t = cy - (int)innerH;
        y1b = cy + (int)innerH;
        y2t = cy - (int)outerH;
        y2b = cy + (int)outerH;
      } else {
        // Left neighbor tilts inward to the right (inner edge is taller)
        y1t = cy - (int)outerH;
        y1b = cy + (int)outerH;
        y2t = cy - (int)innerH;
        y2b = cy + (int)innerH;
      }

      // Occluding Black Fill
      canvas->fillTriangle(x1, y1t, x2, y2t, x2, y2b, COLOR_BG);
      canvas->fillTriangle(x1, y1t, x2, y2b, x1, y1b, COLOR_BG);

      // Trapezoid 3D Perimeter Outline
      canvas->drawLine(x1, y1t, x2, y2t, COLOR_ORANGE_DIM);
      canvas->drawLine(x2, y2t, x2, y2b, COLOR_ORANGE_DIM);
      canvas->drawLine(x2, y2b, x1, y1b, COLOR_ORANGE_DIM);
      canvas->drawLine(x1, y1b, x1, y1t, COLOR_ORANGE_DIM);

      // Scaled Foreshortened Vector Logo
      drawGeometricLogo(index, cx, cy - 8, 0.62f, COLOR_ORANGE_MID);

      // Miniature App Abbreviation inside the trapezoid
      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawCenterString(appTags[index], cx, cy + 22);
    }
  }

  void drawGeometricLogo(int id, int cx, int cy, float s, uint16_t color) {
    switch (id) {
      case APP_VITALS:
        drawVitalsLogo(cx, cy, s, color);
        break;
      case APP_ENVIRONMENT:
        drawEnvironmentLogo(cx, cy, s, color);
        break;
      case APP_CLOCK:
        drawClockLogo(cx, cy, s, color);
        break;
      case APP_DEVICE:
        drawDeviceLogo(cx, cy, s, color);
        break;
      case APP_TOOLS:
        drawToolsLogo(cx, cy, s, color);
        break;
      case APP_SETTINGS:
        drawSettingsLogo(cx, cy, s, color);
        break;
    }
  }

  // 01. VITALS: Faceted Diamond Heart + Embedded EKG Pulse Waveform
  void drawVitalsLogo(int cx, int cy, float s, uint16_t color) {
    // Outer Faceted Heart Geometry
    int tX = cx;
    int bY = cy + (int)(15 * s);
    int lX = cx - (int)(15 * s);
    int rX = cx + (int)(15 * s);
    int mY = cy - (int)(5 * s);
    int tY = cy - (int)(13 * s);

    canvas->drawLine(lX, mY, cx - (int)(7 * s), tY, color);
    canvas->drawLine(cx - (int)(7 * s), tY, tX, cy - (int)(7 * s), color);
    canvas->drawLine(tX, cy - (int)(7 * s), cx + (int)(7 * s), tY, color);
    canvas->drawLine(cx + (int)(7 * s), tY, rX, mY, color);
    canvas->drawLine(rX, mY, tX, bY, color);
    canvas->drawLine(lX, mY, tX, bY, color);

    // EKG Pulse Waveform running through center
    int py = cy + (int)(1 * s);
    canvas->drawLine(cx - (int)(18 * s), py, cx - (int)(8 * s), py, COLOR_ORANGE_MID);
    canvas->drawLine(cx - (int)(8 * s), py, cx - (int)(5 * s), py + (int)(7 * s), color);
    canvas->drawLine(cx - (int)(5 * s), py + (int)(7 * s), cx + (int)(2 * s), py - (int)(12 * s), color);
    canvas->drawLine(cx + (int)(2 * s), py - (int)(12 * s), cx + (int)(6 * s), py + (int)(8 * s), color);
    canvas->drawLine(cx + (int)(6 * s), py + (int)(8 * s), cx + (int)(9 * s), py, color);
    canvas->drawLine(cx + (int)(9 * s), py, cx + (int)(18 * s), py, COLOR_ORANGE_MID);
  }

  // 02. ENVIRONMENT: Hex-Terrarium with 3-Tier Pine Chevron & Atmospheric Isobar
  void drawEnvironmentLogo(int cx, int cy, float s, uint16_t color) {
    // Tier 1 (Top Chevron)
    canvas->drawLine(cx, cy - (int)(14 * s), cx - (int)(6 * s), cy - (int)(4 * s), color);
    canvas->drawLine(cx, cy - (int)(14 * s), cx + (int)(6 * s), cy - (int)(4 * s), color);
    canvas->drawLine(cx - (int)(6 * s), cy - (int)(4 * s), cx + (int)(6 * s), cy - (int)(4 * s), COLOR_ORANGE_DIM);

    // Tier 2 (Middle Chevron)
    canvas->drawLine(cx, cy - (int)(6 * s), cx - (int)(10 * s), cy + (int)(3 * s), color);
    canvas->drawLine(cx, cy - (int)(6 * s), cx + (int)(10 * s), cy + (int)(3 * s), color);
    canvas->drawLine(cx - (int)(10 * s), cy + (int)(3 * s), cx + (int)(10 * s), cy + (int)(3 * s), COLOR_ORANGE_DIM);

    // Tier 3 (Base Chevron)
    canvas->drawLine(cx, cy + (int)(1 * s), cx - (int)(14 * s), cy + (int)(11 * s), color);
    canvas->drawLine(cx, cy + (int)(1 * s), cx + (int)(14 * s), cy + (int)(11 * s), color);
    canvas->drawLine(cx - (int)(14 * s), cy + (int)(11 * s), cx + (int)(14 * s), cy + (int)(11 * s), color);

    // Central Trunk
    canvas->drawFastVLine(cx, cy + (int)(11 * s), (int)(5 * s), color);

    // Atmospheric Isobar Arc Across Top
    canvas->drawArc(cx, cy - (int)(2 * s), (int)(18 * s), (int)(19 * s), -135, -45, COLOR_ORANGE_MID);
    // Celestial Node
    canvas->fillCircle(cx + (int)(13 * s), cy - (int)(10 * s), max(1, (int)(2 * s)), color);
  }

  // 03. CLOCK: Avionic Chronometer Dial with Quadrant Indices & Hands
  void drawClockLogo(int cx, int cy, float s, uint16_t color) {
    int r = (int)(16 * s);
    canvas->drawCircle(cx, cy, r, color);
    canvas->drawCircle(cx, cy, r - 3, COLOR_ORANGE_DARK);

    // 4 Quadrant Ticks
    canvas->drawFastVLine(cx, cy - r, 4, color);
    canvas->drawFastVLine(cx, cy + r - 3, 4, color);
    canvas->drawFastHLine(cx - r, cy, 4, color);
    canvas->drawFastHLine(cx + r - 3, cy, 4, color);

    // Chrono Hands (Hour at 2 o'clock, Minute at 10 o'clock)
    canvas->drawLine(cx, cy, cx + (int)(7 * s), cy - (int)(6 * s), color);
    canvas->drawLine(cx, cy, cx - (int)(9 * s), cy - (int)(9 * s), color);
    canvas->fillCircle(cx, cy, max(1, (int)(2 * s)), COLOR_ORANGE_BRIGHT);

    // Top Chronograph Pusher Tabs
    canvas->drawFastHLine(cx - (int)(6 * s), cy - r - 2, (int)(12 * s), COLOR_ORANGE_MID);
  }

  // 04. DEVICE: Cybernetic Hexagonal Power Core & Cathode Terminal
  void drawDeviceLogo(int cx, int cy, float s, uint16_t color) {
    // Outer Regular Hexagon
    int r = (int)(16 * s);
    for (int i = 0; i < 6; i++) {
      float a1 = (float)i * 1.04719755f;
      float a2 = (float)(i + 1) * 1.04719755f;
      int x1 = cx + (int)roundf(cosf(a1) * r);
      int y1 = cy + (int)roundf(sinf(a1) * r);
      int x2 = cx + (int)roundf(cosf(a2) * r);
      int y2 = cy + (int)roundf(sinf(a2) * r);
      canvas->drawLine(x1, y1, x2, y2, color);
    }

    // Inner Circle Core
    canvas->drawCircle(cx, cy, (int)(9 * s), COLOR_ORANGE_MID);

    // Radial Spokes
    canvas->drawFastHLine(cx - (int)(8 * s), cy, (int)(16 * s), color);
    canvas->drawFastVLine(cx, cy - (int)(8 * s), (int)(16 * s), color);

    // Center Energy Hub
    canvas->fillCircle(cx, cy, max(1, (int)(3 * s)), COLOR_ORANGE_BRIGHT);
  }

  // 05. TOOLS: Crossed Caliper & Precision Micro-Probe
  void drawToolsLogo(int cx, int cy, float s, uint16_t color) {
    // Crossed Diagonal Shafts
    canvas->drawLine(cx - (int)(14 * s), cy + (int)(14 * s), cx + (int)(14 * s), cy - (int)(14 * s), color);
    canvas->drawLine(cx + (int)(14 * s), cy + (int)(14 * s), cx - (int)(14 * s), cy - (int)(14 * s), color);

    // Caliper Jaw Prongs at Top Left
    canvas->drawLine(cx - (int)(14 * s), cy - (int)(14 * s), cx - (int)(18 * s), cy - (int)(10 * s), color);
    canvas->drawLine(cx - (int)(14 * s), cy - (int)(14 * s), cx - (int)(10 * s), cy - (int)(18 * s), color);

    // Probe Stylus Tip at Top Right
    canvas->drawCircle(cx + (int)(14 * s), cy - (int)(14 * s), 2, color);

    // Center Spirit Bubble Reticle
    canvas->drawCircle(cx, cy, (int)(7 * s), COLOR_ORANGE_MID);
    canvas->fillCircle(cx, cy, max(1, (int)(2 * s)), COLOR_ORANGE_BRIGHT);
  }

  // 06. SETTINGS: Equalizer Console Rack Sliders & Rotary Detents
  void drawSettingsLogo(int cx, int cy, float s, uint16_t color) {
    // 3 Vertical Slider Tracks
    int xLeft = cx - (int)(10 * s);
    int xMid  = cx;
    int xRight = cx + (int)(10 * s);
    int yTop  = cy - (int)(14 * s);
    int yLen  = (int)(28 * s);

    canvas->drawFastVLine(xLeft, yTop, yLen, COLOR_ORANGE_DIM);
    canvas->drawFastVLine(xMid, yTop, yLen, COLOR_ORANGE_DIM);
    canvas->drawFastVLine(xRight, yTop, yLen, COLOR_ORANGE_DIM);

    // Track 1 Knob (High)
    int k1 = cy - (int)(6 * s);
    canvas->fillRect(xLeft - 3, k1 - 2, 7, 5, COLOR_BG);
    canvas->drawRect(xLeft - 3, k1 - 2, 7, 5, color);
    canvas->drawFastHLine(xLeft - 2, k1, 5, COLOR_ORANGE_BRIGHT);

    // Track 2 Knob (Mid-Low)
    int k2 = cy + (int)(5 * s);
    canvas->fillRect(xMid - 3, k2 - 2, 7, 5, COLOR_BG);
    canvas->drawRect(xMid - 3, k2 - 2, 7, 5, color);
    canvas->drawFastHLine(xMid - 2, k2, 5, COLOR_ORANGE_BRIGHT);

    // Track 3 Knob (Mid)
    int k3 = cy - (int)(1 * s);
    canvas->fillRect(xRight - 3, k3 - 2, 7, 5, COLOR_BG);
    canvas->drawRect(xRight - 3, k3 - 2, 7, 5, color);
    canvas->drawFastHLine(xRight - 2, k3, 5, COLOR_ORANGE_BRIGHT);

    // Outer Rotary Arc Frame
    canvas->drawArc(cx, cy, (int)(18 * s), (int)(19 * s), -160, -20, COLOR_ORANGE_DARK);
    canvas->drawArc(cx, cy, (int)(18 * s), (int)(19 * s), 20, 160, COLOR_ORANGE_DARK);
  }
};
