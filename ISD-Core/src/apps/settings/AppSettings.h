#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"

enum SettingsViewMode {
  SETTINGS_VIEW_CATEGORIES = 0,
  SETTINGS_VIEW_ITEMS
};

class AppSettings {
private:
  LGFX_Sprite* canvas = nullptr;
  LGFX* display = nullptr;

  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

  SettingsViewMode viewMode = SETTINGS_VIEW_CATEGORIES;
  int selectedCategory = 0; // 0..4
  int selectedItem = 0;
  int itemScrollOffset = 0;

  static const int CATEGORY_COUNT = 5;
  const char* categoryNames[CATEGORY_COUNT] = {
    "DISPLAY",
    "NOTIFICATIONS",
    "POWER SAVE",
    "BUZZER",
    "NX-AIS"
  };

  bool inBrightnessDial = false;
  bool inTimeoutDial = false;
  const char* tiltLabels[4] = { "OFF", "SENSITIVE", "BALANCED", "SLUGGISH" };

  bool matrixLedAlerts = true;
  int hrmReminderIdx = 0; // 0:OFF, 1:30m, 2:1h, 3:2h
  const char* hrmReminderLabels[4] = { "OFF", "30m", "1h", "2h" };

  bool autoStandby = true;
  bool sensorSleep = false;

  int tickDurationIdx = 1; // 0:8ms, 1:15ms, 2:25ms
  const char* tickLabels[3] = { "8ms", "15ms", "25ms" };

  bool nxAisCoProc = false;
  bool adaptiveSensing = true;

public:
  AppSettings(LGFX* tft) : display(tft) {}

  void init(LGFX_Sprite* spr, uint16_t bg, uint16_t bright, uint16_t mid, uint16_t dim, uint16_t dark) {
    canvas = spr;
    COLOR_BG = bg;
    COLOR_ORANGE_BRIGHT = bright;
    COLOR_ORANGE_MID = mid;
    COLOR_ORANGE_DIM = dim;
    COLOR_ORANGE_DARK = dark;
  }

  void onEnter() {
    viewMode = SETTINGS_VIEW_CATEGORIES;
    selectedCategory = 0;
    selectedItem = 0;
    itemScrollOffset = 0;
  }

  void onExit() {}

  // Returns false when exiting root menu back to OS
  bool handleNavBtn() {
    if (inBrightnessDial) {
      inBrightnessDial = false;
      HAL::buzzPip(2400, 10);
      return true;
    }
    if (inTimeoutDial) {
      inTimeoutDial = false;
      HAL::buzzPip(2400, 10);
      return true;
    }
    if (viewMode == SETTINGS_VIEW_ITEMS) {
      viewMode = SETTINGS_VIEW_CATEGORIES;
      selectedItem = 0;
      itemScrollOffset = 0;
      HAL::buzzPip(2400, 10);
      return true;
    }
    HAL::buzzPip(2000, 15);
    return false;
  }

