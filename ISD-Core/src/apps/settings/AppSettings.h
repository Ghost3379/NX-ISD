#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"
#include "../tools/UplinkBridge.h"
#include "../../storage/StorageManager.h"

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
  int selectedCategory = 0; // 0..5
  int categoryScrollOffset = 0;
  int selectedItem = 0;
  int itemScrollOffset = 0;

  static const int CATEGORY_COUNT = 6;
  const char* categoryNames[CATEGORY_COUNT] = {
    "DISPLAY",
    "NOTIFICATIONS",
    "POWER SAVE",
    "BUZZER",
    "NPM",
    "NX-AIS"
  };

  const char* npmPatternLabels[6] = { "RADAR", "RAIN", "PLASMA", "TRACER", "BREATH", "OFF" };

  bool inBrightnessDial = false;
  bool inTimeoutDial = false;
  bool inFadeDial = false;
  bool inNpmBrightnessDial = false;
  bool inBuzzerVolumeDial = false;
  bool inBuzzerDurationDial = false;
  bool inBuzzerPitchDial = false;
  bool inTiltMenu = false;
  int tiltMenuIndex = 2;
  const char* tiltLabels[4] = { "OFF", "SENSITIVE", "BALANCED", "SLUGGISH" };
  const char* hrmReminderLabels[4] = { "OFF", "30m", "1h", "2h" };

  bool isDirty = false;

  void markDirty() {
    isDirty = true;
  }

  void commitSettingsIfDirty() {
    if (isDirty) {
      StorageManager::extractConfigFromHAL(StorageManager::getActiveConfig());
      StorageManager::saveConfig(StorageManager::getActiveConfig());
      isDirty = false;
    }
  }

  void setNpmBrightnessPercent(int pct) {
    HAL::npmBrightnessPercent = constrain(pct, 10, 100);
    uint8_t pwm = (uint8_t)map(HAL::npmBrightnessPercent, 0, 100, 0, 80);
    UplinkBridge::setMatrixBrightness(pwm);
  }

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
    categoryScrollOffset = 0;
    selectedItem = 0;
    itemScrollOffset = 0;
    inBrightnessDial = false;
    inTimeoutDial = false;
    inFadeDial = false;
    inNpmBrightnessDial = false;
    inBuzzerVolumeDial = false;
    inBuzzerDurationDial = false;
    inBuzzerPitchDial = false;
    inTiltMenu = false;
    tiltMenuIndex = (int)HAL::tiltMode;
    isDirty = false;
  }

  void onExit() {
    commitSettingsIfDirty();
  }

  // Returns false when exiting root menu back to OS
  bool handleNavBtn() {
    if (inBrightnessDial) {
      inBrightnessDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inTimeoutDial) {
      inTimeoutDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inFadeDial) {
      inFadeDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inNpmBrightnessDial) {
      inNpmBrightnessDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inBuzzerVolumeDial) {
      inBuzzerVolumeDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inBuzzerDurationDial) {
      inBuzzerDurationDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inBuzzerPitchDial) {
      inBuzzerPitchDial = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (inTiltMenu) {
      inTiltMenu = false;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    if (viewMode == SETTINGS_VIEW_ITEMS) {
      viewMode = SETTINGS_VIEW_CATEGORIES;
      selectedItem = 0;
      itemScrollOffset = 0;
      HAL::buzzPip(2400, 10);
      commitSettingsIfDirty();
      return true;
    }
    HAL::buzzPip(2000, 15);
    commitSettingsIfDirty();
    return false;
  }

  void handleNavLeft() {
    if (inBrightnessDial) {
      HAL::setBrightness(HAL::brightnessPercent + 5, display);
      HAL::buzzPip(3200, 10);
      markDirty();
      return;
    }
    if (inTimeoutDial) {
      if (HAL::screenTimeoutSec < 300) {
        HAL::screenTimeoutSec += 5;
      }
      HAL::buzzPip(3200, 10);
      markDirty();
      return;
    }
    if (inFadeDial) {
      if (HAL::backlightFadeMs < 2000) {
        HAL::backlightFadeMs += 50;
      }
      HAL::buzzPip(3200, 10);
      markDirty();
      return;
    }
    if (inNpmBrightnessDial) {
      setNpmBrightnessPercent(HAL::npmBrightnessPercent + 5);
      HAL::buzzPip(3200, 10);
      markDirty();
      return;
    }
    if (inBuzzerVolumeDial) {
      if (HAL::buzzerVolumePercent < 100) {
        HAL::buzzerVolumePercent += 5;
      }
      HAL::buzzPip(3500, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inBuzzerDurationDial) {
      if (HAL::buzzerTickDurationMs < 30) {
        HAL::buzzerTickDurationMs += 2;
      }
      HAL::buzzPip(3200, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inBuzzerPitchDial) {
      if (HAL::buzzerBaseFreqHz < 4400) {
        HAL::buzzerBaseFreqHz += 100;
      }
      HAL::buzzPip(3000, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inTiltMenu) {
      tiltMenuIndex = (tiltMenuIndex + 3) % 4;
      HAL::buzzPip(3200, 10);
      markDirty();
      return;
    }
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      selectedCategory = (selectedCategory + CATEGORY_COUNT - 1) % CATEGORY_COUNT;
      if (selectedCategory < categoryScrollOffset) {
        categoryScrollOffset = selectedCategory;
      } else if (selectedCategory >= categoryScrollOffset + 4) {
        categoryScrollOffset = selectedCategory - 3;
      }
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
      markDirty();
      return;
    }
    if (inTimeoutDial) {
      if (HAL::screenTimeoutSec >= 5) {
        HAL::screenTimeoutSec -= 5;
      }
      HAL::buzzPip(2800, 10);
      markDirty();
      return;
    }
    if (inFadeDial) {
      if (HAL::backlightFadeMs >= 50) {
        HAL::backlightFadeMs -= 50;
      }
      HAL::buzzPip(2800, 10);
      markDirty();
      return;
    }
    if (inNpmBrightnessDial) {
      setNpmBrightnessPercent(HAL::npmBrightnessPercent - 5);
      HAL::buzzPip(2800, 10);
      markDirty();
      return;
    }
    if (inBuzzerVolumeDial) {
      if (HAL::buzzerVolumePercent > 10) {
        HAL::buzzerVolumePercent -= 5;
      }
      HAL::buzzPip(3500, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inBuzzerDurationDial) {
      if (HAL::buzzerTickDurationMs > 2) {
        HAL::buzzerTickDurationMs -= 2;
      }
      HAL::buzzPip(3200, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inBuzzerPitchDial) {
      if (HAL::buzzerBaseFreqHz > 1600) {
        HAL::buzzerBaseFreqHz -= 100;
      }
      HAL::buzzPip(3000, HAL::buzzerTickDurationMs);
      markDirty();
      return;
    }
    if (inTiltMenu) {
      tiltMenuIndex = (tiltMenuIndex + 1) % 4;
      HAL::buzzPip(2800, 10);
      markDirty();
      return;
    }
    if (viewMode == SETTINGS_VIEW_CATEGORIES) {
      selectedCategory = (selectedCategory + 1) % CATEGORY_COUNT;
      if (selectedCategory >= categoryScrollOffset + 4) {
        categoryScrollOffset = selectedCategory - 3;
      } else if (selectedCategory < categoryScrollOffset) {
        categoryScrollOffset = selectedCategory;
      }
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
      commitSettingsIfDirty();
      return;
    }
    if (inTimeoutDial) {
      inTimeoutDial = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
      return;
    }
    if (inFadeDial) {
      inFadeDial = false;
      HAL::buzzPip(3800, 15);
      HAL::fadeOutBacklight(display);
      HAL::fadeInBacklight(display);
      commitSettingsIfDirty();
      return;
    }
    if (inNpmBrightnessDial) {
      inNpmBrightnessDial = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
      return;
    }
    if (inBuzzerVolumeDial) {
      inBuzzerVolumeDial = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
      return;
    }
    if (inBuzzerDurationDial) {
      inBuzzerDurationDial = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
      return;
    }
    if (inBuzzerPitchDial) {
      inBuzzerPitchDial = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
      return;
    }
    if (inTiltMenu) {
      HAL::tiltMode = (TiltMode)tiltMenuIndex;
      inTiltMenu = false;
      HAL::buzzPip(3800, 15);
      commitSettingsIfDirty();
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
      } else if (selectedCategory == 0 && selectedItem == 2) {
        tiltMenuIndex = (int)HAL::tiltMode;
        inTiltMenu = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 0 && selectedItem == 3) {
        inTimeoutDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 0 && selectedItem == 5) {
        inFadeDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 3 && selectedItem == 1) {
        inBuzzerVolumeDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 3 && selectedItem == 2) {
        inBuzzerDurationDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 3 && selectedItem == 3) {
        inBuzzerPitchDial = true;
        HAL::buzzPip(3800, 15);
      } else if (selectedCategory == 4 && selectedItem == 1) {
        inNpmBrightnessDial = true;
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
    if (inFadeDial) {
      renderFadeDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inNpmBrightnessDial) {
      renderNpmBrightnessDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inBuzzerVolumeDial) {
      renderBuzzerVolumeDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inBuzzerDurationDial) {
      renderBuzzerDurationDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inBuzzerPitchDial) {
      renderBuzzerPitchDial();
      canvas->pushSprite(0, 0);
      return;
    }
    if (inTiltMenu) {
      renderTiltMenu(state);
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
      case 0: return 6; // DISPLAY: Brightness, Auto-Dim, Tilt, Timeout, Wrist Cover, Fade Anim
      case 1: return 3; // NOTIF: Method, Matrix LED, HRM Reminder
      case 2: return 3; // POWER: Eco Mode, Auto Standby, Sensor Sleep
      case 3: return 4; // BUZZER: Master Audio, Volume, Tick Duration, Tone Pitch
      case 4: return 4; // NPM: Matrix Power, Brightness, Animation, Test Pulse
      case 5: return 3; // NX-AIS: Co-Processor, Adaptive Sensing, Diagnostics
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
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 2) {
          tiltMenuIndex = (int)HAL::tiltMode;
          inTiltMenu = true;
        } else if (selectedItem == 3) {
          inTimeoutDial = true;
        } else if (selectedItem == 4) {
          HAL::wristCoverSleep = !HAL::wristCoverSleep;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 5) {
          inFadeDial = true;
        }
        break;

      case 1: // NOTIFICATIONS
        if (selectedItem == 0) {
          int nextMode = ((int)HAL::notifMode + 1) % 4;
          HAL::setNotificationMode((NotificationMode)nextMode);
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 1) {
          HAL::matrixLedAlerts = !HAL::matrixLedAlerts;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 2) {
          HAL::hrmReminderIdx = (HAL::hrmReminderIdx + 1) % 4;
          markDirty();
          commitSettingsIfDirty();
        }
        break;

      case 2: // POWER SAVE
        if (selectedItem == 0) {
          HAL::ecoMode = !HAL::ecoMode;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 1) {
          HAL::autoStandby = !HAL::autoStandby;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 2) {
          HAL::sensorSleep = !HAL::sensorSleep;
          markDirty();
          commitSettingsIfDirty();
        }
        break;

      case 3: // BUZZER
        if (selectedItem == 0) {
          HAL::silentMode = !HAL::silentMode;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 1) {
          inBuzzerVolumeDial = true;
        } else if (selectedItem == 2) {
          inBuzzerDurationDial = true;
        } else if (selectedItem == 3) {
          inBuzzerPitchDial = true;
        }
        break;

      case 4: // NPM (NeoPixel Matrix)
        if (selectedItem == 0) {
          bool curPow = UplinkBridge::getMatrixPower();
          UplinkBridge::setMatrixPower(!curPow);
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 1) {
          inNpmBrightnessDial = true;
        } else if (selectedItem == 2) {
          HAL::npmPatternIdx = (HAL::npmPatternIdx + 1) % 6;
          const MatrixPattern patterns[6] = {
            PATTERN_CYBER_RADAR,
            PATTERN_MATRIX_RAIN,
            PATTERN_SPECTRUM_PLASMA,
            PATTERN_NEON_TRACER,
            PATTERN_GLYPH_BREATH,
            PATTERN_OFF
          };
          UplinkBridge::setMatrixPattern(patterns[HAL::npmPatternIdx]);
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 3) {
          // Test pulse: flash matrix with high-vis amber pulse
          if (HAL::neoPixels && UplinkBridge::getMatrixPower()) {
            for (int p = 0; p < 16; p++) {
              HAL::neoPixels->setPixelColor(p, 255, 115, 0);
            }
            HAL::neoPixels->show();
            delay(50);
            HAL::neoPixels->clear();
            HAL::neoPixels->show();
          }
        }
        break;

      case 5: // NX-AIS
        if (selectedItem == 0) {
          HAL::nxAisCoProc = !HAL::nxAisCoProc;
          markDirty();
          commitSettingsIfDirty();
        } else if (selectedItem == 1) {
          HAL::adaptiveSensing = !HAL::adaptiveSensing;
          markDirty();
          commitSettingsIfDirty();
        }
        break;
    }
  }

  // Straight StackPanel of Category Boxes (4 visible at a time, scrollable)
  void renderCategoryStack() {
    bool hasScroll = (CATEGORY_COUNT > 4);

    int boxX = hasScroll ? 10 : 12;
    int boxW = hasScroll ? 210 : 216;
    const int boxH = 34;
    const int gap = 6;
    const int startY = 36;
    const int textYOffset = 9;
    const int visCount = 4;

    // Keep categoryScrollOffset in valid bounds
    if (selectedCategory < categoryScrollOffset) {
      categoryScrollOffset = selectedCategory;
    } else if (selectedCategory >= categoryScrollOffset + visCount) {
      categoryScrollOffset = selectedCategory - visCount + 1;
    }
    categoryScrollOffset = constrain(categoryScrollOffset, 0, max(0, CATEGORY_COUNT - visCount));

    for (int v = 0; v < visCount && (categoryScrollOffset + v) < CATEGORY_COUNT; v++) {
      int i = categoryScrollOffset + v;
      int y = startY + (v * (boxH + gap));
      bool isSelected = (i == selectedCategory);

      uint16_t boxBg = isSelected ? COLOR_ORANGE_DARK : COLOR_BG;
      uint16_t borderCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;
      uint16_t textCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;

      canvas->fillRoundRect(boxX, y, boxW, boxH, 4, boxBg);
      canvas->drawRoundRect(boxX, y, boxW, boxH, 4, borderCol);

      if (isSelected) {
        canvas->fillRoundRect(boxX + 3, y + 4, 3, boxH - 8, 2, COLOR_ORANGE_BRIGHT);
      }

      canvas->setTextColor(textCol, boxBg);
      canvas->drawString(categoryNames[i], boxX + 16, y + textYOffset, &fonts::Font2);

      // Chevron indicator
      canvas->setTextColor(isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM, boxBg);
      canvas->drawRightString(">", boxX + boxW - 12, y + textYOffset, &fonts::Font2);
    }

    // Sleek minimalist scrollbar track & thumb when CATEGORY_COUNT > 4
    if (hasScroll) {
      int trackX = 226;
      int trackY = startY;
      int trackH = (visCount * boxH) + ((visCount - 1) * gap); // 34*4 + 18 = 154px
      int trackW = 3;

      // Track groove
      canvas->fillRoundRect(trackX, trackY, trackW, trackH, 1, COLOR_ORANGE_DARK);

      // Scroll thumb
      int thumbH = max(24, (visCount * trackH) / CATEGORY_COUNT);
      int maxScroll = CATEGORY_COUNT - visCount;
      int thumbY = trackY + (categoryScrollOffset * (trackH - thumbH)) / maxScroll;

      canvas->fillRoundRect(trackX, thumbY, trackW, thumbH, 1, COLOR_ORANGE_BRIGHT);
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
        } else if (item == 5) {
          snprintf(lbl, lblLen, "FADE ANIM");
          if (HAL::backlightFadeMs == 0) {
            snprintf(val, valLen, "[OFF]");
          } else {
            snprintf(val, valLen, "[%dms]", HAL::backlightFadeMs);
          }
        }
        break;

      case 1: // NOTIFICATIONS
        if (item == 0) {
          snprintf(lbl, lblLen, "METHOD");
          const char* mLabels[4] = { "[SILENT]", "[ALL]", "[SOUND]", "[LIGHTS]" };
          snprintf(val, valLen, "%s", mLabels[(int)HAL::notifMode]);
        } else if (item == 1) {
          snprintf(lbl, lblLen, "MATRIX LED");
          snprintf(val, valLen, HAL::matrixLedAlerts ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "HRM REMINDER");
          snprintf(val, valLen, "[%s]", hrmReminderLabels[HAL::hrmReminderIdx]);
        }
        break;

      case 2: // POWER SAVE
        if (item == 0) {
          snprintf(lbl, lblLen, "ECO MODE");
          snprintf(val, valLen, HAL::ecoMode ? "[ON]" : "[OFF]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "AUTO STANDBY");
          snprintf(val, valLen, HAL::autoStandby ? "[ON]" : "[OFF]");
        } else if (item == 2) {
          snprintf(lbl, lblLen, "SENSOR SLEEP");
          snprintf(val, valLen, HAL::sensorSleep ? "[ON]" : "[OFF]");
        }
        break;

      case 3: // BUZZER
        if (item == 0) {
          snprintf(lbl, lblLen, "MASTER AUDIO");
          snprintf(val, valLen, HAL::silentMode ? "[MUTED]" : "[ACTIVE]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "VOLUME");
          snprintf(val, valLen, "[%d%%]", HAL::buzzerVolumePercent);
        } else if (item == 2) {
          snprintf(lbl, lblLen, "TICK DURATION");
          snprintf(val, valLen, "[%dms]", HAL::buzzerTickDurationMs);
        } else if (item == 3) {
          snprintf(lbl, lblLen, "TONE PITCH");
          snprintf(val, valLen, "[%dHz]", HAL::buzzerBaseFreqHz);
        }
        break;

      case 4: // NPM
        if (item == 0) {
          snprintf(lbl, lblLen, "MATRIX POWER");
          snprintf(val, valLen, UplinkBridge::getMatrixPower() ? "[ON]" : "[OFF]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "BRIGHTNESS");
          snprintf(val, valLen, "[%d%%]", HAL::npmBrightnessPercent);
        } else if (item == 2) {
          snprintf(lbl, lblLen, "ANIMATION");
          const char* patLabels[6] = { "[RADAR]", "[RAIN]", "[PLASMA]", "[TRACER]", "[BREATH]", "[OFF]" };
          snprintf(val, valLen, "%s", patLabels[HAL::npmPatternIdx]);
        } else if (item == 3) {
          snprintf(lbl, lblLen, "TEST PULSE");
          snprintf(val, valLen, "[FLASH]");
        }
        break;

      case 5: // NX-AIS
        if (item == 0) {
          snprintf(lbl, lblLen, "CO-PROCESSOR");
          snprintf(val, valLen, HAL::nxAisCoProc ? "[ACTIVE]" : "[DORMANT]");
        } else if (item == 1) {
          snprintf(lbl, lblLen, "ADAPTIVE");
          snprintf(val, valLen, HAL::adaptiveSensing ? "[ON]" : "[OFF]");
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

  void renderNpmBrightnessDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("NPM BRIGHTNESS", 82, 10);

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

      uint16_t tickColor = (p <= HAL::npmBrightnessPercent) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float curFrac = (HAL::npmBrightnessPercent - 10) / 90.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central 4x4 NeoPixel Matrix Icon
    int matY = cy - 24;
    canvas->drawRoundRect(cx - 13, matY - 4, 26, 26, 3, COLOR_ORANGE_MID);
    bool isPowered = UplinkBridge::getMatrixPower();
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        int dotX = cx - 9 + (c * 6);
        int dotY = matY + (r * 6);
        uint16_t dotCol = isPowered ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DARK;
        canvas->fillRect(dotX, dotY, 2, 2, dotCol);
      }
    }

    // 5. Large Digital Readout
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[8];
    snprintf(pBuf, sizeof(pBuf), "%d%%", HAL::npmBrightnessPercent);
    canvas->drawCenterString(pBuf, cx, cy + 3);

    // Subtitle indicating power state
    canvas->setTextSize(1);
    canvas->setTextColor(isPowered ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString(isPowered ? "4x4 MATRIX ACTIVE" : "MATRIX UNPOWERED", cx, cy + 28);

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

  void renderFadeDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("FADE ANIMATION", 82, 10);

    int cx = 120;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer & Inner Reference Track Rings
    canvas->drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // Multi-lap glow ring if > 1000ms (1 full lap = 1000ms)
    int laps = HAL::backlightFadeMs / 1000;
    if (laps >= 1 && HAL::backlightFadeMs > 1000) {
      canvas->drawCircle(cx, cy, radius + 6, COLOR_ORANGE_MID);
    }

    // 2. Full 360-degree Radial Ticks: 50 ticks (1 tick per 20ms within 1000ms lap, 7.2 deg each)
    // 0ms is at 12 o'clock (270 degrees)
    int msInDial = HAL::backlightFadeMs % 1000;
    if (HAL::backlightFadeMs > 0 && msInDial == 0) {
      msInDial = 1000;
    }
    int curTick = msInDial / 20;

    for (int t = 0; t < 50; t++) {
      float angleDeg = 270.0f + (float)t * 7.2f;
      float rad = angleDeg * 0.0174532925f;

      bool isMajor = (t % 5 == 0); // Major tick every 100ms
      float rIn = isMajor ? (radius - 10) : (radius - 5);
      float rOut = radius + 2;

      int x1 = cx + (int)roundf(cosf(rad) * rIn);
      int y1 = cy + (int)roundf(sinf(rad) * rIn);
      int x2 = cx + (int)roundf(cosf(rad) * rOut);
      int y2 = cy + (int)roundf(sinf(rad) * rOut);

      uint16_t tickColor = COLOR_ORANGE_DARK;
      if (HAL::backlightFadeMs == 0) {
        tickColor = COLOR_ORANGE_DARK;
      } else if (laps >= 1 && HAL::backlightFadeMs > 1000) {
        tickColor = (t <= curTick) ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
      } else {
        if (t <= curTick) {
          tickColor = isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
        } else {
          tickColor = COLOR_ORANGE_DARK;
        }
      }

      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float pipAngleDeg = (HAL::backlightFadeMs == 0) ? 270.0f : (270.0f + ((float)msInDial / 1000.0f) * 360.0f);
    float pipRad = pipAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(pipRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(pipRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, (HAL::backlightFadeMs == 0) ? COLOR_ORANGE_MID : COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, (HAL::backlightFadeMs == 0) ? COLOR_ORANGE_DARK : COLOR_ORANGE_MID);

    // 4. Central Backlight Aperture / Screen Lamp Glyph
    int glyY = cy - 22;
    canvas->drawRoundRect(cx - 14, glyY - 8, 28, 17, 3, COLOR_ORANGE_MID);
    canvas->drawFastHLine(cx - 5, glyY + 11, 11, COLOR_ORANGE_DIM);
    canvas->drawFastVLine(cx, glyY + 9, 2, COLOR_ORANGE_DIM);
    if (HAL::backlightFadeMs == 0) {
      canvas->setTextSize(1);
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawCenterString("OFF", cx, glyY - 3);
    } else {
      // 3 progressive luminance bars representing smooth fade
      canvas->fillRect(cx - 10, glyY - 5, 5, 11, COLOR_ORANGE_BRIGHT);
      canvas->fillRect(cx - 3, glyY - 5, 5, 11, COLOR_ORANGE_MID);
      canvas->fillRect(cx + 4, glyY - 5, 5, 11, COLOR_ORANGE_DARK);
    }

    // 5. Large Digital Readout
    if (HAL::backlightFadeMs == 0) {
      canvas->setTextSize(3);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawCenterString("OFF", cx, cy + 3);
    } else {
      canvas->setTextSize(3);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      char pBuf[16];
      snprintf(pBuf, sizeof(pBuf), "%d ms", HAL::backlightFadeMs);
      canvas->drawCenterString(pBuf, cx, cy + 3);
    }

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderBuzzerVolumeDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("BUZZER VOLUME", 82, 10);

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

      uint16_t tickColor = (p <= HAL::buzzerVolumePercent) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float curFrac = (HAL::buzzerVolumePercent - 10) / 90.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central Tactical Speaker Glyph & Soundwave Arcs
    int spkY = cy - 22;
    canvas->fillRect(cx - 12, spkY - 3, 4, 6, COLOR_ORANGE_MID);
    canvas->fillTriangle(cx - 8, spkY - 3, cx - 1, spkY - 8, cx - 1, spkY + 8, COLOR_ORANGE_MID);
    canvas->fillTriangle(cx - 8, spkY + 3, cx - 1, spkY - 8, cx - 1, spkY + 8, COLOR_ORANGE_MID);

    // Soundwaves
    int waves = (HAL::buzzerVolumePercent >= 80) ? 3 : ((HAL::buzzerVolumePercent >= 45) ? 2 : 1);
    for (int w = 0; w < 3; w++) {
      int rArc = 5 + (w * 4);
      uint16_t arcCol = (w < waves) ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DARK;
      for (int a = -45; a <= 45; a += 15) {
        float rAngle = a * 0.0174533f;
        int ax = cx + 2 + (int)roundf(cosf(rAngle) * rArc);
        int ay = spkY + (int)roundf(sinf(rAngle) * rArc);
        canvas->drawPixel(ax, ay, arcCol);
      }
    }

    // 5. Large Digital Readout
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[8];
    snprintf(pBuf, sizeof(pBuf), "%d%%", HAL::buzzerVolumePercent);
    canvas->drawCenterString(pBuf, cx, cy + 3);

    // Subtitle indicating state
    canvas->setTextSize(1);
    canvas->setTextColor(HAL::silentMode ? COLOR_ORANGE_DIM : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawCenterString(HAL::silentMode ? "[SYSTEM AUDIO MUTED]" : "PIEZO DUTY CYCLE", cx, cy + 28);

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderBuzzerDurationDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("TICK DURATION", 82, 10);

    int cx = 120;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer Reference Track Ring
    canvas->drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // 2. Technical Radial Ticks from 2ms to 30ms (15 ticks, spaced every 2ms)
    for (int d = 2; d <= 30; d += 2) {
      float frac = (d - 2) / 28.0f;
      float angleDeg = 135.0f + frac * 270.0f;
      float rad = angleDeg * 0.0174532925f;

      bool isMajor = (d % 6 == 2 || d == 30);
      float rIn = isMajor ? (radius - 10) : (radius - 5);
      float rOut = radius + 2;

      int x1 = cx + (int)roundf(cosf(rad) * rIn);
      int y1 = cy + (int)roundf(sinf(rad) * rIn);
      int x2 = cx + (int)roundf(cosf(rad) * rOut);
      int y2 = cy + (int)roundf(sinf(rad) * rOut);

      uint16_t tickColor = (d <= HAL::buzzerTickDurationMs) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float curFrac = (HAL::buzzerTickDurationMs - 2) / 28.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central Digital Square Wave Pulse Icon
    int pulY = cy - 22;
    int pulseW = map(HAL::buzzerTickDurationMs, 2, 30, 4, 24);
    int startX = cx - (pulseW / 2);
    // Baseline before pulse
    canvas->drawLine(cx - 18, pulY + 6, startX, pulY + 6, COLOR_ORANGE_DIM);
    // Leading edge
    canvas->drawLine(startX, pulY + 6, startX, pulY - 6, COLOR_ORANGE_BRIGHT);
    // Top pulse plate
    canvas->drawLine(startX, pulY - 6, startX + pulseW, pulY - 6, COLOR_ORANGE_BRIGHT);
    // Trailing edge
    canvas->drawLine(startX + pulseW, pulY - 6, startX + pulseW, pulY + 6, COLOR_ORANGE_BRIGHT);
    // Baseline after pulse
    canvas->drawLine(startX + pulseW, pulY + 6, cx + 18, pulY + 6, COLOR_ORANGE_DIM);

    // 5. Large Digital Readout
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[12];
    snprintf(pBuf, sizeof(pBuf), "%d ms", HAL::buzzerTickDurationMs);
    canvas->drawCenterString(pBuf, cx, cy + 3);

    // Subtitle
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawCenterString("HAPTIC PULSE WIDTH", cx, cy + 28);

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderBuzzerPitchDial() {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("TONE PITCH", 82, 10);

    int cx = 120;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer Reference Track Ring
    canvas->drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // 2. Technical Radial Ticks from 1600Hz to 4400Hz (15 ticks, spaced every 200Hz)
    for (int f = 1600; f <= 4400; f += 200) {
      float frac = (f - 1600) / 2800.0f;
      float angleDeg = 135.0f + frac * 270.0f;
      float rad = angleDeg * 0.0174532925f;

      bool isMajor = ((f - 1600) % 600 == 0 || f == 3000 || f == 4400);
      float rIn = isMajor ? (radius - 10) : (radius - 5);
      float rOut = radius + 2;

      int x1 = cx + (int)roundf(cosf(rad) * rIn);
      int y1 = cy + (int)roundf(sinf(rad) * rIn);
      int x2 = cx + (int)roundf(cosf(rad) * rOut);
      int y2 = cy + (int)roundf(sinf(rad) * rOut);

      uint16_t tickColor = (f <= HAL::buzzerBaseFreqHz) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas->drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip
    float curFrac = (HAL::buzzerBaseFreqHz - 1600) / 2800.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas->fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central Acoustic Frequency Sine Waveform Icon
    int wavY = cy - 22;
    float numCycles = 1.0f + (curFrac * 2.5f); // 1.0 cycle at 1600Hz up to 3.5 cycles at 4400Hz
    canvas->drawRoundRect(cx - 20, wavY - 8, 40, 17, 3, COLOR_ORANGE_DARK);
    int prevX = cx - 17;
    int prevY = wavY;
    for (int px = -16; px <= 16; px++) {
      float t = (px + 17) / 34.0f;
      float ang = t * numCycles * 6.2831853f;
      int curY = wavY + (int)roundf(sinf(ang) * 5.0f);
      int curX = cx + px;
      canvas->drawLine(prevX, prevY, curX, curY, COLOR_ORANGE_BRIGHT);
      prevX = curX;
      prevY = curY;
    }

    // 5. Large Digital Readout
    canvas->setTextSize(3);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[16];
    snprintf(pBuf, sizeof(pBuf), "%d Hz", HAL::buzzerBaseFreqHz);
    canvas->drawCenterString(pBuf, cx, cy + 3);

    // Subtitle indicating tonal register
    canvas->setTextSize(1);
    const char* toneDesc;
    if (HAL::buzzerBaseFreqHz < 2400) {
      toneDesc = "LOW PITCH // MELLOW";
    } else if (HAL::buzzerBaseFreqHz <= 3400) {
      toneDesc = "MID PITCH // BALANCED";
    } else {
      toneDesc = "HIGH PITCH // CRISP";
    }
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawCenterString(toneDesc, cx, cy + 28);

    // 6. Context Bottom Bar
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderTiltMenu(const SensorState& state) {
    canvas->fillSprite(COLOR_BG);

    // Frame & Header
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString("TILT TO WAKE", 82, 10);

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

    const char* titles[4] = { "OFF", "SENSITIVE", "BALANCED", "SLUGGISH" };
    const char* descs[4]  = {
      "MANUAL WAKE ONLY",
      "LIGHT LIFT (12-80 DEG)",
      "NATURAL TURN (22-72 DEG)",
      "DELIBERATE (35-65 DEG)"
    };

    int cardW = 216;
    int cardH = 36;
    int startX = 12;
    int startY[4] = { 34, 74, 114, 154 };

    for (int i = 0; i < 4; i++) {
      bool isSelected = (i == tiltMenuIndex);
      bool isCurrentActive = (i == (int)HAL::tiltMode);
      int cx = startX;
      int cy = startY[i];

      uint16_t cardBg = isSelected ? COLOR_ORANGE_DARK : COLOR_BG;
      uint16_t borderCol = isSelected ? COLOR_ORANGE_BRIGHT : (isCurrentActive ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM);
      uint16_t textCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
      uint16_t iconCol = isSelected ? COLOR_ORANGE_BRIGHT : (isCurrentActive ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM);

      // Card Background & Border
      canvas->fillRoundRect(cx, cy, cardW, cardH, 6, cardBg);
      canvas->drawRoundRect(cx, cy, cardW, cardH, 6, borderCol);
      if (isSelected) {
        canvas->drawRoundRect(cx + 1, cy + 1, cardW - 2, cardH - 2, 5, borderCol);
        canvas->fillRoundRect(cx + 4, cy + 6, 3, cardH - 12, 2, COLOR_ORANGE_BRIGHT);
      }

      // Radio dot if currently active mode
      if (isCurrentActive) {
        int dotX = cx + cardW - 14;
        int dotY = cy + (cardH / 2);
        canvas->fillCircle(dotX, dotY, 3, COLOR_ORANGE_BRIGHT);
        canvas->drawCircle(dotX, dotY, 5, isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
      }

      // Card Icon
      int icx = cx + 22;
      int icy = cy + (cardH / 2);

      switch (i) {
        case 0: { // OFF (Watch with diagonal slash)
          canvas->drawRoundRect(icx - 6, icy - 6, 13, 13, 2, iconCol);
          canvas->drawFastHLine(icx - 3, icy - 8, 7, iconCol);
          canvas->drawFastHLine(icx - 3, icy + 7, 7, iconCol);
          canvas->drawLine(icx - 7, icy + 7, icx + 7, icy - 7, COLOR_ORANGE_BRIGHT);
          break;
        }
        case 1: { // SENSITIVE (Watch with light flick rays)
          canvas->drawRoundRect(icx - 6, icy - 6, 13, 13, 2, iconCol);
          canvas->drawFastHLine(icx - 3, icy - 8, 7, iconCol);
          canvas->drawFastHLine(icx - 3, icy + 7, 7, iconCol);
          canvas->drawLine(icx - 4, icy + 2, icx, icy - 2, COLOR_ORANGE_BRIGHT);
          canvas->drawLine(icx + 4, icy + 2, icx, icy - 2, COLOR_ORANGE_BRIGHT);
          canvas->drawLine(icx - 2, icy, icx, icy - 2, COLOR_ORANGE_BRIGHT);
          canvas->drawLine(icx + 2, icy, icx, icy - 2, COLOR_ORANGE_BRIGHT);
          break;
        }
        case 2: { // BALANCED (Tilted watch angle arc)
          canvas->drawLine(icx - 7, icy + 6, icx + 6, icy + 6, iconCol);
          canvas->drawLine(icx - 7, icy + 6, icx + 4, icy - 5, iconCol);
          canvas->drawPixel(icx - 1, icy + 4, COLOR_ORANGE_MID);
          canvas->drawPixel(icx, icy + 3, COLOR_ORANGE_MID);
          canvas->drawPixel(icx + 1, icy + 1, COLOR_ORANGE_MID);
          canvas->fillCircle(icx + 1, icy - 2, 3, COLOR_ORANGE_BRIGHT);
          canvas->drawCircle(icx + 1, icy - 2, 4, isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
          break;
        }
        case 3: { // SLUGGISH (Steep angle with resistance mark)
          canvas->drawLine(icx - 7, icy + 6, icx + 6, icy + 6, iconCol);
          canvas->drawLine(icx - 7, icy + 6, icx - 2, icy - 6, iconCol);
          canvas->drawPixel(icx - 4, icy + 2, COLOR_ORANGE_MID);
          canvas->drawPixel(icx - 3, icy, COLOR_ORANGE_MID);
          canvas->drawRoundRect(icx + 1, icy - 5, 8, 9, 2, iconCol);
          canvas->drawFastHLine(icx + 3, icy - 1, 4, COLOR_ORANGE_BRIGHT);
          canvas->drawFastVLine(icx + 5, icy - 3, 4, COLOR_ORANGE_BRIGHT);
          break;
        }
      }

      // Title & Subtitle
      canvas->setTextSize(1);
      canvas->setTextColor(textCol, cardBg);
      canvas->drawString(titles[i], cx + 42, cy + 6, &fonts::Font2);

      canvas->setTextColor(isSelected ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM, cardBg);
      canvas->drawString(descs[i], cx + 42, cy + 22);
    }

    // Bottom Navigation Bar
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("[PUSH] SELECT   < LEVER > NAV   [BTN] BACK", 120, 220);
    canvas->setTextDatum(TL_DATUM);
  }
};
