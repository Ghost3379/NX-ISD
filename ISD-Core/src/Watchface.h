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

enum PowerAction {
  PWR_ACT_NONE = 0,
  PWR_ACT_STANDBY,
  PWR_ACT_SHUTDOWN,
  PWR_ACT_RESTART
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

  // Quickpanel Concept State
  int qpFocusIndex = 0;      // 0..5 focused tile
  bool qpInTileMode = false; // whether user is navigating inside the tiles
  bool qpEcoMode = false;
  bool qpSilentMode = true;
  bool qpHrmMeasuring = false;
  uint32_t qpHrmStartMs = 0;

  // Interactive Brightness Menu & Hardware Backlight State
  LGFX* display = nullptr;
  int brightnessPercent = 85;       // 10% .. 100%
  bool qpInBrightnessMenu = false;  // Active circular menu overlay

  // Interactive Power / Shutdown Menu State
  bool qpInShutdownMenu = false;    // Active power menu overlay
  int shutdownMenuIndex = 0;        // 0: Standby, 1: Shutdown, 2: Restart
  PowerAction requestedPowerAction = PWR_ACT_NONE;
  bool isStandby = false;           // Display off standby state

public:
  Watchface(LGFX* tft) : canvas(tft), display(tft) {}

  void applyBrightness() {
    if (display) {
      uint8_t pwm = (uint8_t)map(brightnessPercent, 0, 100, 15, 255);
      display->setBrightness(pwm);
    }
  }

  int getBrightness() const {
    return brightnessPercent;
  }

  PowerAction getRequestedPowerAction() {
    PowerAction act = requestedPowerAction;
    requestedPowerAction = PWR_ACT_NONE;
    return act;
  }

  bool isInStandby() const {
    return isStandby;
  }

  void enterStandby() {
    isStandby = true;
    if (display) {
      display->setBrightness(0);
    }
  }

  void wakeFromStandby() {
    if (!isStandby) return;
    isStandby = false;
    currentView = VIEW_HOME;
    targetView = VIEW_HOME;
    isTransitioning = false;
    qpInTileMode = false;
    qpInBrightnessMenu = false;
    qpInShutdownMenu = false;
    applyBrightness();
  }

  void setView(AppView v) {
    currentView = v;
    targetView = v;
    isTransitioning = false;
    qpInTileMode = false;
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
    qpInTileMode = false;
    qpInBrightnessMenu = false;
    qpInShutdownMenu = false;
  }

  void handleNavLeft() {
    if (isTransitioning) return;
    if (currentView == VIEW_QUICKPANEL && qpInBrightnessMenu) {
      brightnessPercent = min(100, brightnessPercent + 5);
      applyBrightness();
    } else if (currentView == VIEW_QUICKPANEL && qpInShutdownMenu) {
      shutdownMenuIndex = (shutdownMenuIndex + 2) % 3;
    } else if (currentView == VIEW_QUICKPANEL && qpInTileMode) {
      qpFocusIndex = (qpFocusIndex + 5) % 6;
    } else if (currentView == VIEW_QUICKPANEL) {
      startTransition(VIEW_HOME, -1);
    } else if (currentView == VIEW_HOME) {
      startTransition(VIEW_NOTIFICATIONS, -1);
    }
  }

  void handleNavRight() {
    if (isTransitioning) return;
    if (currentView == VIEW_QUICKPANEL && qpInBrightnessMenu) {
      brightnessPercent = max(10, brightnessPercent - 5);
      applyBrightness();
    } else if (currentView == VIEW_QUICKPANEL && qpInShutdownMenu) {
      shutdownMenuIndex = (shutdownMenuIndex + 1) % 3;
    } else if (currentView == VIEW_QUICKPANEL && qpInTileMode) {
      qpFocusIndex = (qpFocusIndex + 1) % 6;
    } else if (currentView == VIEW_NOTIFICATIONS) {
      startTransition(VIEW_HOME, 1);
    } else if (currentView == VIEW_HOME) {
      startTransition(VIEW_QUICKPANEL, 1);
    }
  }