  void handleNavLeft() {
    if (inBrightnessDial) {
      HAL::setBrightness(HAL::brightnessPercent + 5, display);
      HAL::buzzPip(3200, 10);
      return;
    }
    if (inTimeoutDial) {
      if (HAL::screenTimeoutSec < 300) {
        HAL::screenTimeoutSec += 5;
      }
      HAL::buzzPip(3200, 10);
      return;
    }
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      selectedCategory = (selectedCategory + CATEGORY_COUNT - 1) % CATEGORY_COUNT;
      HAL::buzzPip(2800, 10);
    } else {
      int maxItems = getItemCount(selectedCategory);
      selectedItem = (selectedItem + maxItems - 1) % maxItems;
      if (selectedItem < itemScrollOffset) {
        itemScrollOffset = selectedItem;
      } else if (selectedItem >= itemScrollOffset + 4) {
        itemScrollOffset = selectedItem - 3;
      }
      HAL::buzzPip(2800, 10);
    }
  }

  void handleNavRight() {
    if (inBrightnessDial) {
      HAL::setBrightness(HAL::brightnessPercent - 5, display);
      HAL::buzzPip(2800, 10);
      return;
    }
    if (inTimeoutDial) {
      if (HAL::screenTimeoutSec >= 5) {
        HAL::screenTimeoutSec -= 5;
      }
      HAL::buzzPip(2800, 10);
      return;
    }
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      selectedCategory = (selectedCategory + 1) % CATEGORY_COUNT;
      HAL::buzzPip(3200, 10);
    } else {
      int maxItems = getItemCount(selectedCategory);
      selectedItem = (selectedItem + 1) % maxItems;
      if (selectedItem >= itemScrollOffset + 4) {
        itemScrollOffset = selectedItem - 3;
      } else if (selectedItem < itemScrollOffset) {
        itemScrollOffset = selectedItem;
      }
      HAL::buzzPip(3200, 10);
    }
  }

  void handleNavPush() {
    if (inBrightnessDial) {
      inBrightnessDial = false;
      HAL::buzzPip(3800, 15);
      return;
    }
    if (inTimeoutDial) {
      inTimeoutDial = false;
      HAL::buzzPip(3800, 15);
      return;
    }
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      viewMode = SETTINGS_VIEW_ITEMS;
      selectedItem = 0;
      itemScrollOffset = 0;
      HAL::buzzPip(3800, 15);
    } else {
      if (selectedCategory == 0 && selectedItem == 0) {
        inBrightnessDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 0 && selectedItem == 3) {
        inTimeoutDial = true;
        HAL::buzzPip(3800, 15);
      } else {
        toggleSelectedItem();
        HAL::buzzPip(4400, 15);
      }
    }
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    if (inBrightnessDial) {
      renderBrightnessDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inTimeoutDial) {
      renderTimeoutDial();
      canvas->pushSprite(0, 0);
      return;
    }

    canvas->fillSprite(COLOR_BG);

    // Bounding Box Framing
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    // Global ISD Header
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      canvas->drawString("SETTINGS", 82, 10);
    } else {
      canvas->drawString(categoryNames[selectedCategory], 82, 10);
    }

    // Battery Indicator
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

    // Render Content StackPanel
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      renderCategoryStack();
    } else {
      renderItemsStack();
    }

    // Bottom Navigation Bar
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      canvas->drawString("[PUSH] SELECT   < LEVER > NAV   [BTN] BACK", 120, 220);
    } else {
      canvas->drawString("[PUSH] CHANGE   < LEVER > NAV   [BTN] BACK", 120, 220);
    }
    canvas->setTextDatum(TL_DATUM);

    canvas->pushSprite(0, 0);
  }

