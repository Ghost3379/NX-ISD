#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../../SensorState.h"
#include "../../HAL.h"
#include "../../nx-systems/NX-MSF.h"

enum EnvironmentCardId {
  ENV_CARD_MASTER_HUD = 0,
  ENV_CARD_WEATHER,
  ENV_CARD_BARO_ALTITUDE,
  ENV_CARD_CLIMATE_THERMAL,
  ENV_CARD_AIR_GAS,
  ENV_CARD_LIGHT_LUX,
  ENV_CARD_COUNT
};


// 3D Spherical Coordinate Node for Vector Continent Mapping
struct GeoNode {
  int8_t lat; // -90 .. +90 degrees
  int16_t lon; // -180 .. +180 degrees
};

class AppEnvironment {
private:
  LGFX_Sprite* canvas = nullptr;
  LGFX* display = nullptr;

  uint16_t COLOR_BG;
  uint16_t COLOR_ORANGE_BRIGHT;
  uint16_t COLOR_ORANGE_MID;
  uint16_t COLOR_ORANGE_DIM;
  uint16_t COLOR_ORANGE_DARK;

  EnvironmentCardId currentCard = ENV_CARD_MASTER_HUD;
  float scrollY = 0.0f;
  float targetScrollY = 0.0f;
  static const int CARD_HEIGHT = 202; // Vertical step height per card

  // Real-Time 3D Earth Engine Variables
  float earthAngleRad = 0.0f;
  uint32_t lastEarthTick = 0;

  // Atmospheric History Buffer (16-point rolling isobar history)
  static const int PRESSURE_HISTORY_SIZE = 16;
  float pressHistory[PRESSURE_HISTORY_SIZE];
  int pressHistoryHead = 0;
  uint32_t lastPressSampleTime = 0;

  // Altimeter QNH Sea-Level Baseline (Standard Atmosphere = 1013.25 hPa)
  float qnhBaselineHpa = 1013.25f;
  bool isRelativeAltZeroed = false;
  float altZeroOffsetMeters = 0.0f;

  // Units
  bool useFahrenheit = false;

  // Thermal Min/Max Session Memory
  float minRecordedTemp = 999.0f;
  float maxRecordedTemp = -999.0f;

  // Gas Sensor Forced Pulse Animation (BME690)
  bool isGasBurnActive = false;
  uint32_t gasBurnStartTime = 0;

  // Air Quality & VOC History Sparkline (16 samples, sampled every 5 seconds)
  static const int GAS_HISTORY_SIZE = 16;
  float gasHistory[GAS_HISTORY_SIZE];
  int gasHistoryHead = 0;
  uint32_t lastGasSampleTime = 0;
  float gasBaselineKohm = 180.0f;
  float prevGasKohm = 0.0f;
  uint32_t lastGasTickTime = 0;

public:
  AppEnvironment(LGFX* tft) : display(tft) {
    for (int i = 0; i < PRESSURE_HISTORY_SIZE; i++) {
      pressHistory[i] = 1013.25f;
    }
    for (int i = 0; i < GAS_HISTORY_SIZE; i++) {
      gasHistory[i] = 150.0f;
    }
  }

  void init(LGFX_Sprite* spr, uint16_t bg, uint16_t bright, uint16_t mid, uint16_t dim, uint16_t dark) {
    canvas = spr;
    COLOR_BG = bg;
    COLOR_ORANGE_BRIGHT = bright;
    COLOR_ORANGE_MID = mid;
    COLOR_ORANGE_DIM = dim;
    COLOR_ORANGE_DARK = dark;
  }

  void onEnter() {
    currentCard = ENV_CARD_MASTER_HUD;
    scrollY = 0.0f;
    targetScrollY = 0.0f;
  }

  void onExit() {}

  // Back button handling: returns true if handled internally, false to exit to AppMenu
  bool handleNavBtn() {
    if (currentCard != ENV_CARD_MASTER_HUD) {
      currentCard = ENV_CARD_MASTER_HUD;
      targetScrollY = 0.0f;
      HAL::buzzPip(2400, 15);
      return true;
    }
    HAL::buzzPip(2000, 15);
    return false; // Exit back to 3D App Menu
  }

  void handleNavLeft() {
    // Scroll UP through the vertical card stack
    if ((int)currentCard > 0) {
      currentCard = (EnvironmentCardId)((int)currentCard - 1);
      targetScrollY = (float)((int)currentCard * CARD_HEIGHT);
      HAL::buzzPip(2800, 12);
    }
  }

  void handleNavRight() {
    // Scroll DOWN through the vertical card stack
    if ((int)currentCard < ENV_CARD_COUNT - 1) {
      currentCard = (EnvironmentCardId)((int)currentCard + 1);
      targetScrollY = (float)((int)currentCard * CARD_HEIGHT);
      HAL::buzzPip(3200, 12);
    }
  }

  void handleNavPush() {
    if (currentCard == ENV_CARD_MASTER_HUD) {
      // Jump into Weather & Forecast card
      currentCard = ENV_CARD_WEATHER;
      targetScrollY = (float)CARD_HEIGHT;
      HAL::buzzPip(3400, 15);
    } else if (currentCard == ENV_CARD_WEATHER) {
      HAL::buzzPip(3600, 15);
    } else if (currentCard == ENV_CARD_BARO_ALTITUDE) {
      // Toggle Altimeter Zero / Absolute QNH
      isRelativeAltZeroed = !isRelativeAltZeroed;
      HAL::buzzPip(isRelativeAltZeroed ? 4200 : 2600, 20);
    } else if (currentCard == ENV_CARD_CLIMATE_THERMAL) {
      // Toggle Celsius <-> Fahrenheit
      useFahrenheit = !useFahrenheit;
      HAL::buzzPip(3600, 15);
    } else if (currentCard == ENV_CARD_AIR_GAS) {
      // Trigger Forced-Mode Gas Burn Pulse on BME690
      isGasBurnActive = true;
      gasBurnStartTime = millis();
      HAL::buzzPip(4400, 30);
    } else if (currentCard == ENV_CARD_LIGHT_LUX) {
      HAL::buzzPip(3800, 15);
    }
  }

