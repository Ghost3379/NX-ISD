#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "SensorState.h"
#include "HAL.h"

enum AppView {
  VIEW_NOTIFICATIONS = 0,
  VIEW_HOME          = 1,
  VIEW_QUICKPANEL    = 2
};

class Watchface {
private:
  LGFX_Sprite canvas;
  bool initialized = false;

  // View state & animated transition
  AppView currentView = VIEW_HOME;
  AppView targetView  = VIEW_HOME;
  bool isTransitioning = false;
  uint32_t transitionStartTime = 0;
  int transitionDirection = 0; // +1 = moving right, -1 = moving left
  const uint32_t transitionDuration = 150; // 150ms snappy, responsive slide

  // TVA / Flipper Zero Amber-Orange Retro-Modern Palette
  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

public:
  Watchface(LGFX* tft) : canvas(tft) {}

  void setView(AppView v) {
    currentView = v;
    targetView = v;
    isTransitioning = false;
  }

  AppView getView() const {
    return isTransitioning ? targetView : currentView;
  }

  bool isAnimating() const {
    return isTransitioning;
  }

  void startTransition(AppView target, int direction) {
    if (target == currentView && !isTransitioning) return;
    if (isTransitioning) {
      currentView = targetView;
    }
    targetView = target;
    transitionDirection = direction;
    transitionStartTime = millis();
    isTransitioning = true;
  }

  void handleNavLeft() {
    if (currentView == VIEW_QUICKPANEL) {
      startTransition(VIEW_HOME, -1);
    } else if (currentView == VIEW_HOME) {
      startTransition(VIEW_NOTIFICATIONS, -1);
    }
  }

  void handleNavRight() {
    if (currentView == VIEW_NOTIFICATIONS) {
      startTransition(VIEW_HOME, 1);
    } else if (currentView == VIEW_HOME) {
      startTransition(VIEW_QUICKPANEL, 1);
    }
  }

  void handleNavSelect() {
    if (currentView == VIEW_NOTIFICATIONS) {
      startTransition(VIEW_HOME, 1);
    } else if (currentView == VIEW_QUICKPANEL) {
      startTransition(VIEW_HOME, -1);
    }
  }

  void begin() {
    canvas.setColorDepth(16); // 16-bit RGB565
    canvas.setPsram(true);    // Allocate in 8MB PSRAM for smooth double-buffering
    canvas.createSprite(240, 240);

    // Initialize Palette
    COLOR_BG            = canvas.color565(0, 0, 0);          // Pitch Black
    COLOR_ORANGE_BRIGHT = canvas.color565(255, 115, 0);      // High-vis vibrant amber
    COLOR_ORANGE_MID    = canvas.color565(190, 75, 0);       // Mid-tone orange
    COLOR_ORANGE_DIM    = canvas.color565(75, 28, 0);        // Dim hairline borders
    COLOR_ORANGE_DARK   = canvas.color565(35, 12, 0);        // Dark grid background

    initialized = true;
  }

