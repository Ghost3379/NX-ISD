#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "SensorState.h"
#include "HAL.h"
#include "pins.h"

enum MatrixPattern {
  PATTERN_OFF = 0,
  PATTERN_CYBER_RADAR,
  PATTERN_MATRIX_RAIN,
  PATTERN_SPECTRUM_PLASMA,
  PATTERN_NEON_TRACER,
  PATTERN_GLYPH_BREATH,
  PATTERN_QUANTUM_RIPPLE,
  PATTERN_CUSTOM
};

class UplinkBridge {
private:
  static bool streamActive;
  static uint32_t streamIntervalMs;
  static uint32_t lastStreamTime;
  static uint32_t packetsSent;
  static String lastCommand;
  static uint32_t lastCommandTime;
  static bool buzzerEnabled;
  static uint8_t displayBrightness;

  // NeoPixel matrix state
  static bool matrixPower;
  static uint8_t matrixBrightness;
  static MatrixPattern currentPattern;
  static uint32_t lastMatrixFrame;
  static uint8_t matrixStep;
  static uint32_t customPixels[16];

  // Incoming serial line buffer
  static char rxBuffer[256];
  static uint16_t rxIndex;

  static void processCommand(const char* cmd);
  static void updateMatrixAnimation();

public:
  static void begin();
  static void update(const SensorState& state);

  // Status accessors for UI
  static bool isStreamActive() { return streamActive; }
  static void toggleStream() { streamActive = !streamActive; }
  static uint32_t getPacketsSent() { return packetsSent; }
  static const char* getLastCommand() { return lastCommand.c_str(); }
  static uint32_t getLastCommandAgeMs() { return millis() - lastCommandTime; }
  static uint32_t getStreamRateHz() { return (streamIntervalMs > 0) ? (1000 / streamIntervalMs) : 0; }
  static void setStreamRateHz(uint8_t hz);

  static MatrixPattern getMatrixPattern() { return currentPattern; }
  static void setMatrixPattern(MatrixPattern pat);
};
