#pragma once
#include <Arduino.h>

// Standardized Meteorological Conditions across ISD-Core
enum WeatherCondition {
  WEATHER_CLEAR = 0,
  WEATHER_PARTLY_CLOUDY,
  WEATHER_OVERCAST,
  WEATHER_RAIN,
  WEATHER_STORM
};

class NX_MSF {
public:
  // 1. Multi-Sensor Fusion Stress Calculation (HRV RMSSD + Resting HR + Motion Stability)
  static int calculateStress(const uint32_t* beatIntervals, int maxIntervals, int beatCount, int headIdx, float currentBpm, float motionFactor = 1.0f) {
    if (beatCount < 4 || currentBpm < 40.0f) return -1;

    int availablePairs = min(beatCount - 1, maxIntervals - 1);
    float sumSqDiff = 0.0f;
    int pairs = 0;

    for (int i = 1; i <= availablePairs; i++) {
      int idxCurr = (headIdx - i + maxIntervals) % maxIntervals;
      int idxPrev = (headIdx - i - 1 + maxIntervals) % maxIntervals;
      int diff = (int)beatIntervals[idxCurr] - (int)beatIntervals[idxPrev];
      sumSqDiff += (float)(diff * diff);
      pairs++;
    }

    float rmssd = (pairs > 0) ? sqrtf(sumSqDiff / (float)pairs) : 35.0f;
    // High RMSSD (relaxed, parasympathetic tone) = Low Stress; Low RMSSD = High Stress
    float hrvStress = map(constrain((long)roundf(rmssd), 15L, 75L), 15L, 75L, 85L, 15L);
    // Elevated Resting Heart Rate component
    float rhrStress = map(constrain((long)roundf(currentBpm), 55L, 110L), 55L, 110L, 10L, 90L);

    // Final Weighted Fusion Formula
    float fusedStress = (hrvStress * 0.65f) + (rhrStress * 0.35f);
    return (int)constrain(roundf(fusedStress * motionFactor), 5.0f, 95.0f);
  }

  // 2. Optical SpO2 Blood Oxygen Estimation
  static float estimateSpO2(uint32_t redSum, uint32_t irSum, int sampleCount) {
    if (sampleCount <= 0 || irSum == 0) return 98.0f;
    float r = ((float)redSum / (float)sampleCount) / ((float)irSum / (float)sampleCount);
    float est = 108.5f - 18.0f * r;
    return constrain(est, 94.0f, 100.0f);
  }

  // 3. Environmental Barometric Weather Forecast (BME690 Pressure Trend)
  static const char* estimateWeatherTrend(float currentPressureHpa, float pastPressureHpa) {
    float diff = currentPressureHpa - pastPressureHpa;
    if (diff > 1.5f)  return "CLEARING / HIGH";
    if (diff < -1.5f) return "STORM WARNING";
    if (diff < -0.5f) return "PRECIPITATION";
    return "STABLE / FAIR";
  }

  // 4. Ambient Lux to Display Brightness Auto-Dimming (Logarithmic + Low-Pass Filter)
  static int calculateAutoDim(float ambientLux, float& smoothedLux) {
    float lux = constrain(ambientLux, 0.0f, 10000.0f);
    // Smooth low-pass filter (90% historical, 10% instant)
    smoothedLux = (smoothedLux * 0.90f) + (lux * 0.10f);

    // Weber-Fechner logarithmic eye response curve:
    // 0 lux -> 10%, 10 lux -> 35%, 100 lux -> 60%, 500 lux -> 80%, 2000+ lux -> 100%
    float logVal = log10f(smoothedLux + 1.0f); // 0.0 to 4.0
    float targetPct = 10.0f + (logVal / 3.3f) * 90.0f;
    return (int)constrain(roundf(targetPct), 10.0f, 100.0f);
  }

  // 5. Hypsometric Altitude Formula (meters) from Barometric Pressure & QNH Baseline
  static float calculateAltitude(float pressureHpa, float qnhBaselineHpa = 1013.25f) {
    if (pressureHpa <= 300.0f || qnhBaselineHpa <= 300.0f) return 0.0f;
    return 44330.0f * (1.0f - powf(pressureHpa / qnhBaselineHpa, 0.190294957f));
  }

  // 6. Magnus-Tetens Dew Point Calculation (°C)
  // Accurate to ±0.35°C over -45°C to +60°C range
  static float calculateDewPoint(float tempC, float humidityPct) {
    if (humidityPct <= 0.01f) return tempC - 40.0f;
    float clampedHum = constrain(humidityPct, 1.0f, 100.0f);
    const float a = 17.27f;
    const float b = 237.7f;
    float alpha = ((a * tempC) / (b + tempC)) + logf(clampedHum / 100.0f);
    return (b * alpha) / (a - alpha);
  }

  // 7. Dew Point Margin & Condensation / Trail Fog Sentinel (°C)
  static float calculateFogMargin(float tempC, float dewPointC) {
    return tempC - dewPointC;
  }

  // 8. Multi-Factor Meteorological Condition Classifier (BME690 Multi-Variable Fusion)
  static WeatherCondition estimateWeatherCondition(float pressHpa, float humPct, float delta3h = 0.0f) {
    if (pressHpa < 1004.0f || delta3h < -2.0f) {
      return WEATHER_STORM;
    } else if (pressHpa < 1009.0f || humPct > 85.0f || delta3h < -1.0f) {
      return WEATHER_RAIN;
    } else if (humPct > 70.0f || pressHpa < 1013.0f) {
      return WEATHER_OVERCAST;
    } else if (humPct > 50.0f) {
      return WEATHER_PARTLY_CLOUDY;
    } else {
      return WEATHER_CLEAR;
    }
  }