  void playBootAnimation() {
    if (!initialized) return;

    struct LineEntry {
      char text[56];
      uint16_t color;
    };

    LineEntry history[16];
    int historyCount = 0;

    auto pushHistory = [&](const char* text, uint16_t color) {
      if (historyCount < 16) {
        strncpy(history[historyCount].text, text, sizeof(history[historyCount].text) - 1);
        history[historyCount].text[sizeof(history[historyCount].text) - 1] = '\0';
        history[historyCount].color = color;
        historyCount++;
      }
    };

    auto redrawTerminal = [&](const char* activePrefix = nullptr, char spinnerChar = '\0') {
      canvas.fillScreen(COLOR_BG);
      canvas.setTextSize(1);
      int y = 20;

      for (int i = 0; i < historyCount; i++) {
        canvas.setTextColor(history[i].color, COLOR_BG);
        canvas.drawString(history[i].text, 12, y);
        y += 14;
      }

      if (activePrefix != nullptr) {
        canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
        canvas.drawString(activePrefix, 12, y);
        if (spinnerChar != '\0') {
          char sBuf[3] = { ' ', spinnerChar, '\0' };
          canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
          canvas.drawString(sBuf, 12 + (int)strlen(activePrefix) * 6, y);
        }
      }

      canvas.pushSprite(0, 0);
    };

    auto isInterrupted = [&]() -> bool {
      return (digitalRead(BTN) == LOW || digitalRead(LEVER_PUSH) == LOW ||
              digitalRead(LEVER_LEFT) == LOW || digitalRead(LEVER_RIGHT) == LOW);
    };

    const char spinFrames[] = { '/', '|', '\\', '-' };

    auto runSpinnerTask = [&](const char* label, const char* finalSuffix, int ticks = 6) -> bool {
      for (int t = 0; t < ticks; t++) {
        if (isInterrupted()) return false;
        redrawTerminal(label, spinFrames[t % 4]);
        delay(75);
      }
      char completedLine[56];
      snprintf(completedLine, sizeof(completedLine), "%s%s", label, finalSuffix);
      pushHistory(completedLine, COLOR_ORANGE_BRIGHT);
      redrawTerminal();
      delay(120);
      return true;
    };

    // Step 1: Initial line
    pushHistory("// Firmware start", COLOR_ORANGE_DIM);
    redrawTerminal();
    delay(200);

    // Step 2: Fetching data from Internal flash
    if (!runSpinnerTask("Fetching data from Flash...", " [OK]", 6)) return;

    // Step 3: Validating Firmware
    if (!runSpinnerTask("Validating Firmware...", " [OK]", 6)) return;
    pushHistory("    Name: ISD-Core", COLOR_ORANGE_MID);
    redrawTerminal();
    delay(100);
    pushHistory("    Version: v0p1", COLOR_ORANGE_MID);
    redrawTerminal();
    delay(140);

    // Step 4: Checking peripherals
    if (!runSpinnerTask("Checking peripherals...", " [OK]", 6)) return;
    pushHistory("    Buses: SPI @ 40MHz | I2C0", COLOR_ORANGE_MID);
    redrawTerminal();
    delay(100);
    char sensorStr[48];
    snprintf(sensorStr, sizeof(sensorStr), "    Sensors: BME680%s IMU%s",
             HAL::envSensorReady ? " [OK]" : " [--]",
             HAL::imuReady ? " [OK]" : " [--]");
    pushHistory(sensorStr, COLOR_ORANGE_MID);
    redrawTerminal();
    delay(140);

    // Step 5: Preparing UI elements
    if (!runSpinnerTask("Preparing UI elements...", " [OK]", 6)) return;
    pushHistory("    Buffer: 8MB OPI PSRAM", COLOR_ORANGE_MID);
    redrawTerminal();
    delay(100);

    // Step 6: Starting ISD-Core OS
    if (!runSpinnerTask("Starting ISD-Core OS...", " [OK]", 5)) return;
    pushHistory(">> READY", COLOR_ORANGE_BRIGHT);
    redrawTerminal();
    delay(300);

    // Brief blank screen before revealing Home
    canvas.fillScreen(COLOR_BG);
    canvas.pushSprite(0, 0);
    delay(100);
  }

  void render(const SensorState& state) {
    if (!initialized) return;

    // Calculate transition interpolation with easeOutCubic curve
    int outgoingOffsetX = 0;
    int incomingOffsetX = 0;
    float dotX = 120.0f; // Center dot position

    if (isTransitioning) {
      uint32_t elapsed = millis() - transitionStartTime;
      if (elapsed >= transitionDuration) {
        isTransitioning = false;
        currentView = targetView;
      } else {
        float progress = (float)elapsed / (float)transitionDuration;
        // Cubic ease-out: 1 - (1 - t)^3
        float inv = 1.0f - progress;
        float eased = 1.0f - (inv * inv * inv);

        if (transitionDirection > 0) {
          // Navigating Right: Outgoing slides left (0 -> -240), Incoming slides from right (+240 -> 0)
          outgoingOffsetX = (int)(-eased * 240.0f);
          incomingOffsetX = outgoingOffsetX + 240;
        } else {
          // Navigating Left: Outgoing slides right (0 -> +240), Incoming slides from left (-240 -> 0)
          outgoingOffsetX = (int)(eased * 240.0f);
          incomingOffsetX = outgoingOffsetX - 240;
        }

        // Active dot track interpolation (108 -> 120 -> 132)
        float startDot = (currentView == VIEW_NOTIFICATIONS) ? 108.0f : (currentView == VIEW_HOME ? 120.0f : 132.0f);
        float endDot   = (targetView == VIEW_NOTIFICATIONS) ? 108.0f : (targetView == VIEW_HOME ? 120.0f : 132.0f);
        dotX = startDot + (endDot - startDot) * eased;
      }
    }

    if (!isTransitioning) {
      dotX = (currentView == VIEW_NOTIFICATIONS) ? 108.0f : (currentView == VIEW_HOME ? 120.0f : 132.0f);
    }

    // 1. Clear offscreen buffer to Pure Black
    canvas.fillScreen(COLOR_BG);

    // 2. View Content Area with Viewport Clipping (Y: 29 to 220)
    canvas.setClipRect(6, 29, 228, 192);
    if (isTransitioning) {
      renderViewContent(currentView, state, outgoingOffsetX);
      renderViewContent(targetView, state, incomingOffsetX);
    } else {
      renderViewContent(currentView, state, 0);
    }
    canvas.clearClipRect();

    // 3. Header Section (Title slides with subtle parallax, Battery stays fixed)
    renderHeader(state, outgoingOffsetX, incomingOffsetX);

    // 4. Outer Technical Border Framing
    canvas.drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas.drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    // 5. Subtle Carousel Page Indicator with Smooth Gliding Active Dot
    renderPageIndicator(dotX);

    // 6. Flip Double Buffer to ST7789 (Zero Flicker)
    canvas.pushSprite(0, 0);
  }

private:
  const char* getViewTitle(AppView v) {
    switch (v) {
      case VIEW_NOTIFICATIONS: return "Notifications";
      case VIEW_QUICKPANEL:    return "Quickpanel";
      default:                 return "Home";
    }
  }

