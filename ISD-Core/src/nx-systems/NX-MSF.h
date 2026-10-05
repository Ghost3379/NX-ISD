#pragma once
#include <Arduino.h>

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

  // 3. Environmental Barometric Weather Forecast (BME680 Pressure Trend)
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
};