  // 9. Canadian Humidex / Perceived Thermal Comfort Index (°C)
  // Humidex = T + (5/9) * (e - 10), where e is vapor pressure in mbar
  static float calculateHumidex(float tempC, float humidityPct) {
    if (humidityPct <= 0.0f) return tempC;
    float clampedHum = constrain(humidityPct, 1.0f, 100.0f);
    // Vapor pressure e via Tetens formula
    float e = 6.112f * powf(10.0f, (7.5f * tempC) / (237.3f + tempC)) * (clampedHum / 100.0f);
    float humidex = tempC + (5.0f / 9.0f) * (e - 10.0f);
    return max(tempC, humidex);
  }

  // 10. Multi-Factor Air Quality & MOX Gas Fusion (BME690 Environmental Engine)
  struct AirQualityAnalysis {
    float rawGasKohm;           // Raw MOX resistance (kΩ)
    float compGasKohm;          // Moisture & thermal cross-compensated resistance (kΩ)
    float baselineKohm;         // Adaptive clean-air baseline R0 (kΩ)
    float airPurityPct;         // 0.0% to 100.0%
    int iaqScore;               // 0 to 500 (Air Quality Index)
    const char* iaqTier;        // "EXCELLENT", "GOOD", "MODERATE", "POOR", "HAZARDOUS"
    float estEco2Ppm;           // Estimated eCO2 (400 - 3600 ppm)
    float estBvocPpm;           // Estimated bVOC (0.05 - 5.0 ppm)
    const char* eventStatus;    // "AIR STEADY", "PLUME SPIKE ⚠️", "PURGING AIR"
    float rateOfChangeKohmPerS; // ΔR/Δt (trend)
  };

  static AirQualityAnalysis calculateAirQuality(float rawGasOhms, float tempC, float humPct,
                                                float& baselineKohm, float pastGasKohm = 0.0f,
                                                uint32_t deltaMs = 1000) {
    AirQualityAnalysis res;

    // Convert raw Ohms to kΩ
    res.rawGasKohm = (rawGasOhms > 0.0f) ? (rawGasOhms / 1000.0f) : 150.0f;

    // 1. Humidity & Temperature Cross-Compensation (Reference: 20°C, 40% RH)
    // SnO2 surface oxidation efficiency varies with ambient water vapor concentration
    float humFactor = 1.0f + 0.005f * (humPct - 40.0f) + 0.002f * (tempC - 20.0f);
    humFactor = constrain(humFactor, 0.70f, 1.40f);
    res.compGasKohm = res.rawGasKohm * humFactor;

    // 2. Adaptive Clean-Air Baseline Tracking (R0)
    if (baselineKohm < 20.0f || baselineKohm > 2000.0f) {
      baselineKohm = max(res.compGasKohm, 150.0f);
    } else if (res.compGasKohm > baselineKohm) {
      // Fast adaptation to cleaner air
      baselineKohm = (baselineKohm * 0.95f) + (res.compGasKohm * 0.05f);
    } else {
      // Slow baseline retention decay (sensor drift compensation)
      baselineKohm = (baselineKohm * 0.9998f) + (res.compGasKohm * 0.0002f);
    }
    baselineKohm = max(baselineKohm, 50.0f);
    res.baselineKohm = baselineKohm;

    // 3. Air Purity Ratio (rho)
    float rho = constrain(res.compGasKohm / baselineKohm, 0.05f, 1.0f);
    res.airPurityPct = rho * 100.0f;

    // 4. IAQ Index (0 - 500, German UBA Scale)
    float iaqEst = 500.0f * (1.0f - powf(rho, 0.65f));
    res.iaqScore = (int)constrain(roundf(iaqEst), 15.0f, 500.0f);

    if (res.iaqScore <= 50) {
      res.iaqTier = "EXCELLENT";
    } else if (res.iaqScore <= 100) {
      res.iaqTier = "GOOD";
    } else if (res.iaqScore <= 150) {
      res.iaqTier = "MODERATE";
    } else if (res.iaqScore <= 250) {
      res.iaqTier = "POOR";
    } else {
      res.iaqTier = "HAZARDOUS";
    }

    // 5. Estimated eCO2 (400 - 3600 ppm) from VOC metabolic correlation
    res.estEco2Ppm = constrain(400.0f + 1600.0f * powf(1.0f - rho, 1.2f) * 1.8f, 400.0f, 3600.0f);

    // 6. Estimated bVOC (0.05 - 5.0 ppm)
    res.estBvocPpm = constrain(0.05f + 2.8f * (1.0f - rho), 0.05f, 5.0f);

    // 7. VOC Plume Sentinel & Rate of Change (ΔR / Δt)
    if (pastGasKohm > 0.0f && deltaMs > 200) {
      res.rateOfChangeKohmPerS = (res.compGasKohm - pastGasKohm) / ((float)deltaMs / 1000.0f);
    } else {
      res.rateOfChangeKohmPerS = 0.0f;
    }

    if (res.rateOfChangeKohmPerS < -6.0f) {
      res.eventStatus = "PLUME SPIKE ⚠️";
    } else if (res.rateOfChangeKohmPerS > 4.0f) {
      res.eventStatus = "PURGING AIR";
    } else {
      res.eventStatus = "AIR STEADY";
    }

    return res;
  }
};