  void handleNavPush() {
    if (isTransitioning) return;
    if (currentView == VIEW_HOME) {
      // Pushing lever from Home slides to Quickpanel and enters it!
      startTransition(VIEW_QUICKPANEL, 1);
      qpInTileMode = true;
      qpFocusIndex = 0;
    } else if (currentView == VIEW_QUICKPANEL) {
      if (qpInShutdownMenu) {
        // Confirm and trigger selected power action
        switch (shutdownMenuIndex) {
          case 0:
            requestedPowerAction = PWR_ACT_STANDBY;
            break;
          case 1:
            // Shutdown functionality removed/disabled for now per user request
            requestedPowerAction = PWR_ACT_NONE;
            break;
          case 2:
            requestedPowerAction = PWR_ACT_RESTART;
            break;
        }
        qpInShutdownMenu = false;
      } else if (qpInBrightnessMenu) {
        // Exit circular brightness menu and confirm level
        qpInBrightnessMenu = false;
      } else if (!qpInTileMode) {
        // Enter tile navigation mode
        qpInTileMode = true;
        qpFocusIndex = 0;
      } else {
        // Toggle or activate the selected tile
        switch (qpFocusIndex) {
          case 0: // Settings
            break;
          case 1: // HRM
            qpHrmMeasuring = !qpHrmMeasuring;
            qpHrmStartMs = millis();
            break;
          case 2: // Eco Mode
            qpEcoMode = !qpEcoMode;
            break;
          case 3: // Display / Brightness circular menu
            qpInBrightnessMenu = true;
            break;
          case 4: // Silent
            qpSilentMode = !qpSilentMode;
            break;
          case 5: // Shutdown / Power Menu
            qpInShutdownMenu = true;
            shutdownMenuIndex = 0;
            break;
        }
      }
    } else if (currentView == VIEW_NOTIFICATIONS) {
      // Enter notifications
    }
  }

  void handleNavBack() {
    if (isTransitioning) return;
    if (currentView == VIEW_QUICKPANEL) {
      if (qpInShutdownMenu) {
        qpInShutdownMenu = false;
      } else if (qpInBrightnessMenu) {
        qpInBrightnessMenu = false;
      } else if (qpInTileMode) {
        qpInTileMode = false;
      } else {
        startTransition(VIEW_HOME, -1);
      }
    } else if (currentView == VIEW_NOTIFICATIONS) {
      startTransition(VIEW_HOME, 1);
    }
  }

