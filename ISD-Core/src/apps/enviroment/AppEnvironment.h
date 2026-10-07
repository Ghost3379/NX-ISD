#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "../../SensorState.h"
#include "../../HAL.h"
#include "../../nx-systems/NX-MSF.h"

enum EnvironmentCardId {
  ENV_CARD_MASTER_HUD = 0,
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

  // Gas Sensor Forced Pulse Animation
  bool isGasBurnActive = false;
  uint32_t gasBurnStartTime = 0;

public:
  AppEnvironment(LGFX* tft) : display(tft) {
    for (int i = 0; i < PRESSURE_HISTORY_SIZE; i++) {
      pressHistory[i] = 1013.25f;
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
      targetScrollY = (float)((int)currentCard * 240);
      HAL::buzzPip(2800, 12);
    }
  }

  void handleNavRight() {
    // Scroll DOWN through the vertical card stack
    if ((int)currentCard < ENV_CARD_COUNT - 1) {
      currentCard = (EnvironmentCardId)((int)currentCard + 1);
      targetScrollY = (float)((int)currentCard * 240);
      HAL::buzzPip(3200, 12);
    }
  }

  void handleNavPush() {
    if (currentCard == ENV_CARD_MASTER_HUD) {
      // Jump into Barometer deep dive
      currentCard = ENV_CARD_BARO_ALTITUDE;
      targetScrollY = 240.0f;
      HAL::buzzPip(3400, 15);
    } else if (currentCard == ENV_CARD_BARO_ALTITUDE) {
      // Toggle Altimeter Zero / Absolute QNH
      isRelativeAltZeroed = !isRelativeAltZeroed;
      HAL::buzzPip(isRelativeAltZeroed ? 4200 : 2600, 20);
    } else if (currentCard == ENV_CARD_CLIMATE_THERMAL) {
      // Toggle Celsius <-> Fahrenheit
      useFahrenheit = !useFahrenheit;
      HAL::buzzPip(3600, 15);
    } else if (currentCard == ENV_CARD_AIR_GAS) {
      // Trigger Forced-Mode Gas Burn Pulse
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

    // 4. Smooth Vertical Card Scroll Easing
    scrollY += (targetScrollY - scrollY) * 0.35f;
    if (fabsf(targetScrollY - scrollY) < 0.5f) {
      scrollY = targetScrollY;
    }

    // 5. Check gas burn timeout
    if (isGasBurnActive && (now - gasBurnStartTime > 2500)) {
      isGasBurnActive = false;
    }
  }

  void render(const SensorState& state) {
    if (!canvas) return;

    update(state);

    canvas->fillSprite(COLOR_BG);

    // Standard ISD-Core Technical Framing
    canvas->drawRect(4, 4, 232, 232, COLOR_ORANGE_DIM);
    canvas->drawFastHLine(4, 28, 232, COLOR_ORANGE_DIM);

    // Render Master Top System Header
    renderHeader(state);

    // Render Active Deck depending on currentCard
    switch (currentCard) {
      case ENV_CARD_MASTER_HUD:
        renderMasterHudCard(state);
        break;
      case ENV_CARD_BARO_ALTITUDE:
        renderBaroCard(state);
        break;
      case ENV_CARD_CLIMATE_THERMAL:
        renderClimateCard(state);
        break;
      case ENV_CARD_AIR_GAS:
        renderAirQualityCard(state);
        break;
      case ENV_CARD_LIGHT_LUX:
        renderPhotometricsCard(state);
        break;
      default:
        break;
    }

    // Bottom Navigation Bar
    renderBottomBar();

    // Push offscreen buffer to ST7789 display
    canvas->pushSprite(0, 0);
  }

private:
  void renderHeader(const SensorState& state) {
    canvas->setTextSize(1);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ISD-Core // ", 10, 10);

    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    const char* deckNames[ENV_CARD_COUNT] = {
      "ENVIRONMENT",
      "ATMOSPHERE",
      "CLIMATE",
      "AIR QUALITY",
      "PHOTOMETRICS"
    };
    canvas->drawString(deckNames[(int)currentCard], 78, 10);

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
    canvas->drawRightString(batBuf, 230, 10);
  }

  // =========================================================================
  // CARD 0: MASTER ENVIRONMENTAL COMMAND HUD
  // =========================================================================
  void renderMasterHudCard(const SensorState& state) {
    int cx = 120;
    int cy = 118;
    int globeR = 26;

    // 1. Render 3D Vector Rotating Earth Globe in the center
    draw3DVectorEarth(cx, cy, globeR, earthAngleRad);

    // 2. Blueprint Tactical Leader Lines with (O) Anchor Nodes
    // Corner Pod Dimensions:
    // Pod 1 (Top-Left, BARO):       x: 8,   y: 34, w: 98, h: 48
    // Pod 2 (Top-Right, CLIM):      x: 134, y: 34, w: 98, h: 48
    // Pod 3 (Bottom-Left, AIR):     x: 8,   y: 154, w: 98, h: 48
    // Pod 4 (Bottom-Right, LUX):    x: 134, y: 154, w: 98, h: 48

    // Anchor Nodes pinned directly to Earth's atmospheric perimeter
    int anchorNW_X = cx - 18, anchorNW_Y = cy - 18;
    int anchorNE_X = cx + 18, anchorNE_Y = cy - 18;
    int anchorSW_X = cx - 18, anchorSW_Y = cy + 18;
    int anchorSE_X = cx + 18, anchorSE_Y = cy + 18;

    // Draw Traces:
    // NW Trace: Pod1 bottom-right (106, 82) -> (anchorNW_X, 82) -> (anchorNW_X, anchorNW_Y)
    canvas->drawLine(106, 76, anchorNW_X, 76, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorNW_X, 76, anchorNW_X, anchorNW_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorNW_X, anchorNW_Y);

    // NE Trace: Pod2 bottom-left (134, 76) -> (anchorNE_X, 76) -> (anchorNE_X, anchorNE_Y)
    canvas->drawLine(134, 76, anchorNE_X, 76, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorNE_X, 76, anchorNE_X, anchorNE_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorNE_X, anchorNE_Y);

    // SW Trace: Pod3 top-right (106, 160) -> (anchorSW_X, 160) -> (anchorSW_X, anchorSW_Y)
    canvas->drawLine(106, 160, anchorSW_X, 160, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorSW_X, 160, anchorSW_X, anchorSW_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorSW_X, anchorSW_Y);

    // SE Trace: Pod4 top-left (134, 160) -> (anchorSE_X, 160) -> (anchorSE_X, anchorSE_Y)
    canvas->drawLine(134, 160, anchorSE_X, 160, COLOR_ORANGE_DARK);
    canvas->drawLine(anchorSE_X, 160, anchorSE_X, anchorSE_Y, COLOR_ORANGE_DARK);
    drawAnchorNode(anchorSE_X, anchorSE_Y);

    // ================= POD 1: BARO (Top-Left) =================
    int p1x = 8, p1y = 34, p1w = 98, p1h = 48;
    canvas->drawRoundRect(p1x, p1y, p1w, p1h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("ATMOSPHERE", p1x + 6, p1y + 5);

    char valBuf[20];
    if (state.envDataReady && state.press > 300.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0fhPa", state.press);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p1x + 6, p1y + 17, &fonts::Font2);

      // Micro forecast badge
      const char* fc = (state.press > 1018.0f) ? "HIGH" : (state.press < 1005.0f) ? "STORM" : "FAIR";
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(fc, p1x + 6, p1y + 33);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("----hPa", p1x + 6, p1y + 17, &fonts::Font2);
      canvas->drawString("NO SENSOR", p1x + 6, p1y + 33);
    }

    // ================= POD 2: CLIMATE (Top-Right) =================
    int p2x = 134, p2y = 34, p2w = 98, p2h = 48;
    canvas->drawRoundRect(p2x, p2y, p2w, p2h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("CLIMATE", p2x + 6, p2y + 5);

    if (state.envDataReady && state.temp > -40.0f) {
      float t = useFahrenheit ? (state.temp * 1.8f + 32.0f) : state.temp;
      snprintf(valBuf, sizeof(valBuf), "%.1f%s", t, useFahrenheit ? "F" : "C");
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p2x + 6, p2y + 17, &fonts::Font2);

      snprintf(valBuf, sizeof(valBuf), "%.0f%% RH", state.hum);
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(valBuf, p2x + 6, p2y + 33);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--.- C", p2x + 6, p2y + 17, &fonts::Font2);
      canvas->drawString("--% RH", p2x + 6, p2y + 33);
    }

    // ================= POD 3: AIR QUALITY (Bottom-Left) =================
    int p3x = 8, p3y = 154, p3w = 98, p3h = 48;
    canvas->drawRoundRect(p3x, p3y, p3w, p3h, 3, COLOR_ORANGE_DIM);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("AIR QUALITY", p3x + 6, p3y + 5);

    if (state.envDataReady && state.gas > 0.0f) {
      snprintf(valBuf, sizeof(valBuf), "%.0fk", state.gas / 1000.0f);
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString(valBuf, p3x + 6, p3y + 17, &fonts::Font2);

      // Micro 5-segment rating
      int segs = constrain((int)(state.gas / 40000.0f), 1, 5);
      for (int s = 0; s < 5; s++) {
        if (s < segs) {
          canvas->fillRect(p3x + 6 + s * 8, p3y + 35, 6, 5, COLOR_ORANGE_BRIGHT);
        } else {
          canvas->drawRect(p3x + 6 + s * 8, p3y + 35, 6, 5, COLOR_ORANGE_DARK);
        }
      }
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--.- k", p3x + 6, p3y + 17, &fonts::Font2);
      canvas->drawString("NO GAS", p3x + 6, p3y + 33);
    }

    // ================= POD 4: PHOTOMETRICS (Bottom-Right) =================
    int p4x = 134, p4y = 154, p4w = 98, p4h = 48;
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
      canvas->drawString(valBuf, p4x + 6, p4y + 17, &fonts::Font2);

      const char* tag = (state.lightLux > 1200.0f) ? "SUNLIGHT" : (state.lightLux > 80.0f) ? "INDOORS" : "NIGHT";
      canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
      canvas->drawString(tag, p4x + 6, p4y + 33);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("--- lx", p4x + 6, p4y + 17, &fonts::Font2);
      canvas->drawString("OPT3001 OFF", p4x + 6, p4y + 33);
    }
  }

  // =========================================================================
  // CARD 1: BAROMETER & MOUNTAIN ALTITUDE SCHEMATIC
  // =========================================================================
  void renderBaroCard(const SensorState& state) {
    float press = (state.envDataReady && state.press > 300.0f) ? state.press : 1013.25f;

    // Calculate Elevation from barometric hypsometric formula
    float rawAltitudeM = 44330.0f * (1.0f - powf(press / qnhBaselineHpa, 0.190294957f));
    if (isRelativeAltZeroed && altZeroOffsetMeters == 0.0f) {
      altZeroOffsetMeters = rawAltitudeM;
    }
    float altitudeM = isRelativeAltZeroed ? (rawAltitudeM - altZeroOffsetMeters) : rawAltitudeM;
    float altitudeFt = altitudeM * 3.28084f;

    // 1. Digital Telemetry Readout
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("BARO PRESSURE:", 12, 34);
    char pBuf[32];
    snprintf(pBuf, sizeof(pBuf), "%.1f hPa", press);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(pBuf, 116, 34, &fonts::Font2);

    // Weather Trend Badge
    const char* trendStr = (press > 1018.0f) ? "[ CLEARING / HIGH ]" : (press < 1005.0f) ? "[ STORM WARNING ]" : "[ STABLE / FAIR ]";
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawRightString(trendStr, 228, 52);

    // 2. Vector Mountain Peak & Elevation Schematic
    // Ground level: y = 126. Peak: (x: 58, y: 72)
    int groundY = 126;
    canvas->drawFastHLine(10, groundY, 220, COLOR_ORANGE_DARK);

    // Mountain silhouettes (Layered Wireframe Peaks)
    // Mountain 1 (Background): (x: 104, y: 80)
    canvas->drawLine(64, groundY, 104, 78, COLOR_ORANGE_DARK);
    canvas->drawLine(104, 78, 144, groundY, COLOR_ORANGE_DARK);

    // Mountain 2 (Foreground Primary): (x: 54, y: 68)
    canvas->drawLine(14, groundY, 54, 66, COLOR_ORANGE_MID);
    canvas->drawLine(54, 66, 94, groundY, COLOR_ORANGE_MID);
    // Summit tick
    canvas->drawCircle(54, 66, 2, COLOR_ORANGE_BRIGHT);

    // Elevation Horizon Target Line: dynamic height based on altitude (-100m to 1200m -> mapped to groundY to 72)
    int targetY = map((long)constrain(altitudeM, -50.0f, 1000.0f), -50L, 1000L, groundY, 70L);
    canvas->drawFastHLine(54, targetY, 172, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(54, targetY, 3, COLOR_ORANGE_BRIGHT);

    // Elevation Readouts on Target Line
    char altBuf[32];
    snprintf(altBuf, sizeof(altBuf), "%+.0f m", altitudeM);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(altBuf, 116, targetY - 14, &fonts::Font2);

    snprintf(altBuf, sizeof(altBuf), "(%+.0f ft)", altitudeFt);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(altBuf, 176, targetY - 10);

    const char* modeTag = isRelativeAltZeroed ? "[REL ZERO]" : "[QNH 1013]";
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString(modeTag, 116, targetY + 3);

    // 3. 16-Bar Historical Isobaric Sparkline (y: 138 .. 198)
    canvas->drawFastHLine(10, 136, 220, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("ISOBARIC HISTORY (16 SAMPLES):", 12, 140);

    int chartX = 14;
    int chartY = 196;
    int barW = 10;
    int barGap = 3;

    // Find min/max in history buffer
    float minP = 1050.0f, maxP = 950.0f;
    for (int i = 0; i < PRESSURE_HISTORY_SIZE; i++) {
      if (pressHistory[i] < minP) minP = pressHistory[i];
      if (pressHistory[i] > maxP) maxP = pressHistory[i];
    }
    if (maxP - minP < 2.0f) {
      maxP = minP + 2.0f;
    }

    for (int b = 0; b < PRESSURE_HISTORY_SIZE; b++) {
      int idx = (pressHistoryHead + b) % PRESSURE_HISTORY_SIZE;
      float val = pressHistory[idx];
      int barH = map((long)(val * 10.0f), (long)(minP * 10.0f), (long)(maxP * 10.0f), 4L, 34L);
      int bx = chartX + b * (barW + barGap);
      int by = chartY - barH;

      bool isLatest = (b == PRESSURE_HISTORY_SIZE - 1);
      if (isLatest) {
        canvas->fillRect(bx, by, barW, barH, COLOR_ORANGE_BRIGHT);
      } else {
        canvas->drawRect(bx, by, barW, barH, COLOR_ORANGE_MID);
      }
    }
  }

  // =========================================================================
  // CARD 2: CLIMATE & THERMOMETER SCHEMATIC
  // =========================================================================
  void renderClimateCard(const SensorState& state) {
    float tempC = (state.envDataReady && state.temp > -40.0f) ? state.temp : 22.0f;
    float hum = (state.envDataReady && state.hum > 0.0f) ? state.hum : 45.0f;

    // Calculate Dew Point (Magnus-Tetens formula approximation)
    float a = 17.27f, b = 237.7f;
    float alpha = ((a * tempC) / (b + tempC)) + logf(hum / 100.0f);
    float dewPointC = (b * alpha) / (a - alpha);

    float dispTemp = useFahrenheit ? (tempC * 1.8f + 32.0f) : tempC;
    float dispDew  = useFahrenheit ? (dewPointC * 1.8f + 32.0f) : dewPointC;

    // 1. Vector Thermometer Capillary Tube (Left: x = 14 .. 54)
    int tubeX = 28;
    int tubeTopY = 40;
    int tubeBotY = 160;
    int bulbY = 174;
    int bulbR = 12;

    // Glass bulb & stem outline
    canvas->drawRoundRect(tubeX - 4, tubeTopY, 8, tubeBotY - tubeTopY, 4, COLOR_ORANGE_MID);
    canvas->fillCircle(tubeX, bulbY, bulbR, COLOR_ORANGE_DARK);
    canvas->drawCircle(tubeX, bulbY, bulbR, COLOR_ORANGE_BRIGHT);

    // Mercury fluid height (-10C to +50C)
    int mercuryH = map((long)constrain(tempC, -10.0f, 50.0f), -10L, 50L, 6L, (long)(tubeBotY - tubeTopY - 8));
    int mercuryTopY = tubeBotY - mercuryH;
    canvas->fillRect(tubeX - 2, mercuryTopY, 4, mercuryH + 6, COLOR_ORANGE_BRIGHT);
    canvas->fillCircle(tubeX, bulbY, bulbR - 3, COLOR_ORANGE_BRIGHT);

    // Thermometer etched tick marks
    canvas->drawFastHLine(tubeX + 6, tubeTopY + 10, 6, COLOR_ORANGE_DIM);
    canvas->drawString("40", tubeX + 14, tubeTopY + 7);
    canvas->drawFastHLine(tubeX + 6, tubeTopY + (tubeBotY - tubeTopY)/2, 8, COLOR_ORANGE_MID);
    canvas->drawString("20", tubeX + 16, tubeTopY + (tubeBotY - tubeTopY)/2 - 3);
    canvas->drawFastHLine(tubeX + 6, tubeBotY - 12, 6, COLOR_ORANGE_DIM);
    canvas->drawString("0", tubeX + 14, tubeBotY - 15);

    // 2. Large Temperature & Humidity Primary Readouts
    char tBuf[32];
    snprintf(tBuf, sizeof(tBuf), "%.1f%s", dispTemp, useFahrenheit ? "°F" : "°C");
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(tBuf, 68, 38, &fonts::Font4);

    snprintf(tBuf, sizeof(tBuf), "%.0f%% RH", hum);
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString(tBuf, 68, 72, &fonts::Font2);

    snprintf(tBuf, sizeof(tBuf), "DEW POINT: %.1f%s", dispDew, useFahrenheit ? "°F" : "°C");
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString(tBuf, 68, 92);

    // Session Min/Max
    if (minRecordedTemp < 100.0f) {
      float minD = useFahrenheit ? (minRecordedTemp * 1.8f + 32.0f) : minRecordedTemp;
      float maxD = useFahrenheit ? (maxRecordedTemp * 1.8f + 32.0f) : maxRecordedTemp;
      char mmBuf[32];
      snprintf(mmBuf, sizeof(mmBuf), "MIN:%.1f°  MAX:%.1f°", minD, maxD);
      canvas->drawString(mmBuf, 68, 108);
    }

    // 3. Psychrometric Comfort Matrix (y: 130 .. 198)
    int boxX = 64, boxY = 132, boxW = 164, boxH = 62;
    canvas->drawRect(boxX, boxY, boxW, boxH, COLOR_ORANGE_DARK);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("PSYCHROMETRIC COMFORT ENVELOPE", boxX + 4, boxY + 4);

    // Optimal Comfort Target Box (20C..26C, 30%..60% RH)
    int optX1 = boxX + map(20, 0, 40, 10, boxW - 10);
    int optX2 = boxX + map(26, 0, 40, 10, boxW - 10);
    int optY1 = boxY + boxH - map(60, 10, 90, 8, boxH - 12);
    int optY2 = boxY + boxH - map(30, 10, 90, 8, boxH - 12);
    canvas->drawRect(optX1, optY1, optX2 - optX1, optY2 - optY1, COLOR_ORANGE_MID);
    canvas->drawString("OPTIMAL", optX1 + 4, optY1 + 4);

    // Current State Dot
    int curDotX = boxX + map((long)constrain(tempC, 0.0f, 40.0f), 0L, 40L, 10L, (long)(boxW - 10));
    int curDotY = boxY + boxH - map((long)constrain(hum, 10.0f, 90.0f), 10L, 90L, 8L, (long)(boxH - 12));
    canvas->fillCircle(curDotX, curDotY, 3, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(curDotX, curDotY, 5, COLOR_ORANGE_BRIGHT);
  }

  // =========================================================================
  // CARD 3: AIR QUALITY & VOC GAS CHAMBER SCHEMATIC
  // =========================================================================
  void renderAirQualityCard(const SensorState& state) {
    float gasVal = (state.envDataReady && state.gas > 0.0f) ? (state.gas / 1000.0f) : 142.0f;

    // 1. BME680 Sensor Chamber Blueprint Schematic
    int chX = 14, chY = 36, chW = 212, chH = 64;
    canvas->drawRoundRect(chX, chY, chW, chH, 4, COLOR_ORANGE_MID);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("BME680 MOX CHAMBER SCHEMATIC", chX + 6, chY + 5);

    // Gas Diffusion Mesh Layer
    canvas->drawFastHLine(chX + 8, chY + 18, chW - 16, COLOR_ORANGE_MID);
    canvas->drawString("DIFFUSION MESH (VOC INFLUX)", chX + 10, chY + 22);

    // Metal-Oxide Sensing Surface
    canvas->fillRect(chX + 8, chY + 34, chW - 16, 6, COLOR_ORANGE_DARK);
    canvas->drawFastHLine(chX + 8, chY + 34, chW - 16, COLOR_ORANGE_MID);

    // Micro-Hotplate Meander Heater Track (300°C)
    int heaterY = chY + 48;
    canvas->setTextColor(isGasBurnActive ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("HEATER COIL: 300°C", chX + 10, heaterY);
    // Draw meander pulse coil
    for (int k = 0; k < 6; k++) {
      int hx = chX + 140 + k * 10;
      canvas->drawLine(hx, heaterY, hx + 4, heaterY + 6, isGasBurnActive ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
      canvas->drawLine(hx + 4, heaterY + 6, hx + 8, heaterY, isGasBurnActive ? COLOR_ORANGE_BRIGHT : COLOR_ORANGE_MID);
    }

    // 2. Gas Resistance Metric Readout
    char gBuf[32];
    snprintf(gBuf, sizeof(gBuf), "%.1f kΩ", gasVal);
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(gBuf, 14, 108, &fonts::Font4);

    const char* iaqLabel = (gasVal > 150.0f) ? "EXCELLENT" : (gasVal > 80.0f) ? "GOOD" : (gasVal > 40.0f) ? "MODERATE" : "POOR / VENT";
    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawRightString(iaqLabel, 226, 116, &fonts::Font2);

    // 3. Wide 8-Segment Tactical IAQ Meter
    int segW = 23;
    int segH = 12;
    int segGap = 4;
    int segStartX = 14;
    int segY = 142;

    int activeSegs = map((long)constrain(gasVal, 20.0f, 250.0f), 20L, 250L, 1L, 8L);
    for (int s = 0; s < 8; s++) {
      int sx = segStartX + s * (segW + segGap);
      if (s < activeSegs) {
        canvas->fillRect(sx, segY, segW, segH, COLOR_ORANGE_BRIGHT);
      } else {
        canvas->drawRect(sx, segY, segW, segH, COLOR_ORANGE_DARK);
      }
    }

    // Legend
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("HAZARDOUS", 14, 160);
    canvas->drawString("MODERATE", 96, 160);
    canvas->drawRightString("CLEAN AIR", 226, 160);

    // Burn status alert
    if (isGasBurnActive) {
      canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
      canvas->drawString("[ PULSING MOX HEATER... 300°C ]", 36, 178);
    } else {
      canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
      canvas->drawString("PRESS LEVER TO TRIGGER FORCED SCAN", 16, 178);
    }
  }

  // =========================================================================
  // CARD 4: PHOTOMETRICS & LIGHT INTENSITY SCHEMATIC
  // =========================================================================
  void renderPhotometricsCard(const SensorState& state) {
    float lux = (state.lightLux >= 0.0f) ? state.lightLux : 450.0f;

    // 1. Optical Iris Aperture Graphic in the Center (x = 64, y = 84)
    int irisX = 64, irisY = 84, irisR = 28;
    canvas->drawCircle(irisX, irisY, irisR, COLOR_ORANGE_MID);
    canvas->drawCircle(irisX, irisY, irisR + 4, COLOR_ORANGE_DARK);

    // Draw rotating aperture iris blades based on lux
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
    canvas->drawString(lBuf, 114, 52, &fonts::Font4);

    canvas->setTextColor(COLOR_ORANGE_MID, COLOR_BG);
    canvas->drawString("LUX (LUMENS/M²)", 114, 82);

    const char* zone = (lux > 5000.0f) ? "[ DIRECT SUN ]" : (lux > 1000.0f) ? "[ OVERCAST ]" : (lux > 200.0f) ? "[ INDOOR WORK ]" : (lux > 30.0f) ? "[ DIM ROOM ]" : "[ NIGHT ]";
    canvas->setTextColor(COLOR_ORANGE_BRIGHT, COLOR_BG);
    canvas->drawString(zone, 114, 98);

    // 3. 180° Celestial Daylight Horizon Arc (y: 130 .. 198)
    int arcX = 120, arcY = 196, arcR = 64;
    canvas->drawCircle(arcX, arcY, arcR, COLOR_ORANGE_DARK);
    canvas->drawFastHLine(14, arcY, 212, COLOR_ORANGE_DARK);

    // Sun pip position along the arc (mapped logarithmically from 0 to 10000 lux)
    float logLux = log10f(constrain(lux, 1.0f, 10000.0f)); // 0.0 to 4.0
    float angleRad = 3.14159265f - (logLux / 4.0f) * 3.14159265f; // pi to 0
    int sunX = arcX + (int)(cosf(angleRad) * arcR);
    int sunY = arcY - (int)(sinf(angleRad) * arcR);

    // Draw glowing sun pip
    canvas->fillCircle(sunX, sunY, 4, COLOR_ORANGE_BRIGHT);
    canvas->drawCircle(sunX, sunY, 7, COLOR_ORANGE_BRIGHT);

    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);
    canvas->drawString("NIGHT", 14, arcY + 4);
    canvas->drawString("INDOOR", 100, arcY - 48);
    canvas->drawRightString("DAYLIGHT", 226, arcY + 4);
  }

  // =========================================================================
  // 3D VECTOR EARTH MATHEMATICAL ENGINE
  // =========================================================================
  void draw3DVectorEarth(int cx, int cy, int radius, float rotRad) {
    // 1. Outer Exosphere Halo Aura
    canvas->drawCircle(cx, cy, radius + 4, COLOR_ORANGE_DARK);
    canvas->drawCircle(cx, cy, radius, COLOR_ORANGE_MID);

    // Earth axial tilt (23.44 degrees = 0.4091 radians)
    const float tilt = -0.4091f;
    const float cosT = cosf(tilt);
    const float sinT = sinf(tilt);

    // 2. Latitude Parallels (Equator, Tropic of Cancer, Tropic of Capricorn)
    const float lats[3] = { 0.0f, 0.41f, -0.41f };
    for (int l = 0; l < 3; l++) {
      float phi = lats[l];
      float rRing = cosf(phi) * (float)radius;
      float yRing = sinf(phi) * (float)radius;

      // Sample circle around the ring
      int prevX = 0, prevY = 0;
      bool prevVis = false;

      for (int s = 0; s <= 24; s++) {
        float theta = (float)s * (6.2831853f / 24.0f);
        float x0 = sinf(theta) * rRing;
        float y0 = yRing;
        float z0 = cosf(theta) * rRing;

        // Apply axial tilt around Z axis
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

    // 3. Rotating Longitude Meridians (4 meridians spaced 45 deg apart)
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

        // Tilt
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

    // 4. Vector Continents Polyline Array
    // Compact geographical node coordinates (lat, lon) in degrees
    static const GeoNode continentNodes[] = {
      // Europe & Africa Coastline Outline
      { 70, 25 }, { 60, 5 }, { 50, -5 }, { 36, -6 }, { 30, -10 },
      { 15, -17 }, { 5, 0 }, { -15, 12 }, { -34, 18 }, { -30, 31 },
      { 0, 42 }, { 12, 51 }, { 30, 32 }, { 41, 28 }, { 55, 37 },
      { 70, 25 }, // Loop back
      // Asia Coastline
      { 70, 40 }, { 65, 80 }, { 60, 140 }, { 40, 120 }, { 22, 114 },
      { 10, 105 }, { 20, 80 }, { 25, 60 }, { 40, 50 }, { 70, 40 },
      // North America Outline
      { 70, -140 }, { 60, -165 }, { 54, -130 }, { 35, -120 }, { 25, -110 },
      { 20, -105 }, { 10, -85 }, { 25, -80 }, { 35, -75 }, { 45, -65 },
      { 55, -60 }, { 70, -140 },
      // South America Outline
      { 10, -75 }, { 0, -50 }, { -10, -37 }, { -23, -43 }, { -53, -68 },
      { -40, -73 }, { -15, -75 }, { -5, -80 }, { 10, -75 },
      // Australia Outline
      { -15, 130 }, { -20, 115 }, { -35, 117 }, { -37, 140 }, { -25, 153 },
      { -15, 145 }, { -15, 130 }
    };

    // Draw continents with 3D projection
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

      // Tilt
      float x = x0 * cosT - y0 * sinT;
      float y = x0 * sinT + y0 * cosT;
      float z = z0;

      int sx = cx + (int)roundf(x);
      int sy = cy - (int)roundf(y);

      // Check if this node closes a loop
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

    // 5. Polar Axis Ticks (23.4 deg tilted North and South poles)
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

  void renderBottomBar() {
    canvas->drawFastHLine(4, 206, 232, COLOR_ORANGE_DARK);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COLOR_ORANGE_DIM, COLOR_BG);

    if (currentCard == ENV_CARD_MASTER_HUD) {
      canvas->drawString("[▼] SCROLL FOR DETAILS   [BTN] EXIT", 120, 220);
    } else if (currentCard == ENV_CARD_BARO_ALTITUDE) {
      canvas->drawString("[PUSH] ZERO ELEV   ▲/▼ SCROLL   [BTN] HOME", 120, 220);
    } else if (currentCard == ENV_CARD_CLIMATE_THERMAL) {
      canvas->drawString("[PUSH] °C / °F     ▲/▼ SCROLL   [BTN] HOME", 120, 220);
    } else if (currentCard == ENV_CARD_AIR_GAS) {
      canvas->drawString("[PUSH] BURN HEATER ▲/▼ SCROLL   [BTN] HOME", 120, 220);
    } else if (currentCard == ENV_CARD_LIGHT_LUX) {
      canvas->drawString("[PUSH] ZOOM SCALE  ▲/▼ SCROLL   [BTN] HOME", 120, 220);
    }
    canvas->setTextDatum(TL_DATUM);
  }
};