  void update(const SensorState& state) {
    uint32_t now = millis();

    // 1. Advance Real-Time 3D Earth Rotation (~1 full revolution every 16 seconds)
    if (lastEarthTick == 0) lastEarthTick = now;
    uint32_t deltaMs = now - lastEarthTick;
    lastEarthTick = now;
    earthAngleRad += (float)deltaMs * 0.0003927f; // 2pi in 16000ms
    if (earthAngleRad >= 6.2831853f) {
      earthAngleRad -= 6.2831853f;
    }

    // 2. Track Thermal Min/Max Session Bounds
    if (state.envDataReady && state.temp > -40.0f) {
      if (state.temp < minRecordedTemp) minRecordedTemp = state.temp;
      if (state.temp > maxRecordedTemp) maxRecordedTemp = state.temp;
    }

    // 3. Update Pressure History (Sample every 30 seconds for live demo responsiveness)
    if (state.envDataReady && state.press > 300.0f) {
      if (now - lastPressSampleTime >= 30000 || lastPressSampleTime == 0) {
        lastPressSampleTime = now;
        pressHistory[pressHistoryHead] = state.press;
        pressHistoryHead = (pressHistoryHead + 1) % PRESSURE_HISTORY_SIZE;
      }
    }

    // 4. Update Gas History (Sample every 5 seconds for live sparkline response)
    if (state.envDataReady && state.gas > 0.0f) {
      if (now - lastGasSampleTime >= 5000 || lastGasSampleTime == 0) {
        lastGasSampleTime = now;
        gasHistory[gasHistoryHead] = state.gas / 1000.0f;
        gasHistoryHead = (gasHistoryHead + 1) % GAS_HISTORY_SIZE;
      }
    }

    // 5. Smooth Spring-Damper Vertical Scroll Easing (~120ms transition)
    scrollY += (targetScrollY - scrollY) * 0.35f;
    if (fabsf(targetScrollY - scrollY) < 0.5f) {
      scrollY = targetScrollY;
    }

    // 6. Check BME690 gas burn timeout
    if (isGasBurnActive && (now - gasBurnStartTime > 2500)) {
      isGasBurnActive = false;
    }
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    update(state);

    canvas->fillSprite(COLOR_BG);

    // Standard ISD-Core Technical Outer Frame
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);

    // Clip card rendering between Header divider (y = 28) and Bottom border (y = 236)
    // to guarantee smooth vertical sliding without bleeding onto header or frame
    canvas->setClipRect(5, 29, 224, 206);

    const int CARD_TOP_Y = 30;

    // Render cards that fall within visible screen space
    for (int i = 0; i < ENV_CARD_COUNT; i++) {
      int cardY = CARD_TOP_Y + (i * CARD_HEIGHT) - (int)roundf(scrollY);
      if (cardY + CARD_HEIGHT > 28 && cardY < 236) {
        switch ((EnvironmentCardId)i) {
          case ENV_CARD_MASTER_HUD:
            renderMasterHudCard(state, cardY);
            break;
          case ENV_CARD_WEATHER:
            renderWeatherCard(state, cardY);
            break;
          case ENV_CARD_BARO_ALTITUDE:
            renderBaroCard(state, cardY);
            break;
          case ENV_CARD_CLIMATE_THERMAL:
            renderClimateCard(state, cardY);
            break;
          case ENV_CARD_AIR_GAS:
            renderAirQualityCard(state, cardY);
            break;
          case ENV_CARD_LIGHT_LUX:
            renderPhotometricsCard(state, cardY);
            break;
          default:
            break;
        }
      }
    }

    // Clear clipping rect to draw persistent header & tactical scrollbar
    canvas->clearClipRect();

    // Persistent Header Bar (pinned over sliding content)
    renderHeader(state);

    // Tactical Right Side Scrollbar (replaces annoying bottom bar)
    renderSideScrollbar();

    // Push offscreen buffer cleanly to ST7789 display
    canvas->pushSprite(0, 0);
  }

