#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "UplinkBridge.h"

enum ToolsTab {
  TAB_UPLINK = 0,
  TAB_SPIRIT_LEVEL,
  TAB_COUNT
};

class AppTools {
private:
  LGFX_Sprite* canvas = nullptr;
  LGFX* display = nullptr;

  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

  ToolsTab currentTab = TAB_UPLINK;
  float calibRoll = 0.0f;
  float calibPitch = 0.0f;
  bool calibrated = false;
  float lastPitch = 0.0f;
  float lastRoll = 0.0f;
  bool isLevelLocked = false;
  uint32_t lastPulseAnim = 0;
  bool pulseState = false;

public:
  AppTools(LGFX* tft) : display(tft) {}

  void init(LGFX_Sprite* spr, uint16_t bg, uint16_t bright, uint16_t mid, uint16_t dim, uint16_t dark) {
    canvas = spr;
    COLOR_BG = bg;
    COLOR_ORANGE_BRIGHT = bright;
    COLOR_ORANGE_MID = mid;
    COLOR_ORANGE_DIM = dim;
    COLOR_ORANGE_DARK = dark;
  }

  void onEnter() {
    currentTab = TAB_UPLINK;
    isLevelLocked = false;
  }

  void handleNavLeft() {
    if (currentTab == TAB_SPIRIT_LEVEL) {
      currentTab = TAB_UPLINK;
    }
  }

  void handleNavRight() {
    if (currentTab == TAB_UPLINK) {
      currentTab = TAB_SPIRIT_LEVEL;
    }
  }