private:
  int getItemCount(int cat) {
    switch (cat) {
      case 0: return 5; // DISPLAY: Brightness, Auto-Dim, Tilt, Timeout, Wrist Cover
      case 1: return 3; // NOTIF: Method, Matrix LED, HRM Reminder
      case 2: return 3; // POWER: Eco Mode, Auto Standby, Sensor Sleep
      case 3: return 2; // BUZZER: Master Audio, Pip Duration
      case 4: return 3; // NX-AIS: Co-Processor, Adaptive Sensing, Diagnostics
      default: return 0;
    }
  }

  void toggleSelectedItem() {
    switch (selectedCategory) {
      case 0: // DISPLAY
        if (selectedItem == 0) {
          inBrightnessDial = true;
        } else if (selectedItem == 1) {
          HAL::autoDimEnabled = !HAL::autoDimEnabled;
        } else if (selectedItem == 2) {
          int nextTilt = ((int)HAL::tiltMode + 1) % 4;
          HAL::tiltMode = (TiltMode)nextTilt;
        } else if (selectedItem == 3) {
          inTimeoutDial = true;
        } else if (selectedItem == 4) {
          HAL::wristCoverSleep = !HAL::wristCoverSleep;
        }
        break;

      case 1: // NOTIFICATIONS
        if (selectedItem == 0) {
          int nextMode = ((int)HAL::notifMode + 1) % 4;
          HAL::setNotificationMode((NotificationMode)nextMode);
        } else if (selectedItem == 1) {
          matrixLedAlerts = !matrixLedAlerts;
        } else if (selectedItem == 2) {
          hrmReminderIdx = (hrmReminderIdx + 1) % 4;
        }
        break;

      case 2: // POWER SAVE
        if (selectedItem == 0) {
          // Eco mode handled
        } else if (selectedItem == 1) {
          autoStandby = !autoStandby;
        } else if (selectedItem == 2) {
          sensorSleep = !sensorSleep;
        }
        break;

      case 3: // BUZZER
        if (selectedItem == 0) {
          HAL::silentMode = !HAL::silentMode;
        } else if (selectedItem == 1) {
          tickDurationIdx = (tickDurationIdx + 1) % 3;
        }
        break;

      case 4: // NX-AIS
        if (selectedItem == 0) {
          nxAisCoProc = !nxAisCoProc;
        } else if (selectedItem == 1) {
          adaptiveSensing = !adaptiveSensing;
        }
        break;
    }
  }

  // Straight StackPanel of Category Boxes
  void renderCategoryStack() {
    int boxX = 12;
    int boxW = 216;
    int boxH = 30;
    int startY = 36;
    int gap = 5;

    for (int i = 0; i < CATEGORY_COUNT; i++) {
      int y = startY + (i * (boxH + gap));
      bool isSelected = (i == selectedCategory);

      uint16_t boxBg = isSelected ? COLOR_ORANGE_DARK : COLOR_BG;
      uint16_t borderCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;
      uint16_t textCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;

      canvas->fillRoundRect(boxX, y, boxW, boxH, 4, boxBg);
      canvas->drawRoundRect(boxX, y, boxW, boxH, 4, borderCol);

      if (isSelected) {
        canvas->fillRoundRect(boxX + 3, y + 5, 3, boxH - 10, 2, COLOR_ORANGE_BRIGHT);
      }

      canvas->setTextColor(textCol, boxBg);
      canvas->drawString(categoryNames[i], boxX + 16, y + 7, &fonts::Font2);

      // Chevron indicator
      canvas->setTextColor(isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM, boxBg);
      canvas->drawRightString(">", boxX + boxW - 12, y + 7, &fonts::Font2);
    }
  }

  // Straight StackPanel of Setting Item Boxes (4 visible at a time, scrollable)
  void renderItemsStack() {
    int count = getItemCount(selectedCategory);
    bool hasScroll = (count > 4);

    int boxX = hasScroll ? 10 : 12;
    int boxW = hasScroll ? 210 : 216;
    const int boxH = 34;
    const int gap = 6;
    const int startY = 36;
    const int textYOffset = 9;
    const int visCount = 4;

    // Keep itemScrollOffset in valid bounds
    if (selectedItem < itemScrollOffset) {
      itemScrollOffset = selectedItem;
    } else if (selectedItem >= itemScrollOffset + visCount) {
      itemScrollOffset = selectedItem - visCount + 1;
    }
    itemScrollOffset = constrain(itemScrollOffset, 0, max(0, count - visCount));

    for (int v = 0; v < visCount && (itemScrollOffset + v) < count; v++) {
      int i = itemScrollOffset + v;
      int y = startY + (v * (boxH + gap));
      bool isSelected = (i == selectedItem);

      uint16_t boxBg = isSelected ? COLOR_ORANGE_DARK : COLOR_BG;
      uint16_t borderCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;
      uint16_t textCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;

      canvas->fillRoundRect(boxX, y, boxW, boxH, 4, boxBg);
      canvas->drawRoundRect(boxX, y, boxW, boxH, 4, borderCol);

      if (isSelected) {
        canvas->fillRoundRect(boxX + 3, y + 4, 3, boxH - 8, 2, COLOR_ORANGE_BRIGHT);
      }

      char labelBuf[32];
      char valBuf[32];
      getItemDisplay(selectedCategory, i, labelBuf, sizeof(labelBuf), valBuf, sizeof(valBuf));

      canvas->setTextColor(textCol, boxBg);
      canvas->drawString(labelBuf, boxX + 14, y + textYOffset, &fonts::Font2);

      // Value tag in brackets
      canvas->setTextColor(isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM, boxBg);
      canvas->drawRightString(valBuf, boxX + boxW - 10, y + textYOffset, &fonts::Font2);
    }

    // Sleek minimalist scrollbar track & thumb when count > 4
    if (hasScroll) {
      int trackX = 226;
      int trackY = startY;
      int trackH = (visCount * boxH) + ((visCount - 1) * gap); // 34*4 + 18 = 154px
      int trackW = 3;

      // Track groove
      canvas->fillRoundRect(trackX, trackY, trackW, trackH, 1, COLOR_ORANGE_DARK);

      // Scroll thumb
      int thumbH = max(24, (visCount * trackH) / count);
      int maxScroll = count - visCount;
      int thumbY = trackY + (itemScrollOffset * (trackH - thumbH)) / maxScroll;

      canvas->fillRoundRect(trackX, thumbY, trackW, thumbH, 1, COLOR_ORANGE_BRIGHT);
    }
  }

  void getItemDisplay(int cat, int item, char* lbl, size_t lblLen, char* val, size_t valLen) {
    lbl[0] = '\0';
    val[0] = '\0';

    switch (cat) {
      case 0: // DISPLAY
        if (item == 0) {
          snprintf(lbl, lblLen, "BRIGHTNESS");
          snprintf(val, valLen, "[%d%%]", HAL::brightnessPercent);
        } else if (item == 1) {
          snprintf(lbl, lblLen, "AUTO DIM");
          snprintf(val, valLen, HAL::autoDimEnabled ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "TILT TO WAKE");
          snprintf(val, valLen, "[%s]", tiltLabels[(int)HAL::tiltMode]);
        } else if (item == 3) {
          snprintf(lbl, lblLen, "TIMEOUT");
          if (HAL::screenTimeoutSec == 0) {
            snprintf(val, valLen, "[NEVER]");
          } else if (HAL::screenTimeoutSec < 60) {
            snprintf(val, valLen, "[%ds]", HAL::screenTimeoutSec);
          } else {
            int m = HAL::screenTimeoutSec / 60;
            int s = HAL::screenTimeoutSec % 60;
            if (s == 0) {
              snprintf(val, valLen, "[%dm]", m);
            } else {
              snprintf(val, valLen, "[%dm%ds]", m, s);
            }
          }
        } else if (item == 4) {
          snprintf(lbl, lblLen, "WRIST COVER");
          snprintf(val, valLen, HAL::wristCoverSleep ? "[ON]" : "[OFF]");
        }
        break;

      case 1: // NOTIFICATIONS
        if (item == 0) {
          snprintf(lbl, lblLen, "METHOD");
          const char* mLabels[4] = { "[SILENT]", "[ALL]", "[SOUND]", "[LIGHTS]" };
          snprintf(val, valLen, "%s", mLabels[(int)HAL::notifMode]);
        } else if (item == 1) {
          snprintf(lbl, lblLen, "MATRIX LED");
          snprintf(val, valLen, matrixLedAlerts ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "HRM REMINDER");
          snprintf(val, valLen, "[%s]", hrmReminderLabels[hrmReminderIdx]);
        }
        break;

      case 2: // POWER SAVE
        if (item == 0) {
          snprintf(lbl, lblLen, "ECO MODE");
          snprintf(val, valLen, "[OFF]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "AUTO STANDBY");
          snprintf(val, valLen, autoStandby ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "SENSOR SLEEP");
          snprintf(val, valLen, sensorSleep ? "[ON]" : "[OFF]");
        }
        break;

      case 3: // BUZZER
        if (item == 0) {
          snprintf(lbl, lblLen, "MASTER AUDIO");
          snprintf(val, valLen, HAL::silentMode ? "[MUTED]" : "[ACTIVE]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "TICK DURATION");
          snprintf(val, valLen, "[%s]", tickLabels[tickDurationIdx]);
        }
        break;

      case 4: // NX-AIS
        if (item == 0) {
          snprintf(lbl, lblLen, "CO-PROCESSOR");
          snprintf(val, valLen, nxAisCoProc ? "[ACTIVE]" : "[DORMANT]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "ADAPTIVE");
          snprintf(val, valLen, adaptiveSensing ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "DIAGNOSTICS");
          snprintf(val, valLen, "[READY]");
        }
        break;
    }
  }

  void renderBrightnessDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("BRIGHTNESS", 82, 10);

    int cx = 120;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer Reference Track Ring
    canvas->drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // 2. Technical Radial Ticks from 10% to 100% (19 ticks, spaced every 5%)
    for (int p = 10; p <= 100; p += 5) {
      float frac = (p - 10) / 90.0f;
      float angleDeg = 135.0f + frac * 270.0f;
      float rad = angleDeg * 0.0174532925f;

      bool isMajor = (p % 10 == 0);
      float rIn = isMajor ? (radius - 10) : (radius - 5);
      float rOut = radius + 2;

      int x1 = cx + (int)roundf(cosf(rad) * rIn);
      int y1 = cy + (int)roundf(sinf(rad) * rIn);
      int x2 = cx + (int)roundf(cosf(rad) * rOut);
      int y2 = cy + (int)roundf(sinf(rad) * rOut);

      uint16_t tickColor = (p <= HAL::brightnessPercent) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float curFrac = (HAL::brightnessPercent - 10) / 90.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central Sun Glyph
    int sunY = cy - 20;
    canvas->drawCircle(cx, sunY, 4, COLOR_ORANGE_BRIGHT);
    canvas->fillCircle(cx, sunY, 2, COLOR_ORANGE_BRIGHT);
    for (int r = 0; r < 8; r++) {
      float sRad = r * 0.785398f;
      int sx1 = cx + (int)roundf(cosf(sRad) * 6.0f);
      int sy1 = sunY + (int)roundf(sinf(sRad) * 6.0f);
      int sx2 = cx + (int)roundf(cosf(sRad) * 9.0f);
      int sy2 = sunY + (int)roundf(sinf(sRad) * 9.0f);
      canvas->drawLine(sx1, sy1, sx2, sy2, COLOR_ORANGE_MID);
    }

    // 5. Large Digital Readout
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[8];
    snprintf(pBuf, sizeof(pBuf), "%d%%", HAL::brightnessPercent);
    canvas->drawCenterString(pBuf, cx, cy + 2);

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderTimeoutDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("STANDBY TIMEOUT", 82, 10);

    int cx = 120;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer & Inner Reference Track Rings
    canvas->drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // Multi-lap glow ring if > 60s
    int laps = HAL::screenTimeoutSec / 60;
    if (laps >= 1 && HAL::screenTimeoutSec > 60) {
      canvas->drawCircle(cx, cy, radius + 6, COLOR_ORANGE_MID);
    }

    // 2. Full 360-degree Radial Ticks: 60 ticks (1 tick per second, 6 deg each)
    // 0s is at 12 o'clock (270 degrees)
    int secInDial = HAL::screenTimeoutSec % 60;
    if (HAL::screenTimeoutSec > 0 && secInDial == 0) {
      secInDial = 60;
    }

    for (int s = 0; s < 60; s++) {
      float angleDeg = 270.0f + (float)s * 6.0f;
      float rad = angleDeg * 0.0174532925f;

      bool isMajor = (s % 5 == 0);
      float rIn = isMajor ? (radius - 10) : (radius - 5);
      float rOut = radius + 2;

      int x1 = cx + (int)roundf(cosf(rad) * rIn);
      int y1 = cy + (int)roundf(sinf(rad) * rIn);
      int x2 = cx + (int)roundf(cosf(rad) * rOut);
      int y2 = cy + (int)roundf(sinf(rad) * rOut);

      uint16_t tickColor = COLOR_ORANGE_DARK;
      if (HAL::screenTimeoutSec == 0) {
        tickColor = COLOR_ORANGE_DARK;
      } else if (laps >= 1 && HAL::screenTimeoutSec > 60) {
        tickColor = (s <= secInDial) ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
      } else {
        if (s <= secInDial) {
          tickColor = isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
        } else {
          tickColor = COLOR_ORANGE_DARK;
        }
      }

      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip (Glowing pointer pip outside the ring)
    float pipAngleDeg = (HAL::screenTimeoutSec == 0) ? 270.0f : (270.0f + (float)secInDial * 6.0f);
    float pipRad = pipAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(pipRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(pipRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, (HAL::screenTimeoutSec == 0) ? COLOR_ORANGE_MID : COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, (HAL::screenTimeoutSec == 0) ? COLOR_ORANGE_DARK : COLOR_ORANGE_MID);

    // 4. Central Stopwatch / Timer Glyph
    int glyphY = cy - 22;
    canvas->drawCircle(cx, glyphY, 6, COLOR_ORANGE_BRIGHT);
    canvas->fillCircle(cx, glyphY, 1, COLOR_ORANGE_BRIGHT);
    canvas->drawLine(cx, glyphY, cx, glyphY - 4, COLOR_ORANGE_BRIGHT);
    canvas->drawLine(cx, glyphY, cx + 3, glyphY, COLOR_ORANGE_MID);
    canvas->drawFastHLine(cx - 2, glyphY - 8, 5, COLOR_ORANGE_MID);
    canvas->drawFastVLine(cx, glyphY - 8, 2, COLOR_ORANGE_MID);

    // 5. Large Digital Readout & Context Subtitle
    if (HAL::screenTimeoutSec == 0) {
      canvas->setTextSize(3);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawCenterString("NEVER", cx, cy - 2);
      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawCenterString("ALWAYS ON", cx, cy + 24);
    } else if (HAL::screenTimeoutSec < 60) {
      canvas->setTextSize(3);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      char pBuf[16];
      snprintf(pBuf, sizeof(pBuf), "%ds", HAL::screenTimeoutSec);
      canvas->drawCenterString(pBuf, cx, cy - 2);
      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawCenterString("SECONDS", cx, cy + 24);
    } else {
      canvas->setTextSize(3);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      char pBuf[16];
      snprintf(pBuf, sizeof(pBuf), "%ds", HAL::screenTimeoutSec);
      canvas->drawCenterString(pBuf, cx, cy - 4);

      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      int m = HAL::screenTimeoutSec / 60;
      int s = HAL::screenTimeoutSec % 60;
      char subBuf[24];
      if (s == 0) {
        snprintf(subBuf, sizeof(subBuf), "(%d MIN)", m);
      } else {
        snprintf(subBuf, sizeof(subBuf), "(%dm %02ds)", m, s);
      }
      canvas->drawCenterString(subBuf, cx, cy + 22);
    }

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }
};