  void handleNavSelect() {
    handleNavPush();
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

    applyBrightness();
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
    pushHistory("    Version: v0p2", COLOR_ORANGE_MID);
    redrawTerminal();
    delay(140);

    // Step 4: Checking peripherals
    if (!runSpinnerTask("Checking peripherals...", " [OK]", 6)) return;
    pushHistory("    Buses: SPI @ 24MHz | I2C0", COLOR_ORANGE_MID);
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

  void renderPowerMessage(const char* title, const char* subtitle, int iconType) {
    canvas.fillScreen(COLOR_BG);

    int cx = 120;
    int cy = 95;

    // Outer Avionic Frame
    canvas.drawRoundRect(16, 24, 208, 172, 10, COLOR_ORANGE_BRIGHT);
    canvas.drawRoundRect(18, 26, 204, 168, 8, COLOR_ORANGE_DARK);

    // Icon
    if (iconType == 1) { // Power off
      canvas.drawCircle(cx, cy - 20, 20, COLOR_ORANGE_BRIGHT);
      canvas.drawCircle(cx, cy - 20, 19, COLOR_ORANGE_BRIGHT);
      canvas.fillRect(cx - 5, cy - 42, 10, 8, COLOR_BG);
      canvas.fillRect(cx - 2, cy - 40, 4, 18, COLOR_ORANGE_BRIGHT);
    } else if (iconType == 2) { // Restart
      canvas.drawCircle(cx, cy - 20, 20, COLOR_ORANGE_BRIGHT);
      canvas.drawCircle(cx, cy - 20, 19, COLOR_ORANGE_BRIGHT);
      canvas.fillRect(cx, cy - 42, 18, 16, COLOR_BG);
      canvas.fillTriangle(cx + 12, cy - 42, cx + 22, cy - 20, cx + 6, cy - 25, COLOR_ORANGE_BRIGHT);
    }

    canvas.setTextSize(2);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas.drawCenterString(title, cx, cy + 20);

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawCenterString(subtitle, cx, cy + 50);

    // Direct flush to screen
    if (display) {
      display->startWrite();
      canvas.pushSprite(0, 0);
      display->endWrite();
    }
  }

private:
  const char* getViewTitle(AppView v) {
    if (v == VIEW_QUICKPANEL && qpInShutdownMenu) return "Power Menu";
    if (v == VIEW_QUICKPANEL && qpInBrightnessMenu) return "Brightness";
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
    canvas.drawCenterString(timeStr, 120 + offsetX, 44);

    // Date
    const char* dateStr = (state.rtcDate[0] != '\0' && state.rtcDate[0] != '-') ? state.rtcDate : "----/--/--";
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawCenterString(dateStr, 120 + offsetX, 74);

    // Multipurpose Instrument Dial (Reticle + Spirit Level + Rotating Compass + Ambient Temp)
    int centerX = 120 + offsetX;
    int centerY = 152;
    int radius  = 40;

    // 1. Outer Dial Ring
    canvas.drawCircle(centerX, centerY, radius, COLOR_ORANGE_DIM);

    // 2. Subtle Reference Lines (avionic pitch / horizon rungs)
    canvas.drawFastHLine(centerX - 14, centerY - 14, 28, COLOR_ORANGE_DARK);
    canvas.drawFastHLine(centerX - 14, centerY + 14, 28, COLOR_ORANGE_DARK);

    // 3. Rotating Compass Axis with 'W' and 'E' Wings + North Indicator
    float headingRad = -1.5707963f; // Default North pointing UP (-90 deg)
    if (state.imuDataReady) {
      // Clockwise watch rotation decreases yaw, so (state.yaw - 90°) decreases angle (rotates counter-clockwise)
      // to keep pointing to true physical North!
      headingRad = (state.yaw - 90.0f) * 0.0174532925f;
    }

    // North angle, East angle (+90° CW), and West angle (+180° opposite East)
    float thetaN = headingRad;
    float thetaE = headingRad + 1.5707963f;
    float thetaW = thetaE + 3.14159265f;

    float cosE = cosf(thetaE);
    float sinE = sinf(thetaE);
    float cosW = -cosE;
    float sinW = -sinE;

    // A single continuous line through the middle from West wing to East wing (no arrow)
    int wx = centerX + (int)roundf(cosW * 52.0f);
    int wy = centerY + (int)roundf(sinW * 52.0f);
    int ex = centerX + (int)roundf(cosE * 52.0f);
    int ey = centerY + (int)roundf(sinE * 52.0f);

    // Draw the continuous line
    canvas.drawLine(wx, wy, ex, ey, COLOR_ORANGE_MID);

    // Cross-ticks at the wing ends (perpendicular to line, length 5 px)
    float perpX = -sinE * 2.5f;
    float perpY =  cosE * 2.5f;
    canvas.drawLine(wx - (int)perpX, wy - (int)perpY, wx + (int)perpX, wy + (int)perpY, COLOR_ORANGE_DIM);
    canvas.drawLine(ex - (int)perpX, ey - (int)perpY, ex + (int)perpX, ey + (int)perpY, COLOR_ORANGE_DIM);

    // 'W' and 'E' markings rotating dynamically with the wings
    int wLabelX = centerX + (int)roundf(cosW * 59.0f);
    int wLabelY = centerY + (int)roundf(sinW * 59.0f);
    int eLabelX = centerX + (int)roundf(cosE * 59.0f);
    int eLabelY = centerY + (int)roundf(sinE * 59.0f);

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas.drawCenterString("W", wLabelX, wLabelY - 3);
    canvas.drawCenterString("E", eLabelX, eLabelY - 3);

    // North Indicator: small prominent tick line crossing the circle rim where North is
    float cosN = cosf(thetaN);
    float sinN = sinf(thetaN);
    int nInX  = centerX + (int)roundf(cosN * 34.0f);
    int nInY  = centerY + (int)roundf(sinN * 34.0f);
    int nOutX = centerX + (int)roundf(cosN * 44.0f);
    int nOutY = centerY + (int)roundf(sinN * 44.0f);
    canvas.drawLine(nInX, nInY, nOutX, nOutY, COLOR_ORANGE_BRIGHT);

    // 4. Center Reference Hub / Level Target
    canvas.drawCircle(centerX, centerY, 5, COLOR_ORANGE_DARK);

    // 5. Active 2D Spirit Level Bubble (Tilts in 2D with IMU roll & pitch)
    if (state.imuDataReady) {
      float bx = state.roll * 0.75f;
      float by = state.pitch * 0.75f;
      float dist = sqrtf(bx * bx + by * by);
      if (dist > 22.0f) {
        bx = (bx / dist) * 22.0f;
        by = (by / dist) * 22.0f;
      }
      int bubbleX = centerX + (int)roundf(bx);
      int bubbleY = centerY + (int)roundf(by);

      canvas.fillCircle(bubbleX, bubbleY, 3, COLOR_ORANGE_BRIGHT);
      canvas.drawCircle(bubbleX, bubbleY, 4, COLOR_ORANGE_MID);
    } else {
      canvas.fillCircle(centerX, centerY, 3, COLOR_ORANGE_MID);
    }

    // 6. Ambient Temperature Readout (Cleanly embedded in lower crescent of circle)
    char tempBuf[16];
    if (state.envDataReady && state.temp > -40.0f) {
      snprintf(tempBuf, sizeof(tempBuf), "%.1f", state.temp);
    } else {
      snprintf(tempBuf, sizeof(tempBuf), "--.-");
    }

    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    int tWidth = canvas.textWidth(tempBuf);
    int totalW = tWidth + 3 + 6; // number + gap + degree symbol (3) + 'C' (6)
    int startTx = centerX - totalW / 2;
    int textY = centerY + 22;

    canvas.drawString(tempBuf, startTx, textY);
    canvas.drawCircle(startTx + tWidth + 2, textY + 1, 1, COLOR_ORANGE_MID);
    canvas.drawString("C", startTx + tWidth + 5, textY);
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

    // Dedicated Power / Shutdown Menu overlay
    if (qpInShutdownMenu) {
      renderShutdownMenu(offsetX);
      return;
    }

    // Dedicated Circular Brightness Menu overlay
    if (qpInBrightnessMenu) {
      renderBrightnessMenu(offsetX);
      return;
    }

    // 6 Concept Quick Action Tiles (Android Wear / Squircle Style)
    // Layout: 2 rows x 3 columns
    // Tile size: 64 x 62 px, corner radius: 8 px
    const int tileW = 64;
    const int tileH = 62;
    const int startX[3] = { 14, 88, 162 };
    const int startY[2] = { 46, 122 };

    char dispLabel[12];
    snprintf(dispLabel, sizeof(dispLabel), "%d%%", brightnessPercent);

    const char* tileLabels[6] = {
      "SETTINGS",
      qpHrmMeasuring ? "MEASURE" : "HRM",
      qpEcoMode ? "ECO: ON" : "ECO: OFF",
      dispLabel,
      qpSilentMode ? "SILENT" : "SOUND",
      "SHUTDOWN"
    };

    for (int i = 0; i < 6; i++) {
      int col = i % 3;
      int row = i / 3;
      int tx = startX[col] + offsetX;
      int ty = startY[row];
      int icx = tx + tileW / 2;
      int icy = ty + 22;

      bool isFocused = (qpInTileMode && qpFocusIndex == i);
      bool isToggledOn = false;
      if (i == 1 && qpHrmMeasuring) isToggledOn = true;
      if (i == 2 && qpEcoMode)      isToggledOn = true;
      if (i == 3 && brightnessPercent >= 90) isToggledOn = true;
      if (i == 4 && qpSilentMode)   isToggledOn = true;

      uint16_t borderColor = isFocused ? COLOR_ORANGE_BRIGHT : (isToggledOn ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM);
      uint16_t iconColor   = isFocused ? COLOR_ORANGE_BRIGHT : (isToggledOn ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
      uint16_t textColor   = isFocused ? COLOR_ORANGE_BRIGHT : (isToggledOn ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);

      // Background fill
      if (isToggledOn) {
        canvas.fillRoundRect(tx + 1, ty + 1, tileW - 2, tileH - 2, 7, COLOR_ORANGE_DARK);
      } else {
        canvas.fillRoundRect(tx + 1, ty + 1, tileW - 2, tileH - 2, 7, COLOR_BG);
      }

      // Border outline
      canvas.drawRoundRect(tx, ty, tileW, tileH, 8, borderColor);

      // High-vis focus halo / corner brackets
      if (isFocused) {
        canvas.drawRoundRect(tx - 2, ty - 2, tileW + 4, tileH + 4, 10, COLOR_ORANGE_BRIGHT);
      }

      // Vector Icon Rendering
      switch (i) {
        case 0: { // SETTINGS (Gear)
          canvas.drawCircle(icx, icy, 5, iconColor);
          canvas.fillCircle(icx, icy, 2, iconColor);
          for (int a = 0; a < 6; a++) {
            float rad = a * 1.04719755f;
            int x1 = icx + (int)roundf(cosf(rad) * 5.5f);
            int y1 = icy + (int)roundf(sinf(rad) * 5.5f);
            int x2 = icx + (int)roundf(cosf(rad) * 9.0f);
            int y2 = icy + (int)roundf(sinf(rad) * 9.0f);
            canvas.drawLine(x1, y1, x2, y2, iconColor);
          }
          break;
        }

        case 1: { // HRM (Heart with Pulse ECG Line)
          int heartR = 4;
          if (qpHrmMeasuring && ((millis() / 400) % 2 == 0)) {
            heartR = 5; // Beat pulse animation
          }
          canvas.fillCircle(icx - heartR, icy - 2, heartR, iconColor);
          canvas.fillCircle(icx + heartR, icy - 2, heartR, iconColor);
          canvas.fillTriangle(icx - (heartR * 2), icy, icx + (heartR * 2), icy, icx, icy + (heartR * 2), iconColor);
          // ECG cutout line
          canvas.drawLine(icx - 7, icy + 1, icx - 3, icy + 1, COLOR_BG);
          canvas.drawLine(icx - 3, icy + 1, icx - 1, icy - 4, COLOR_BG);
          canvas.drawLine(icx - 1, icy - 4, icx + 1, icy + 4, COLOR_BG);
          canvas.drawLine(icx + 1, icy + 4, icx + 3, icy + 1, COLOR_BG);
          canvas.drawLine(icx + 3, icy + 1, icx + 7, icy + 1, COLOR_BG);
          break;
        }

        case 2: { // ECO / POWER SAVE (Battery with Lightning Bolt)
          canvas.drawRoundRect(icx - 6, icy - 9, 12, 17, 2, iconColor);
          canvas.drawFastHLine(icx - 2, icy - 11, 4, iconColor);
          canvas.drawLine(icx + 1, icy - 6, icx - 2, icy - 1, iconColor);
          canvas.drawLine(icx - 2, icy - 1, icx + 2, icy - 1, iconColor);
          canvas.drawLine(icx + 2, icy - 1, icx - 1, icy + 5, iconColor);
          break;
        }

        case 3: { // DISPLAY (Sun / Brightness)
          canvas.drawCircle(icx, icy, 4, iconColor);
          canvas.fillCircle(icx, icy, 2, iconColor);
          for (int r = 0; r < 8; r++) {
            float rad = r * 0.785398f;
            int x1 = icx + (int)roundf(cosf(rad) * 6.0f);
            int y1 = icy + (int)roundf(sinf(rad) * 6.0f);
            int x2 = icx + (int)roundf(cosf(rad) * 9.5f);
            int y2 = icy + (int)roundf(sinf(rad) * 9.5f);
            canvas.drawLine(x1, y1, x2, y2, iconColor);
          }
          break;
        }

        case 4: { // AUDIO / SILENT (Bell with Mute Slash)
          canvas.drawCircle(icx, icy - 2, 4, iconColor);
          canvas.drawLine(icx - 6, icy + 4, icx - 4, icy - 2, iconColor);
          canvas.drawLine(icx + 6, icy + 4, icx + 4, icy - 2, iconColor);
          canvas.drawFastHLine(icx - 7, icy + 4, 15, iconColor);
          canvas.fillCircle(icx, icy + 6, 1, iconColor);
          if (qpSilentMode) {
            canvas.drawLine(icx - 8, icy - 8, icx + 8, icy + 8, COLOR_ORANGE_BRIGHT);
          }
          break;
        }

        case 5: { // POWER OFF / SHUTDOWN (Power Symbol)
          canvas.drawCircle(icx, icy + 1, 7, iconColor);
          canvas.fillRect(icx - 2, icy - 7, 5, 4, isToggledOn ? COLOR_ORANGE_DARK : COLOR_BG);
          canvas.drawFastVLine(icx, icy - 7, 8, iconColor);
          break;
        }
      }

      // Tile Label
      canvas.setTextColor(textColor, isToggledOn ? COLOR_ORANGE_DARK : COLOR_BG);
      canvas.drawCenterString(tileLabels[i], tx + tileW / 2, ty + 46);
    }

    // Bottom Navigation Context Bar
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    if (!qpInTileMode) {
      canvas.drawCenterString("PUSH LEVER TO NAVIGATE", 120 + offsetX, 196);
    } else {
      canvas.drawCenterString("LEVER: SELECT | BTN: BACK", 120 + offsetX, 196);
    }
  }

  void renderBrightnessMenu(int offsetX) {
    int cx = 120 + offsetX;
    int cy = 118;
    int radius = 62;

    // 1. Subtle Outer Reference Track Ring
    canvas.drawCircle(cx, cy, radius + 3, COLOR_ORANGE_DARK);
    canvas.drawCircle(cx, cy, radius - 11, COLOR_ORANGE_DARK);

    // 2. Technical Radial Ticks from 10% to 100% (19 ticks, spaced every 5%)
    // Angular sweep: 270 deg total, from 135 deg (bottom-left) to 405 deg (bottom-right)
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

      uint16_t tickColor = (p <= brightnessPercent) ? (isMajor ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID) : COLOR_ORANGE_DARK;
      canvas.drawLine(x1, y1, x2, y2, tickColor);
    }

    // 3. Active Orbital Satellite Pip (Glowing pointer pip outside the ring)
    float curFrac = (brightnessPercent - 10) / 90.0f;
    float curAngleDeg = 135.0f + curFrac * 270.0f;
    float curRad = curAngleDeg * 0.0174532925f;
    int pipX = cx + (int)roundf(cosf(curRad) * (radius + 9));
    int pipY = cy + (int)roundf(sinf(curRad) * (radius + 9));
    canvas.fillCircle(pipX, pipY, 3, COLOR_ORANGE_BRIGHT);
    canvas.drawCircle(pipX, pipY, 4, COLOR_ORANGE_MID);

    // 4. Central Sun Glyph
    int sunY = cy - 20;
    canvas.drawCircle(cx, sunY, 4, COLOR_ORANGE_BRIGHT);
    canvas.fillCircle(cx, sunY, 2, COLOR_ORANGE_BRIGHT);
    for (int r = 0; r < 8; r++) {
      float sRad = r * 0.785398f;
      int sx1 = cx + (int)roundf(cosf(sRad) * 6.0f);
      int sy1 = sunY + (int)roundf(sinf(sRad) * 6.0f);
      int sx2 = cx + (int)roundf(cosf(sRad) * 9.0f);
      int sy2 = sunY + (int)roundf(sinf(sRad) * 9.0f);
      canvas.drawLine(sx1, sy1, sx2, sy2, COLOR_ORANGE_MID);
    }

    // 5. Large Digital Readout
    canvas.setTextSize(3);
    canvas.setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    char pBuf[8];
    snprintf(pBuf, sizeof(pBuf), "%d%%", brightnessPercent);
    canvas.drawCenterString(pBuf, cx, cy + 2);

    // 6. Context Bottom Bar
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawCenterString("< LEVER > ADJUST | PUSH: OK", cx, 196);
  }

  void renderShutdownMenu(int offsetX) {
    const char* titles[3] = { "STANDBY", "SHUTDOWN", "RESTART" };
    const char* descs[3]  = { "DISPLAY OFF (ANY KEY)", "DISABLED (PLANNED)", "REBOOT ISD-CORE" };

    int cardW = 216;
    int cardH = 46;
    int startX = 12 + offsetX;
    int startY[3] = { 44, 96, 148 };

    for (int i = 0; i < 3; i++) {
      bool isSelected = (i == shutdownMenuIndex);
      int cx = startX;
      int cy = startY[i];

      uint16_t cardBg = isSelected ? COLOR_ORANGE_DARK : COLOR_BG;
      uint16_t borderCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;
      uint16_t textCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID;
      uint16_t iconCol = isSelected ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_DIM;

      // Card Background & Border
      canvas.fillRoundRect(cx, cy, cardW, cardH, 8, cardBg);
      canvas.drawRoundRect(cx, cy, cardW, cardH, 8, borderCol);
      if (isSelected) {
        canvas.drawRoundRect(cx + 1, cy + 1, cardW - 2, cardH - 2, 7, borderCol);
        // Active indicator vertical bar on left edge
        canvas.fillRoundRect(cx + 4, cy + 8, 3, cardH - 16, 2, COLOR_ORANGE_BRIGHT);
      }

      // Card Icon
      int icx = cx + 24;
      int icy = cy + (cardH / 2);

      switch (i) {
        case 0: { // STANDBY (Crescent Moon)
          canvas.fillCircle(icx, icy, 8, iconCol);
          canvas.fillCircle(icx + 4, icy - 2, 6, cardBg);
          canvas.fillCircle(icx + 6, icy - 5, 1, iconCol);
          canvas.fillCircle(icx + 8, icy + 2, 1, iconCol);
          break;
        }
        case 1: { // SHUTDOWN (Power Symbol)
          canvas.drawCircle(icx, icy + 1, 8, iconCol);
          canvas.drawCircle(icx, icy + 1, 7, iconCol);
          canvas.fillRect(icx - 3, icy - 8, 6, 6, cardBg);
          canvas.fillRect(icx - 1, icy - 8, 3, 9, iconCol);
          break;
        }
        case 2: { // RESTART (Reboot circular arrow)
          canvas.drawCircle(icx, icy, 8, iconCol);
          canvas.drawCircle(icx, icy, 7, iconCol);
          canvas.fillRect(icx, icy - 10, 9, 8, cardBg);
          canvas.fillTriangle(icx + 6, icy - 10, icx + 9, icy - 2, icx + 2, icy - 5, iconCol);
          break;
        }
      }

      // Title & Subtitle
      canvas.setTextSize(2);
      canvas.setTextColor(textCol, cardBg);
      canvas.drawString(titles[i], cx + 46, cy + 7);

      canvas.setTextSize(1);
      canvas.setTextColor(isSelected ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM, cardBg);
      canvas.drawString(descs[i], cx + 46, cy + 27);
    }

    // Bottom Navigation Context Prompt
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas.drawCenterString("< LEVER > SELECT | PUSH: OK", 120 + offsetX, 206);
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