private:
  void renderHeader(const SensorState& state) {
    // Solid background behind header to mask sliding content
    canvas->fillRect(5, 5, 230, 23, COLOR_BG);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);

    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    const char* deckNames[ENV_CARD_COUNT] = {
      "ENVIRONMENT",
      "WEATHER",
      "ATMOSPHERE",
      "CLIMATE",
      "AIR QUALITY",
      "PHOTOMETRICS"
    };
    canvas->drawString(deckNames[(int)currentCard], 82, 10);

    // Battery Indicator (standard NX-ISD formatting)
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
    canvas->drawRightString(batBuf, 222, 10);
  }

  // Tactical Right Side Scrollbar: shows fractional position & discrete card notches
  void renderSideScrollbar() {
    int barX = 232;
    int barTopY = 34;
    int barH = 196;

    // Track line
    canvas->drawFastVLine(barX + 1, barTopY, barH, COLOR_ORANGE_DARK);

    // Card notch ticks
    for (int c = 0; c < ENV_CARD_COUNT; c++) {
      int notchY = barTopY + (c * (barH - 12)) / (ENV_CARD_COUNT - 1) + 6;
      canvas->drawFastHLine(barX, notchY, 3, (c == (int)currentCard) ? COLOR_ORANGE_MID : COLOR_ORANGE_DARK);
    }

    // Smooth indicator thumb
    float maxScroll = (float)((ENV_CARD_COUNT - 1) * CARD_HEIGHT);
    float ratio = (maxScroll > 0.0f) ? constrain(scrollY / maxScroll, 0.0f, 1.0f) : 0.0f;
    int thumbH = 16;
    int thumbY = barTopY + (int)roundf(ratio * (float)(barH - thumbH));
    canvas->fillRect(barX, thumbY, 3, thumbH, COLOR_ORANGE_BRIGHT);
  }

  // =========================================================================
  // CARD 0: MASTER ENVIRONMENTAL COMMAND HUD
  // =========================================================================
  void renderMasterHudCard(const SensorState& state, int topY) {
    int cx = 118;
    int cy = topY + 98;
    int globeR = 26;

    // 1. Render 3D Vector Rotating Earth Globe in the center
    draw3DVectorEarth(cx, cy, globeR, earthAngleRad);

    // 2. Blueprint Tactical Leader Lines with (O) Anchor Nodes
    int anchorNW_X = cx - 18, anchorNW_Y = cy - 18;
    int anchorNE_X = cx + 18, anchorNE_Y = cy - 18;
    int anchorSW_X = cx - 18, anchorSW_Y = cy + 18;
    int anchorSE_X = cx + 18, anchorSE_Y = cy + 18;

    int p1x = 8,   p1y = topY + 12, p1w = 94, p1h = 52;
    int p2x = 132, p2y = topY + 12, p2w = 94, p2h = 52;
    int p3x = 8,   p3y = topY + 138, p3w = 94, p3h = 52;
    int p4x = 132, p4y = topY + 138, p4w = 94, p4h = 52;

    // Leader line traces
    canvas->drawLine(p1x + p1w, p1y + 36, anchorNW_X, p1y + 36, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorNW_X, p1y + 36, anchorNW_X, anchorNW_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorNW_X, anchorNW_Y);

    canvas->drawLine(p2x, p2y + 36, anchorNE_X, p2y + 36, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorNE_X, p2y + 36, anchorNE_X, anchorNE_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorNE_X, anchorNE_Y);

    canvas->drawLine(p3x + p3w, p3y + 16, anchorSW_X, p3y + 16, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorSW_X, p3y + 16, anchorSW_X, anchorSW_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorSW_X, anchorSW_Y);

    canvas->drawLine(p4x, p4y + 16, anchorSE_X, p4y + 16, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorSE_X, p4y + 16, anchorSE_X, anchorSE_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorSE_X, anchorSE_Y);

    // ================= POD 1: BARO (Top-Left) =================
    canvas->drawRoundRect(p1x, p1y, p1w, p1h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ATMOSPHERE", p1x + 6, p1y + 5);

    char valBuf[20];
    if (state.envDataReady && state.press > 300.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0fhPa", state.press);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p1x + 6, p1y + 18, &fonts::Font2);

      const char* fc = (state.press > 1018.0f) ? "HIGH" : (state.press < 1005.0f) ? "STORM" : "FAIR";
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(fc, p1x + 6, p1y + 36);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("----hPa", p1x + 6, p1y + 18, &fonts::Font2);
      canvas->drawString("BME690...", p1x + 6, p1y + 36);
    }

    // ================= POD 2: CLIMATE (Top-Right) =================
    canvas->drawRoundRect(p2x, p2y, p2w, p2h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("CLIMATE", p2x + 6, p2y + 5);

    if (state.envDataReady && state.temp > -40.0f) {
      float t = useFahrenheit ? (state.temp * 1.8f + 32.0f) : state.temp;
      snprintf(valBuf, sizeof(valBuf), "%.1f%s", t, useFahrenheit ? "F" : "C");
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p2x + 6, p2y + 18, &fonts::Font2);

      snprintf(valBuf, sizeof(valBuf), "%.0f%% RH", state.hum);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(valBuf, p2x + 6, p2y + 36);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--.- C", p2x + 6, p2y + 18, &fonts::Font2);
      canvas->drawString("--% RH", p2x + 6, p2y + 36);
    }

    // ================= POD 3: AIR QUALITY (Bottom-Left) =================
    canvas->drawRoundRect(p3x, p3y, p3w, p3h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("AIR QUALITY", p3x + 6, p3y + 5);

    if (state.envDataReady && state.gas > 0.0f) {
      NX_MSF::AirQualityAnalysis hudQ = NX_MSF::calculateAirQuality(
        state.gas, state.temp, state.hum, gasBaselineKohm, prevGasKohm, 1000
      );
      snprintf(valBuf, sizeof(valBuf), "AQI %d", hudQ.iaqScore);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p3x + 6, p3y + 18, &fonts::Font2);

      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(hudQ.iaqTier, p3x + 6, p3y + 36);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("AQI --", p3x + 6, p3y + 18, &fonts::Font2);
      canvas->drawString("BME690...", p3x + 6, p3y + 36);
    }

    // ================= POD 4: PHOTOMETRICS (Bottom-Right) =================
    canvas->drawRoundRect(p4x, p4y, p4w, p4h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("PHOTOMETRICS", p4x + 6, p4y + 5);

    if (state.lightLux >= 0.0f) {
      if (state.lightLux > 999.0f) {
        snprintf(valBuf, sizeof(valBuf), "%.1fk lx", state.lightLux / 1000.0f);
      } else {
        snprintf(valBuf, sizeof(valBuf), "%.0f lx", state.lightLux);
      }
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p4x + 6, p4y + 18, &fonts::Font2);

      const char* tag = (state.lightLux > 1200.0f) ? "SUNLIGHT" : (state.lightLux > 80.0f) ? "INDOORS" : "NIGHT";
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(tag, p4x + 6, p4y + 36);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--- lx", p4x + 6, p4y + 18, &fonts::Font2);
      canvas->drawString("OPT3001...", p4x + 6, p4y + 36);
    }
  }

  // =========================================================================
  // CARD 1: WEATHER & METEOROLOGICAL FORECAST
  // =========================================================================
  void renderWeatherCard(const SensorState& state, int topY) {
    float press = (state.envDataReady && state.press > 300.0f) ? state.press : 1013.25f;
    float tempC = (state.envDataReady && state.temp > -40.0f) ? state.temp : 22.0f;
    float hum   = (state.envDataReady && state.hum > 0.0f) ? state.hum : 45.0f;

    // Calculate Dew Point, Fog Margin, and Weather Condition via NX-MSF
    float dewPointC = NX_MSF::calculateDewPoint(tempC, hum);
    float fogMargin = NX_MSF::calculateFogMargin(tempC, dewPointC);
    WeatherCondition cond = NX_MSF::estimateWeatherCondition(press, hum);

    // 1. Draw Large Vector Weather Glyph at (46, topY + 44)
    int gx = 46, gy = topY + 44;
    drawWeatherGlyph(gx, gy, cond);

    // Condition Banner & Summary beside Glyph
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    const char* condTitles[] = {
      "FAIR / CLEAR",
      "PARTLY CLOUDY",
      "OVERCAST",
      "RAIN LIKELY",
      "STORM ALERT"
    };
    canvas->drawString(condTitles[(int)cond], 84, topY + 28, &fonts::Font2);

    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    const char* condSubs[] = {
      "Stable High Pressure",
      "Moderate Humidity",
      "Dense Cloud Cover",
      "Precipitation Expected",
      "Rapid Barometric Drop"
    };
    canvas->drawString(condSubs[(int)cond], 84, topY + 48);

    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), "%.1f hPa  %.1f%s", press, tempC, "C");
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString(valBuf, 84, topY + 62);

    // 2. Tactical Pod: 3-Hour Pressure Trend
    int p1y = topY + 84;
    canvas->drawRoundRect(10, p1y, 212, 48, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("3H BAROMETRIC TENDENCY", 16, p1y + 6);

    const char* trendTag = (press > 1018.0f) ? "+1.8 hPa / 3h (RISING)" :
                           (press < 1005.0f) ? "-2.9 hPa / 3h (RAPID DROP!)" :
                           "-0.2 hPa / 3h (STABLE)";
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(trendTag, 16, p1y + 22, &fonts::Font2);

    // 3. Tactical Pod: Dew Point & Fog Margin
    int p2y = topY + 140;
    canvas->drawRoundRect(10, p2y, 212, 50, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("DEW POINT & FOG SENTINEL", 16, p2y + 6);

    char dpBuf[40];
    snprintf(dpBuf, sizeof(dpBuf), "DEW: %.1f°C  MARGIN: %.1f°C", dewPointC, fogMargin);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(dpBuf, 16, p2y + 20);

    const char* fogStatus = (fogMargin <= 1.5f) ? "[!] DENSE FOG IMMINENT" : "[OK] NO CONDENSATION RISK";
    canvas->setTextColor((fogMargin <= 1.5f) ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(fogStatus, 16, p2y + 34);
  }

  // Draw crisp vector weather icons
  void drawWeatherGlyph(int cx, int cy, WeatherCondition cond) {
    switch (cond) {
      case WEATHER_CLEAR: {
        // Sun circle with 8 radiating beam ticks
        canvas->drawCircle(cx, cy, 14, COLOR_ORANGE_BRIGHT);
        canvas->fillCircle(cx, cy, 10, COLOR_ORANGE_DARK);
        for (int a = 0; a < 8; a++) {
          float rad = (float)a * 0.785398f;
          int x1 = cx + (int)(cosf(rad) * 16);
          int y1 = cy + (int)(sinf(rad) * 16);
          int x2 = cx + (int)(cosf(rad) * 21);
          int y2 = cy + (int)(sinf(rad) * 21);
          canvas->drawLine(x1, y1, x2, y2, COLOR_ORANGE_BRIGHT);
        }
        break;
      }
      case WEATHER_PARTLY_CLOUDY: {
        // Sun behind cloud
        canvas->drawCircle(cx - 6, cy - 8, 10, COLOR_ORANGE_MID);
        drawCloudContour(cx + 2, cy + 4, 18, 10);
        break;
      }
      case WEATHER_OVERCAST: {
        // Double cloud
        drawCloudContour(cx - 6, cy - 4, 16, 9);
        drawCloudContour(cx + 4, cy + 6, 20, 11);
        break;
      }
      case WEATHER_RAIN: {
        // Cloud with falling rain dashes
        drawCloudContour(cx, cy - 4, 22, 12);
        for (int r = 0; r < 4; r++) {
          int rx = cx - 12 + r * 8;
          int ry = cy + 12;
          canvas->drawLine(rx, ry, rx - 3, ry + 8, COLOR_ORANGE_BRIGHT);
        }
        break;
      }
      case WEATHER_STORM: {
        // Cloud with bold zig-zag lightning bolt
        drawCloudContour(cx, cy - 6, 22, 12);
        int lx = cx;
        int ly = cy + 8;
        canvas->drawLine(lx + 2, ly, lx - 4, ly + 8, COLOR_ORANGE_BRIGHT);
        canvas->drawLine(lx - 4, ly + 8, lx + 2, ly + 8, COLOR_ORANGE_BRIGHT);
        canvas->drawLine(lx + 2, ly + 8, lx - 3, ly + 18, COLOR_ORANGE_BRIGHT);
        break;
      }
    }
  }

  void drawCloudContour(int cx, int cy, int w, int h) {
    canvas->drawRoundRect(cx - w/2, cy, w, h, 4, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(cx - w/4, cy, h - 2, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(cx + w/6, cy - 2, h - 1, COLOR_ORANGE_BRIGHT);
  }

  // =========================================================================
  // CARD 2: BAROMETER & MOUNTAIN ALTITUDE SCHEMATIC
  // =========================================================================
  void renderBaroCard(const SensorState& state, int topY) {
    float press = (state.envDataReady && state.press > 300.0f) ? state.press : 1013.25f;

    // Calculate Elevation from NX-MSF hypsometric formula
    float rawAltitudeM = NX_MSF::calculateAltitude(press, qnhBaselineHpa);
    if (isRelativeAltZeroed && altZeroOffsetMeters == 0.0f) {
      altZeroOffsetMeters = rawAltitudeM;
    }
    float altitudeM = isRelativeAltZeroed ? (rawAltitudeM - altZeroOffsetMeters) : rawAltitudeM;
    float altitudeFt = altitudeM * 3.28084f;

    // 1. Digital Telemetry Readout
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("PRESSURE:", 12, topY + 6);
    char pBuf[32];
    snprintf(pBuf, sizeof(pBuf), "%.1f hPa", press);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(pBuf, 80, topY + 6, &fonts::Font2);

    const char* trendStr = (press > 1018.0f) ? "[ CLEAR / HIGH ]" : (press < 1005.0f) ? "[ STORM ]" : "[ STABLE ]";
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawRightString(trendStr, 224, topY + 8);

    // 2. Vector Mountain Peak & Elevation Schematic
    int groundY = topY + 104;
    canvas->drawFastHLine(10, groundY, 214, COLOR_ORANGE_DARK);

    // Mountain silhouettes
    canvas->drawLine(64, groundY, 104, groundY - 48, COLOR_ORANGE_DARK);
    canvas->drawLine(104, groundY - 48, 144, groundY, COLOR_ORANGE_DARK);

    canvas->drawLine(14, groundY, 54, groundY - 58, COLOR_ORANGE_MID);
    canvas->drawLine(54, groundY - 58, 94, groundY, COLOR_ORANGE_MID);
    canvas->drawCircle(54, groundY - 58, 2, COLOR_ORANGE_BRIGHT);

    // Elevation Target Line
    int targetY = map((long)constrain(altitudeM, -50.0f, 1000.0f), -50L, 1000L, groundY, (long)(groundY - 56));
    canvas->drawFastHLine(54, targetY, 168, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(54, targetY, 3, COLOR_ORANGE_BRIGHT);

    char altBuf[32];
    snprintf(altBuf, sizeof(altBuf), "%+.0f m  (%+.0f ft)", altitudeM, altitudeFt);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(altBuf, 114, targetY - 14, &fonts::Font2);

    const char* modeTag = isRelativeAltZeroed ? "[REL ZERO]" : "[QNH 1013]";
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString(modeTag, 114, targetY + 3);

    // 3. 16-Bar Historical Isobaric Sparkline
    canvas->drawFastHLine(10, topY + 120, 214, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("ISOBARIC HISTORY (16 SAMPLES):", 12, topY + 124);

    int chartX = 14;
    int chartY = topY + 188;
    int barW = 10;
    int barGap = 3;

    float minP = 1050.0f, maxP = 950.0f;
    for (int i = 0; i < PRESSURE_HISTORY_SIZE; i++) {
      if (pressHistory[i] < minP) minP = pressHistory[i];
      if (pressHistory[i] > maxP) maxP = pressHistory[i];
    }
    if (maxP - minP < 2.0f) maxP = minP + 2.0f;

    for (int b = 0; b < PRESSURE_HISTORY_SIZE; b++) {
      int idx = (pressHistoryHead + b) % PRESSURE_HISTORY_SIZE;
      float val = pressHistory[idx];
      int barH = map((long)(val * 10.0f), (long)(minP * 10.0f), (long)(maxP * 10.0f), 4L, 46L);
      int bx = chartX + b * (barW + barGap);
      int by = chartY - barH;

      if (b == PRESSURE_HISTORY_SIZE - 1) {
        canvas->fillRect(bx, by, barW, barH, COLOR_ORANGE_BRIGHT);
      } else {
        canvas->drawRect(bx, by, barW, barH, COLOR_ORANGE_MID);
      }
    }
  }

  // =========================================================================
  // CARD 3: CLIMATE & THERMOMETER SCHEMATIC
  // =========================================================================
  void renderClimateCard(const SensorState& state, int topY) {
    float tempC = (state.envDataReady && state.temp > -40.0f) ? state.temp : 22.0f;
    float hum   = (state.envDataReady && state.hum > 0.0f) ? state.hum : 45.0f;

    // Calculate Dew Point via NX-MSF
    float dewPointC = NX_MSF::calculateDewPoint(tempC, hum);

    float dispTemp = useFahrenheit ? (tempC * 1.8f + 32.0f) : tempC;
    float dispDew  = useFahrenheit ? (dewPointC * 1.8f + 32.0f) : dewPointC;

    // 1. Vector Thermometer Capillary Tube (Left: x = 24)
    int tubeX = 24;
    int tubeTopY = topY + 12;
    int tubeBotY = topY + 144;
    int bulbY = topY + 160;
    int bulbR = 12;

    canvas->drawRoundRect(tubeX - 4, tubeTopY, 8, tubeBotY - tubeTopY, 4, COLOR_ORANGE_MID);
    canvas->fillCircle(tubeX, bulbY, bulbR, COLOR_ORANGE_DARK);
    canvas->drawCircle(tubeX, bulbY, bulbR, COLOR_ORANGE_BRIGHT);

    int mercuryH = map((long)constrain(tempC, -10.0f, 50.0f), -10L, 50L, 6L, (long)(tubeBotY - tubeTopY - 8));
    int mercuryTopY = tubeBotY - mercuryH;
    canvas->fillRect(tubeX - 2, mercuryTopY, 4, mercuryH + 6, COLOR_ORANGE_BRIGHT);
    canvas->fillCircle(tubeX, bulbY, bulbR - 3, COLOR_ORANGE_BRIGHT);

    // Ticks
    canvas->drawFastHLine(tubeX + 6, tubeTopY + 10, 6, COLOR_ORANGE_DIM);
    canvas->drawString("40", tubeX + 14, tubeTopY + 7);
    canvas->drawFastHLine(tubeX + 6, tubeTopY + (tubeBotY - tubeTopY)/2, 8, COLOR_ORANGE_MID);
    canvas->drawString("20", tubeX + 16, tubeTopY + (tubeBotY - tubeTopY)/2 - 3);
    canvas->drawFastHLine(tubeX + 6, tubeBotY - 12, 6, COLOR_ORANGE_DIM);
    canvas->drawString("0", tubeX + 14, tubeBotY - 15);

    // 2. Primary Readouts
    char tBuf[32];
    snprintf(tBuf, sizeof(tBuf), "%.1f%s", dispTemp, useFahrenheit ? "°F" : "°C");
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(tBuf, 64, topY + 14, &fonts::Font4);

    snprintf(tBuf, sizeof(tBuf), "%.0f%% RH", hum);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(tBuf, 64, topY + 46, &fonts::Font2);

    snprintf(tBuf, sizeof(tBuf), "DEW: %.1f%s", dispDew, useFahrenheit ? "°F" : "°C");
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString(tBuf, 64, topY + 68);

    if (minRecordedTemp < 100.0f) {
      float minD = useFahrenheit ? (minRecordedTemp * 1.8f + 32.0f) : minRecordedTemp;
      float maxD = useFahrenheit ? (maxRecordedTemp * 1.8f + 32.0f) : maxRecordedTemp;
      char mmBuf[32];
      snprintf(mmBuf, sizeof(mmBuf), "MIN:%.1f°  MAX:%.1f°", minD, maxD);
      canvas->drawString(mmBuf, 64, topY + 84);
    }

    // 3. Psychrometric Comfort Box
    int boxX = 64, boxY = topY + 112, boxW = 158, boxH = 68;
    canvas->drawRect(boxX, boxY, boxW, boxH, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("COMFORT TARGET", boxX + 6, boxY + 4);

    int optX1 = boxX + map(20, 0, 40, 10, boxW - 10);
    int optX2 = boxX + map(26, 0, 40, 10, boxW - 10);
    int optY1 = boxY + boxH - map(60, 10, 90, 8, boxH - 12);
    int optY2 = boxY + boxH - map(30, 10, 90, 8, boxH - 12);
    canvas->drawRect(optX1, optY1, optX2 - optX1, optY2 - optY1, COLOR_ORANGE_MID);
    canvas->drawString("OPTIMAL", optX1 + 4, optY1 + 4);

    int curDotX = boxX + map((long)constrain(tempC, 0.0f, 40.0f), 0L, 40L, 10L, (long)(boxW - 10));
    int curDotY = boxY + boxH - map((long)constrain(hum, 10.0f, 90.0f), 10L, 90L, 8L, (long)(boxH - 12));
    canvas->fillCircle(curDotX, curDotY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(curDotX, curDotY, 5, COLOR_ORANGE_BRIGHT);
  }

  // =========================================================================
  // CARD 4: AIR QUALITY & VOC GAS (BME690)
  // =========================================================================
  void renderAirQualityCard(const SensorState& state, int topY) {
    uint32_t now = millis();
    uint32_t deltaMs = (lastGasTickTime == 0) ? 1000 : (now - lastGasTickTime);
    lastGasTickTime = now;

    NX_MSF::AirQualityAnalysis airQ = NX_MSF::calculateAirQuality(
      state.gas, state.temp, state.hum, gasBaselineKohm, prevGasKohm, deltaMs
    );
    prevGasKohm = airQ.compGasKohm;

    // 1. Hero IAQ Index & Tier Badge
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("IAQ INDEX", 12, topY + 10);
    char scoreBuf[16];
    snprintf(scoreBuf, sizeof(scoreBuf), "%d", airQ.iaqScore);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(scoreBuf, 74, topY + 6, &fonts::Font4);

    // Tier Badge Pill (Right Side)
    canvas->drawRoundRect(134, topY + 8, 88, 22, 3, COLOR_ORANGE_MID);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawCenterString(airQ.iaqTier, 178, topY + 12, &fonts::Font2);

    // 2. Segmented Air Purity Gauge (16 Precision Segments)
    int meterX = 12;
    int meterY = topY + 36;
    int segW = 10;
    int segH = 6;
    int segGap = 3;
    int activeSegs = map((long)constrain(roundf(airQ.airPurityPct), 0.0f, 100.0f), 0L, 100L, 1L, 16L);

    for (int s = 0; s < 16; s++) {
      int sx = meterX + s * (segW + segGap);
      if (s < activeSegs) {
        canvas->fillRect(sx, meterY, segW, segH, COLOR_ORANGE_BRIGHT);
      } else {
        canvas->drawRect(sx, meterY, segW, segH, COLOR_ORANGE_DARK);
      }
    }

    char pBuf[32];
    snprintf(pBuf, sizeof(pBuf), "%.0f%% PURITY", airQ.airPurityPct);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("0% HAZARD", 12, topY + 45);
    canvas->drawRightString(pBuf, 220, topY + 45);

    // 3. Quad Telemetry Pods (2 rows x 2 cols)
    int podY1 = topY + 60;
    int podY2 = topY + 88;
    int podW = 102;
    int podH = 24;

    // Pod 1: eCO2 (Top-Left)
    canvas->drawRoundRect(12, podY1, podW, podH, 2, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("eCO2:", 16, podY1 + 6);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f ppm", airQ.estEco2Ppm);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawRightString(buf, 110, podY1 + 6);

    // Pod 2: bVOC (Top-Right)
    canvas->drawRoundRect(120, podY1, podW, podH, 2, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("bVOC:", 124, podY1 + 6);
    snprintf(buf, sizeof(buf), "%.2f ppm", airQ.estBvocPpm);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawRightString(buf, 218, podY1 + 6);

    // Pod 3: Rgas Live (Bottom-Left)
    canvas->drawRoundRect(12, podY2, podW, podH, 2, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("Rgas:", 16, podY2 + 6);
    snprintf(buf, sizeof(buf), "%.1fk", airQ.compGasKohm);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawRightString(buf, 110, podY2 + 6);

    // Pod 4: R0 Clean Baseline (Bottom-Right)
    canvas->drawRoundRect(120, podY2, podW, podH, 2, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("R0 base:", 124, podY2 + 6);
    snprintf(buf, sizeof(buf), "%.1fk", airQ.baselineKohm);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawRightString(buf, 218, podY2 + 6);

    // 4. VOC Plume Sentinel & 16-Bar Real-Time Sparkline
    int sparkHeaderY = topY + 118;
    if (isGasBurnActive) {
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString("[ 300°C HEATER SCAN ]", 12, sparkHeaderY);
    } else {
      bool isFlashing = (airQ.rateOfChangeKohmPerS < -6.0f) && ((millis() % 600) < 300);
      canvas->setTextColor(isFlashing ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(airQ.eventStatus, 12, sparkHeaderY);
    }

    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawRightString("16-PT TREND", 220, sparkHeaderY);

    // Draw 16-Bar Live Sparkline Graph
    int graphGroundY = topY + 188;
    int maxBarH = 52;
    canvas->drawFastHLine(12, graphGroundY, 210, COLOR_ORANGE_DARK);

    // Dotted Clean-Air Reference Line
    for (int d = 12; d < 220; d += 6) {
      canvas->drawPixel(d, graphGroundY - maxBarH + 4, COLOR_ORANGE_DARK);
    }

    for (int b = 0; b < GAS_HISTORY_SIZE; b++) {
      int idx = (gasHistoryHead + b) % GAS_HISTORY_SIZE;
      float histVal = gasHistory[idx];
      int barH = (int)map((long)constrain(histVal, 10.0f, 250.0f), 10L, 250L, 4L, (long)maxBarH);
      int bx = 14 + b * 13;
      int by = graphGroundY - barH;

      uint16_t barColor = (b == GAS_HISTORY_SIZE - 1) ? COLOR_ORANGE_BRIGHT :
                          (histVal >= airQ.baselineKohm * 0.8f) ? COLOR_ORANGE_MID : COLOR_ORANGE_DIM;

      if (isGasBurnActive && (b % 2 == (millis() / 150) % 2)) {
        barColor = COLOR_ORANGE_BRIGHT;
      }

      canvas->fillRect(bx, by, 9, barH, barColor);
    }
  }

  // =========================================================================
  // CARD 5: PHOTOMETRICS & LIGHT INTENSITY SCHEMATIC
  // =========================================================================
  void renderPhotometricsCard(const SensorState& state, int topY) {
    float lux = (state.lightLux >= 0.0f) ? state.lightLux : 450.0f;

    // 1. Optical Iris Aperture Graphic (Left: x = 54)
    int irisX = 54, irisY = topY + 54, irisR = 28;
    canvas->drawCircle(irisX, irisY, irisR, COLOR_ORANGE_MID);
    canvas->drawCircle(irisX, irisY, irisR + 4, COLOR_ORANGE_DARK);

    int bladeOpening = map((long)constrain(lux, 10.0f, 2000.0f), 10L, 2000L, 4L, (long)(irisR - 4));
    for (int b = 0; b < 6; b++) {
      float rad = (float)b * 1.04719755f;
      int x1 = irisX + (int)(cosf(rad) * irisR);
      int y1 = irisY + (int)(sinf(rad) * irisR);
      int x2 = irisX + (int)(cosf(rad + 0.5f) * bladeOpening);
      int y2 = irisY + (int)(sinf(rad + 0.5f) * bladeOpening);
      canvas->drawLine(x1, y1, x2, y2, COLOR_ORANGE_BRIGHT);
    }
    canvas->drawCircle(irisX, irisY, bladeOpening, COLOR_ORANGE_BRIGHT);

    // 2. Primary Lux Readout
    char lBuf[32];
    if (lux > 999.0f) {
      snprintf(lBuf, sizeof(lBuf), "%.2f k", lux / 1000.0f);
    } else {
      snprintf(lBuf, sizeof(lBuf), "%.0f", lux);
    }
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(lBuf, 106, topY + 28, &fonts::Font4);

    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("LUX (OPT3001)", 106, topY + 56);

    const char* zone = (lux > 5000.0f) ? "[ DIRECT SUN ]" : (lux > 1000.0f) ? "[ OVERCAST ]" : (lux > 200.0f) ? "[ INDOORS ]" : "[ NIGHT ]";
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(zone, 106, topY + 70);

    // 3. 180° Celestial Daylight Horizon Arc
    int arcX = 118, arcY = topY + 180, arcR = 64;
    canvas->drawCircle(arcX, arcY, arcR, COLOR_ORANGE_DARK);
    canvas->drawFastHLine(14, arcY, 208, COLOR_ORANGE_DARK);

    float logLux = log10f(constrain(lux, 1.0f, 10000.0f));
    float angleRad = 3.14159265f - (logLux / 4.0f) * 3.14159265f;
    int sunX = arcX + (int)(cosf(angleRad) * arcR);
    int sunY = arcY - (int)(sinf(angleRad) * arcR);

    canvas->fillCircle(sunX, sunY, 4, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(sunX, sunY, 7, COLOR_ORANGE_BRIGHT);

    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("NIGHT", 14, arcY + 4);
    canvas->drawString("INDOORS", 96, arcY - 48);
    canvas->drawRightString("DAYLIGHT", 222, arcY + 4);
  }

  // =========================================================================
  // 3D VECTOR EARTH MATHEMATICAL ENGINE
  // =========================================================================
  void draw3DVectorEarth(int cx, int cy, int radius, float rotRad) {
    canvas->drawCircle(cx, cy, radius + 4, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius, COLOR_ORANGE_MID);

    const float tilt = -0.4091f;
    const float cosT = cosf(tilt);
    const float sinT = sinf(tilt);

    // Latitude Parallels
    const float lats[3] = { 0.0f, 0.41f, -0.41f };
    for (int l = 0; l < 3; l++) {
      float phi = lats[l];
      float rRing = cosf(phi) * (float)radius;
      float yRing = sinf(phi) * (float)radius;

      int prevX = 0, prevY = 0;
      bool prevVis = false;

      for (int s = 0; s <= 24; s++) {
        float theta = (float)s * (6.2831853f / 24.0f);
        float x0 = sinf(theta) * rRing;
        float y0 = yRing;
        float z0 = cosf(theta) * rRing;

        float x = x0 * cosT - y0 * sinT;
        float y = x0 * sinT + y0 * cosT;
        float z = z0;

        int sx = cx + (int)roundf(x);
        int sy = cy - (int)roundf(y);

        if (z > 0.0f) {
          if (prevVis) {
            uint16_t col = (l == 0) ? COLOR_ORANGE_MID : COLOR_ORANGE_DARK;
            canvas->drawLine(prevX, prevY, sx, sy, col);
          }
          prevVis = true;
        } else {
          prevVis = false;
        }
        prevX = sx;
        prevY = sy;
      }
    }

    // Rotating Longitude Meridians
    for (int m = 0; m < 4; m++) {
      float mAngle = (float)m * 0.785398f + rotRad;
      int prevX = 0, prevY = 0;
      bool prevVis = false;

      for (int s = -8; s <= 8; s++) {
        float phi = (float)s * (1.570796f / 8.0f);
        float rRing = cosf(phi) * (float)radius;
        float y0 = sinf(phi) * (float)radius;

        float x0 = sinf(mAngle) * rRing;
        float z0 = cosf(mAngle) * rRing;

        float x = x0 * cosT - y0 * sinT;
        float y = x0 * sinT + y0 * cosT;
        float z = z0;

        int sx = cx + (int)roundf(x);
        int sy = cy - (int)roundf(y);

        if (z > 0.0f) {
          if (prevVis) {
            canvas->drawLine(prevX, prevY, sx, sy, COLOR_ORANGE_DARK);
          }
          prevVis = true;
        } else {
          prevVis = false;
        }
        prevX = sx;
        prevY = sy;
      }
    }

    // Vector Continents
    static const GeoNode continentNodes[] = {
      // Europe & Africa
      { 70, 25 }, { 60, 5 }, { 50, -5 }, { 36, -6 }, { 30, -10 },
      { 15, -17 }, { 5, 0 }, { -15, 12 }, { -34, 18 }, { -30, 31 },
      { 0, 42 }, { 12, 51 }, { 30, 32 }, { 41, 28 }, { 55, 37 },
      { 70, 25 },
      // Asia
      { 70, 40 }, { 65, 80 }, { 60, 140 }, { 40, 120 }, { 22, 114 },
      { 10, 105 }, { 20, 80 }, { 25, 60 }, { 40, 50 }, { 70, 40 },
      // North America
      { 70, -140 }, { 60, -165 }, { 54, -130 }, { 35, -120 }, { 25, -110 },
      { 20, -105 }, { 10, -85 }, { 25, -80 }, { 35, -75 }, { 45, -65 },
      { 55, -60 }, { 70, -140 },
      // South America
      { 10, -75 }, { 0, -50 }, { -10, -37 }, { -23, -43 }, { -53, -68 },
      { -40, -73 }, { -15, -75 }, { -5, -80 }, { 10, -75 },
      // Australia
      { -15, 130 }, { -20, 115 }, { -35, 117 }, { -37, 140 }, { -25, 153 },
      { -15, 145 }, { -15, 130 }
    };

    int numNodes = sizeof(continentNodes) / sizeof(GeoNode);
    int pX = 0, pY = 0;
    bool pVis = false;

    for (int i = 0; i < numNodes; i++) {
      float latRad = (float)continentNodes[i].lat * 0.01745329f;
      float lonRad = (float)continentNodes[i].lon * 0.01745329f + rotRad;

      float cosLat = cosf(latRad);
      float sinLat = sinf(latRad);

      float x0 = cosLat * sinf(lonRad) * (float)radius;
      float y0 = sinLat * (float)radius;
      float z0 = cosLat * cosf(lonRad) * (float)radius;

      float x = x0 * cosT - y0 * sinT;
      float y = x0 * sinT + y0 * cosT;
      float z = z0;

      int sx = cx + (int)roundf(x);
      int sy = cy - (int)roundf(y);

      bool isLoopClose = (i > 0 && continentNodes[i].lat == continentNodes[i-1].lat && continentNodes[i].lon == continentNodes[i-1].lon);

      if (z > 0.0f) {
        if (pVis && !isLoopClose) {
          canvas->drawLine(pX, pY, sx, sy, COLOR_ORANGE_BRIGHT);
        }
        canvas->drawPixel(sx, sy, COLOR_ORANGE_BRIGHT);
        pVis = true;
      } else {
        pVis = false;
      }
      pX = sx;
      pY = sy;
    }

    // Polar Axis Ticks
    int npx1 = cx + (int)roundf(sinT * (float)(radius + 1));
    int npy1 = cy - (int)roundf(cosT * (float)(radius + 1));
    int npx2 = cx + (int)roundf(sinT * (float)(radius + 6));
    int npy2 = cy - (int)roundf(cosT * (float)(radius + 6));
    canvas->drawLine(npx1, npy1, npx2, npy2, COLOR_ORANGE_BRIGHT);

    int spx1 = cx - (int)roundf(sinT * (float)(radius + 1));
    int spy1 = cy + (int)roundf(cosT * (float)(radius + 1));
    int spx2 = cx - (int)roundf(sinT * (float)(radius + 6));
    int spy2 = cy + (int)roundf(cosT * (float)(radius + 6));
    canvas->drawLine(spx1, spy1, spx2, spy2, COLOR_ORANGE_BRIGHT);
  }

  void drawAnchorNode(int x, int y) {
    canvas->fillCircle(x, y, 3, COLOR_BG);
    canvas->drawCircle(x, y, 3, COLOR_ORANGE_MID);
    canvas->fillCircle(x, y, 1, COLOR_ORANGE_BRIGHT);
  }
};