  void renderHeader(const SensorState& state, int outOffX, int inOffX) {
    canvas.setTextSize(1);

    // 1. Static Prefix: 'ISD-Core // ' stays locked in place
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawString("ISD-Core // ", 10, 10);

    // 2. Dynamic Page Title Slot (starts at x=82, clipped so it never touches prefix or battery)
    canvas.setClipRect(82, 6, 85, 20);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    if (isTransitioning) {
      canvas.drawString(getViewTitle(currentView), 82 + (int)(outOffX * 0.4f), 10);
      canvas.drawString(getViewTitle(targetView), 82 + (int)(inOffX * 0.4f), 10);
    } else {
      canvas.drawString(getViewTitle(currentView), 82, 10);
    }
    canvas.clearClipRect();

    // Battery Readout (Rock solid at top right)
    char batBuf[16];
    if (state.batPercent > 0.0f) {
      int displayPct = (int)constrain(roundf(state.batPercent), 0.0f, 100.0f);
      if (state.usbConnected || state.isCharging) {
        snprintf(batBuf, sizeof(batBuf), "%d%% [CHG]", displayPct);
      } else {
        snprintf(batBuf, sizeof(batBuf), "%d%%", displayPct);
      }
      canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    } else {
      snprintf(batBuf, sizeof(batBuf), "--%%");
      canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    }
    canvas.drawRightString(batBuf, 230, 10);
  }

  void renderViewContent(AppView view, const SensorState& state, int offsetX) {
    switch (view) {
      case VIEW_NOTIFICATIONS:
        renderNotifications(state, offsetX);
        break;
      case VIEW_HOME:
        renderHome(state, offsetX);
        break;
      case VIEW_QUICKPANEL:
        renderQuickpanel(state, offsetX);
        break;
    }
  }

  void renderHome(const SensorState& state, int offsetX) {
    // Clock Section
    const char* timeStr = (state.rtcTime[0] != '\0' && state.rtcTime[0] != '-') ? state.rtcTime : "--:--:--";
    canvas.setTextSize(3);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas.drawCenterString(timeStr, 120 + offsetX, 52);

    // Date
    const char* dateStr = (state.rtcDate[0] != '\0' && state.rtcDate[0] != '-') ? state.rtcDate : "----/--/--";
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawCenterString(dateStr, 120 + offsetX, 84);

    // Tactical Attitude Reticle
    int centerX = 120 + offsetX;
    int centerY = 156;
    int radius  = 30;

    canvas.drawCircle(centerX, centerY, radius, COLOR_ORANGE_DIM);
    canvas.drawCircle(centerX, centerY, 3, COLOR_ORANGE_MID);

    canvas.drawFastHLine(centerX - 12, centerY - 12, 24, COLOR_ORANGE_DARK);
    canvas.drawFastHLine(centerX - 12, centerY + 12, 24, COLOR_ORANGE_DARK);
    canvas.drawFastVLine(centerX, centerY - radius, 6, COLOR_ORANGE_DIM);
    canvas.drawFastVLine(centerX, centerY + radius - 6, 6, COLOR_ORANGE_DIM);

    if (state.imuDataReady) {
      float rad = -state.roll * 0.0174533f;
      int dy = (int)(state.pitch * 0.5f);
      if (dy > 20) dy = 20;
      if (dy < -20) dy = -20;

      int halfLen = 22;
      int x1 = centerX - (int)(cos(rad) * halfLen);
      int y1 = (centerY + dy) - (int)(sin(rad) * halfLen);
      int x2 = centerX + (int)(cos(rad) * halfLen);
      int y2 = (centerY + dy) + (int)(sin(rad) * halfLen);

      canvas.drawLine(x1, y1, x2, y2, COLOR_ORANGE_BRIGHT);
      canvas.fillCircle(centerX, centerY + dy, 2, COLOR_ORANGE_BRIGHT);
    } else {
      canvas.drawFastHLine(centerX - 22, centerY, 44, COLOR_ORANGE_MID);
    }

    canvas.drawFastHLine(centerX - 76, centerY, 38, COLOR_ORANGE_DIM);
    canvas.drawFastHLine(centerX + 38, centerY, 38, COLOR_ORANGE_DIM);
  }