  void handleNavPush() {
    if (currentTab == TAB_UPLINK) {
      UplinkBridge::toggleStream();
      HAL::buzzPip(UplinkBridge::isStreamActive() ? 3800 : 2200, 15);
    } else if (currentTab == TAB_SPIRIT_LEVEL) {
      if (calibrated) {
        calibPitch = 0.0f;
        calibRoll = 0.0f;
        calibrated = false;
        HAL::buzzPip(2400, 15);
      } else {
        calibPitch = lastPitch;
        calibRoll = lastRoll;
        calibrated = true;
        HAL::buzzPip(4400, 15);
      }
    }
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    lastPitch = state.pitch;
    lastRoll = state.roll;

    canvas->fillSprite(COLOR_BG);

    // 1. Cyberpunk Header Bar
    canvas->drawFastHLine(0, 22, 240, COLOR_ORANGE_DARK);
    canvas->drawFastHLine(0, 23, 240, COLOR_ORANGE_MID);

    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    canvas->setTextDatum(TL_DATUM);
    canvas->drawString("[05] TOOLS", 8, 5, &fonts::Font2);

    // Tab indicator pills in header
    int pill1X = 140, pill2X = 188;
    if (currentTab == TAB_UPLINK) {
      canvas->fillRoundRect(pill1X, 4, 44, 15, 3, COLOR_ORANGE_MID);
      canvas->setTextColor(COLOR_BG);
      canvas->setTextDatum(MC_DATUM);
      canvas->drawString("UPLINK", pill1X + 22, 11, &fonts::Font0);

      canvas->drawRoundRect(pill2X, 4, 44, 15, 3, COLOR_ORANGE_DARK);
      canvas->setTextColor(COLOR_ORANGE_DIM);
      canvas->drawString("LEVEL", pill2X + 22, 11, &fonts::Font0);
    } else {
      canvas->drawRoundRect(pill1X, 4, 44, 15, 3, COLOR_ORANGE_DARK);
      canvas->setTextColor(COLOR_ORANGE_DIM);
      canvas->setTextDatum(MC_DATUM);
      canvas->drawString("UPLINK", pill1X + 22, 11, &fonts::Font0);

      canvas->fillRoundRect(pill2X, 4, 44, 15, 3, COLOR_ORANGE_MID);
      canvas->setTextColor(COLOR_BG);
      canvas->drawString("LEVEL", pill2X + 22, 11, &fonts::Font0);
    }

    // 2. Render Active Tab Content
    if (currentTab == TAB_UPLINK) {
      renderUplinkTab(state);
    } else {
      renderSpiritTab(state);
    }

    // 3. Bottom Navigation Bar
    canvas->drawFastHLine(0, 218, 240, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->setTextDatum(MC_DATUM);

    if (currentTab == TAB_UPLINK) {
      bool streamOn = UplinkBridge::isStreamActive();
      canvas->drawString(streamOn ? "[PUSH] PAUSE STREAM   [R] LEVEL" : "[PUSH] START STREAM   [R] LEVEL", 120, 228, &fonts::Font0);
    } else {
      canvas->drawString(calibrated ? "[PUSH] RESET ZERO     [L] UPLINK" : "[PUSH] ZERO HORIZON   [L] UPLINK", 120, 228, &fonts::Font0);
    }

    canvas->pushSprite(0, 0);
  }

private:
  void renderUplinkTab(const SensorState& state) {
    uint32_t now = millis();
    if (now - lastPulseAnim > 350) {
      lastPulseAnim = now;
      pulseState = !pulseState;
    }

    // Card Frame
    canvas->drawRoundRect(6, 28, 228, 185, 4, COLOR_ORANGE_MID);
    // Corner accents
    canvas->drawFastHLine(4, 26, 12, COLOR_ORANGE_BRIGHT);
    canvas->drawFastVLine(4, 26, 12, COLOR_ORANGE_BRIGHT);
    canvas->drawFastHLine(224, 26, 12, COLOR_ORANGE_BRIGHT);
    canvas->drawFastVLine(235, 26, 12, COLOR_ORANGE_BRIGHT);

    // Ground Station Identity
    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    canvas->setTextDatum(TL_DATUM);
    canvas->drawString("NX-UPLINK GROUND STATION", 14, 34, &fonts::Font0);

    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->drawString("nx-uplink.base44.app", 14, 46, &fonts::Font0);

    // Link Status Indicator
    canvas->drawFastHLine(14, 59, 212, COLOR_ORANGE_DARK);

    bool streamOn = UplinkBridge::isStreamActive();
    if (streamOn) {
      canvas->fillCircle(20, 71, 4, pulseState ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT);
      canvas->drawString("LINK: USB-CDC 115200 (STREAMING)", 30, 67, &fonts::Font0);
    } else {
      canvas->drawCircle(20, 71, 4, COLOR_ORANGE_DIM);
      canvas->setTextColor(COLOR_ORANGE_DIM);
      canvas->drawString("LINK: USB-CDC 115200 (PAUSED)", 30, 67, &fonts::Font0);
    }

    // Live Metrics Grid
    int y = 84;
    canvas->drawRoundRect(12, y, 104, 46, 3, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->drawString("TX PACKETS", 18, y + 5, &fonts::Font0);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    char pktBuf[16];
    snprintf(pktBuf, sizeof(pktBuf), "%lu", UplinkBridge::getPacketsSent());
    canvas->drawString(pktBuf, 18, y + 20, &fonts::Font2);

    canvas->drawRoundRect(124, y, 104, 46, 3, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->drawString("STREAM RATE", 130, y + 5, &fonts::Font0);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    char rateBuf[16];
    snprintf(rateBuf, sizeof(rateBuf), "%lu Hz", UplinkBridge::getStreamRateHz());
    canvas->drawString(rateBuf, 130, y + 20, &fonts::Font2);

    // Last Received Command Card
    int yCmd = 136;
    canvas->drawRoundRect(12, yCmd, 216, 42, 3, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->drawString("LAST INCOMING COMMAND:", 18, yCmd + 5, &fonts::Font0);

    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    canvas->drawString(UplinkBridge::getLastCommand(), 18, yCmd + 20, &fonts::Font0);

    // Active Matrix Pattern
    int yMat = 184;
    canvas->setTextColor(COLOR_ORANGE_DIM);
    canvas->drawString("4x4 MATRIX ANIMATION:", 14, yMat, &fonts::Font0);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT);
    const char* patNames[] = {"OFF", "CYBER_RADAR", "MATRIX_RAIN", "SPECTRUM_PLASMA", "NEON_TRACER", "GLYPH_BREATH", "QUANTUM_RIPPLE", "CUSTOM"};
    canvas->drawString(patNames[UplinkBridge::getMatrixPattern()], 142, yMat, &fonts::Font0);
  }

  void renderSpiritTab(const SensorState& state) {
    // 2D Digital Spirit Bubble Level
    int cx = 120;
    int cy = 118;
    int maxR = 64;

    // Draw circular reticle target
    canvas->drawCircle(cx, cy, maxR, COLOR_ORANGE_MID);
    canvas->drawCircle(cx, cy, 42, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, 20, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, 8, COLOR_ORANGE_BRIGHT); // Bullseye center

    // Crosshair axis lines
    canvas->drawFastHLine(cx - maxR - 6, cy, (maxR - 20), COLOR_ORANGE_DARK);
    canvas->drawFastHLine(cx + 20, cy, (maxR - 20), COLOR_ORANGE_DARK);
    canvas->drawFastVLine(cx, cy - maxR - 6, (maxR - 20), COLOR_ORANGE_DARK);
    canvas->drawFastVLine(cx, cy + 20, (maxR - 20), COLOR_ORANGE_DARK);

    // BNO085 axes mapped to screen rotation (matching Watchface gyro dot):
    // Physical Left/Right tilt corresponds to pitch (inverted: -p -> Screen X)
    // Physical Forth/Back tilt corresponds to roll (-r -> Screen Y)
    float p = state.pitch - calibPitch;
    float r = state.roll - calibRoll;

    float tiltX = -p;
    float tiltY = -r;
    float bx = cx + (tiltX * 2.2f);
    float by = cy + (tiltY * 2.2f);

    // Clamp within reticle
    float dx = bx - cx;
    float dy = by - cy;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > (maxR - 8)) {
      bx = cx + (dx / dist) * (maxR - 8);
      by = cy + (dy / dist) * (maxR - 8);
    }

    // Check if level is locked (within ±0.8 deg on both axes)
    bool locked = (fabsf(tiltX) < 0.8f && fabsf(tiltY) < 0.8f);
    if (locked && !isLevelLocked) {
      HAL::buzzPip(4400, 15);
    }
    isLevelLocked = locked;

    // Render Spirit Bubble
    if (locked) {
      canvas->fillCircle(cx, cy, 8, COLOR_ORANGE_BRIGHT);
      canvas->drawCircle(cx, cy, 12, COLOR_ORANGE_BRIGHT);
    } else {
      canvas->fillCircle((int)bx, (int)by, 7, COLOR_ORANGE_MID);
      canvas->drawCircle((int)bx, (int)by, 8, COLOR_ORANGE_BRIGHT);
    }

    // Readout Display
    canvas->setTextDatum(MC_DATUM);
    if (locked) {
      canvas->setTextColor(COLOR_ORANGE_BRIGHT);
      canvas->drawString("[ === LEVEL LOCKED === ]", cx, 196, &fonts::Font2);
    } else {
      canvas->setTextColor(COLOR_ORANGE_BRIGHT);
      char degBuf[36];
      if (calibrated) {
        snprintf(degBuf, sizeof(degBuf), "P:%+05.1f R:%+05.1f [CAL]", p, r);
      } else {
        snprintf(degBuf, sizeof(degBuf), "PITCH: %+05.1f   ROLL: %+05.1f", p, r);
      }
      canvas->drawString(degBuf, cx, 196, &fonts::Font0);
    }
  }
};