  void renderNotifications(const SensorState& state, int offsetX) {
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawCenterString("NO NOTIFICATIONS", 120 + offsetX, 112);
    canvas.setTextColor(COLOR_ORANGE_DARK, COLOR_BG);
    canvas.drawCenterString("All caught up", 120 + offsetX, 130);
  }

  void renderQuickpanel(const SensorState& state, int offsetX) {
    canvas.setTextSize(1);

    // Tile 1: DISPLAY
    canvas.drawRoundRect(10 + offsetX, 36, 106, 54, 3, COLOR_ORANGE_DIM);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("DISPLAY", 18 + offsetX, 42);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas.drawString("ST7789 IPS", 18 + offsetX, 56);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawString("Backlight 85%", 18 + offsetX, 70);

    // Tile 2: AUDIO
    canvas.drawRoundRect(124 + offsetX, 36, 106, 54, 3, COLOR_ORANGE_DIM);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("AUDIO", 132 + offsetX, 42);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas.drawString("MUTED", 132 + offsetX, 56);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawString("Silent Mode", 132 + offsetX, 70);

    // Tile 3: POWER RAIL
    canvas.drawRoundRect(10 + offsetX, 96, 106, 54, 3, COLOR_ORANGE_DIM);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("POWER", 18 + offsetX, 102);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char voltBuf[16];
    if (state.batVoltage >= 2.0f) {
      snprintf(voltBuf, sizeof(voltBuf), "%.2f V", state.batVoltage);
    } else {
      snprintf(voltBuf, sizeof(voltBuf), "--.-- V");
    }
    canvas.drawString(voltBuf, 18 + offsetX, 116);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    if (state.isCharging) {
      canvas.drawString("Charging", 18 + offsetX, 130);
    } else if (state.usbConnected) {
      canvas.drawString("USB Power", 18 + offsetX, 130);
    } else {
      canvas.drawString("Battery", 18 + offsetX, 130);
    }

    // Tile 4: IMU TILT
    canvas.drawRoundRect(124 + offsetX, 96, 106, 54, 3, COLOR_ORANGE_DIM);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("ATTITUDE", 132 + offsetX, 102);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char imuBuf[16];
    snprintf(imuBuf, sizeof(imuBuf), "R:%.0f P:%.0f", state.roll, state.pitch);
    canvas.drawString(state.imuDataReady ? imuBuf : "STANDBY", 132 + offsetX, 116);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawString("BNO085 9-DOF", 132 + offsetX, 130);

    // Bottom Environment Status Strip
    canvas.drawRoundRect(10 + offsetX, 156, 220, 52, 3, COLOR_ORANGE_DIM);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawString("ENVIRONMENT // BME680", 18 + offsetX, 162);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char envBuf[32];
    if (state.envDataReady && state.temp > -40.0f) {
      snprintf(envBuf, sizeof(envBuf), "%.1f C  |  %.0f hPa", state.temp, state.press);
    } else {
      snprintf(envBuf, sizeof(envBuf), "--.- C  |  ---- hPa");
    }
    canvas.drawString(envBuf, 18 + offsetX, 176);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    if (state.envDataReady && state.hum > 0.0f) {
      snprintf(envBuf, sizeof(envBuf), "Humidity: %.0f%% RH", state.hum);
    } else {
      snprintf(envBuf, sizeof(envBuf), "Humidity: --%% RH");
    }
    canvas.drawString(envBuf, 18 + offsetX, 190);
  }

  void renderPageIndicator(float dotX) {
    int y = 224;
    // Static base track
    canvas.drawCircle(108, y, 2, COLOR_ORANGE_DARK);
    canvas.drawCircle(120, y, 2, COLOR_ORANGE_DARK);
    canvas.drawCircle(132, y, 2, COLOR_ORANGE_DARK);

    // Smooth gliding active dot
    canvas.fillCircle((int)(dotX + 0.5f), y, 2, COLOR_ORANGE_BRIGHT);
  }
};
