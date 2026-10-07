#include "StorageManager.h"
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "../pins.h"
#include "../HAL.h"
#include "../apps/tools/UplinkBridge.h"

bool StorageManager::sdAvailable = false;
DeviceConfig StorageManager::activeConfig;
LGFX* StorageManager::displayRef = nullptr;

uint16_t StorageManager::computeCRC(const DeviceConfig& config) {
  // Compute CCITT CRC-16 (poly 0x1021, init 0xFFFF) starting after crc16 field (offset 8)
  const uint8_t* data = ((const uint8_t*)&config) + 8;
  size_t length = sizeof(DeviceConfig) - 8;
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void StorageManager::setDefaults(DeviceConfig& config) {
  memset(&config, 0, sizeof(DeviceConfig));
  config.magic = NX_CONFIG_MAGIC;
  config.version = NX_CONFIG_VERSION;
  config.reserved1 = 0;
  config.structSize = sizeof(DeviceConfig);

  // Display Defaults
  config.brightnessPercent = 80;
  config.autoDimEnabled = 0;
  config.tiltMode = 2; // BALANCED
  config.wristCoverSleep = 1;
  config.screenTimeoutSec = 15;
  config.backlightFadeMs = 300;

  // Audio Defaults
  config.silentMode = 1; // Default Muted
  config.buzzerVolumePercent = 80;
  config.buzzerTickDurationMs = 8;
  config.buzzerBaseFreqHz = 2700;

  // Notifications Defaults
  config.notifMode = 1; // NOTIF_ALL
  config.matrixLedAlerts = 1;
  config.hrmReminderIdx = 0; // OFF

  // NPM Defaults
  config.npmPower = 0;
  config.npmBrightnessPercent = 50;
  config.npmPatternIdx = 0; // RADAR

  // Power Defaults
  config.ecoMode = 0;
  config.autoStandby = 1;
  config.sensorSleep = 0;

  // AI Defaults
  config.nxAisCoProc = 0;
  config.adaptiveSensing = 1;

  config.crc16 = computeCRC(config);
}

void StorageManager::applyConfigToHAL(const DeviceConfig& config) {
  // 1. Display
  HAL::brightnessPercent = constrain((int)config.brightnessPercent, 10, 100);
  HAL::autoDimEnabled = (config.autoDimEnabled != 0);
  HAL::tiltMode = (TiltMode)constrain((int)config.tiltMode, 0, 3);
  HAL::wristCoverSleep = (config.wristCoverSleep != 0);
  HAL::screenTimeoutSec = constrain((int)config.screenTimeoutSec, 0, 300);
  HAL::backlightFadeMs = constrain((int)config.backlightFadeMs, 0, 2000);

  if (displayRef) {
    HAL::applyBrightness(displayRef);
  }

  // 2. Audio
  HAL::silentMode = (config.silentMode != 0);
  HAL::buzzerVolumePercent = constrain((int)config.buzzerVolumePercent, 10, 100);
  HAL::buzzerTickDurationMs = constrain((int)config.buzzerTickDurationMs, 2, 30);
  HAL::buzzerBaseFreqHz = constrain((int)config.buzzerBaseFreqHz, 1600, 4400);

  // 3. Notifications
  HAL::setNotificationMode((NotificationMode)constrain((int)config.notifMode, 0, 3));
  HAL::matrixLedAlerts = (config.matrixLedAlerts != 0);
  HAL::hrmReminderIdx = constrain((int)config.hrmReminderIdx, 0, 3);

  // 4. NPM (Matrix)
  HAL::npmBrightnessPercent = constrain((int)config.npmBrightnessPercent, 10, 100);
  HAL::npmPatternIdx = constrain((int)config.npmPatternIdx, 0, 5);
  UplinkBridge::setMatrixPower(config.npmPower != 0);
  uint8_t pwm = (uint8_t)map(HAL::npmBrightnessPercent, 0, 100, 0, 80);
  UplinkBridge::setMatrixBrightness(pwm);
  const MatrixPattern patterns[6] = {
    PATTERN_CYBER_RADAR,
    PATTERN_MATRIX_RAIN,
    PATTERN_SPECTRUM_PLASMA,
    PATTERN_NEON_TRACER,
    PATTERN_GLYPH_BREATH,
    PATTERN_OFF
  };
  UplinkBridge::setMatrixPattern(patterns[HAL::npmPatternIdx]);

  // 5. Power
  HAL::ecoMode = (config.ecoMode != 0);
  HAL::autoStandby = (config.autoStandby != 0);
  HAL::sensorSleep = (config.sensorSleep != 0);

  // 6. AI
  HAL::nxAisCoProc = (config.nxAisCoProc != 0);
  HAL::adaptiveSensing = (config.adaptiveSensing != 0);
}

void StorageManager::extractConfigFromHAL(DeviceConfig& config) {
  config.magic = NX_CONFIG_MAGIC;
  config.version = NX_CONFIG_VERSION;
  config.reserved1 = 0;
  config.structSize = sizeof(DeviceConfig);

  // Display
  config.brightnessPercent = (uint8_t)HAL::brightnessPercent;
  config.autoDimEnabled = HAL::autoDimEnabled ? 1 : 0;
  config.tiltMode = (uint8_t)HAL::tiltMode;
  config.wristCoverSleep = HAL::wristCoverSleep ? 1 : 0;
  config.screenTimeoutSec = (uint16_t)HAL::screenTimeoutSec;
  config.backlightFadeMs = HAL::backlightFadeMs;

  // Audio
  config.silentMode = HAL::silentMode ? 1 : 0;
  config.buzzerVolumePercent = HAL::buzzerVolumePercent;
  config.buzzerTickDurationMs = HAL::buzzerTickDurationMs;
  config.reservedAudio = 0;
  config.buzzerBaseFreqHz = HAL::buzzerBaseFreqHz;

  // Notifications
  config.notifMode = (uint8_t)HAL::notifMode;
  config.matrixLedAlerts = HAL::matrixLedAlerts ? 1 : 0;
  config.hrmReminderIdx = HAL::hrmReminderIdx;
  config.reservedNotif = 0;

  // NPM
  config.npmPower = UplinkBridge::getMatrixPower() ? 1 : 0;
  config.npmBrightnessPercent = HAL::npmBrightnessPercent;
  config.npmPatternIdx = HAL::npmPatternIdx;
  config.reservedNpm = 0;

  // Power
  config.ecoMode = HAL::ecoMode ? 1 : 0;
  config.autoStandby = HAL::autoStandby ? 1 : 0;
  config.sensorSleep = HAL::sensorSleep ? 1 : 0;
  config.reservedPower = 0;

  // AI
  config.nxAisCoProc = HAL::nxAisCoProc ? 1 : 0;
  config.adaptiveSensing = HAL::adaptiveSensing ? 1 : 0;
  config.reservedAi = 0;

  memset(config.futureExpansion, 0, sizeof(config.futureExpansion));
  config.crc16 = computeCRC(config);
}

bool StorageManager::begin(LGFX* display) {
  displayRef = display;

  // Ensure TFT CS is unasserted (HIGH)
  pinMode(CS_TFT, OUTPUT);
  digitalWrite(CS_TFT, HIGH);

  pinMode(CS_SD, OUTPUT);
  digitalWrite(CS_SD, HIGH);

  // Initialize shared SPI bus pins for SD driver
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, CS_SD);

  Serial.println("[STORAGE] Probing ZDSD NAND Flash via SPI (CS=GPIO 47)...");

  // Attempt mounting ZDSD NAND / SD card at 20MHz
  if (!SD.begin(CS_SD, SPI, 20000000, "/sd")) {
    Serial.println("[STORAGE] NOTICE: ZDSD NAND not mounted. Operating in volatile fallback mode.");
    sdAvailable = false;
    setDefaults(activeConfig);
    applyConfigToHAL(activeConfig);
    return false;
  }

  sdAvailable = true;
  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  Serial.printf("[STORAGE] ZDSD NAND Online. Type: %d | Total: %llu MB | Used: %llu KB\n",
                SD.cardType(),
                totalBytes / (1024ULL * 1024ULL),
                usedBytes / 1024ULL);

  // Ensure /sys directory exists
  if (!SD.exists("/sys")) {
    SD.mkdir("/sys");
  }

  // Attempt to load /sys/config.bin
  if (loadConfig(activeConfig)) {
    Serial.println("[STORAGE] Successfully loaded and verified /sys/config.bin.");
  } else {
    Serial.println("[STORAGE] /sys/config.bin absent or invalid. Writing factory defaults...");
    setDefaults(activeConfig);
    saveConfig(activeConfig);
  }

  applyConfigToHAL(activeConfig);
  return true;
}

bool StorageManager::isAvailable() {
  return sdAvailable;
}

DeviceConfig& StorageManager::getActiveConfig() {
  return activeConfig;
}

bool StorageManager::loadConfig(DeviceConfig& config) {
  if (!sdAvailable) return false;

  // Release TFT CS before SD read
  digitalWrite(CS_TFT, HIGH);

  if (!SD.exists("/sys/config.bin")) {
    return false;
  }

  File f = SD.open("/sys/config.bin", FILE_READ);
  if (!f) {
    Serial.println("[STORAGE] Failed to open /sys/config.bin for read.");
    return false;
  }

  size_t readBytes = f.read((uint8_t*)&config, sizeof(DeviceConfig));
  f.close();

  // Reset SD CS
  digitalWrite(CS_SD, HIGH);

  if (readBytes != sizeof(DeviceConfig)) {
    Serial.printf("[STORAGE] Size mismatch: read %u of %u bytes.\n", readBytes, sizeof(DeviceConfig));
    return false;
  }

  if (config.magic != NX_CONFIG_MAGIC) {
    Serial.printf("[STORAGE] Invalid magic header: 0x%04X (expected 0x%04X)\n", config.magic, NX_CONFIG_MAGIC);
    return false;
  }

  if (config.structSize != sizeof(DeviceConfig)) {
    Serial.printf("[STORAGE] Struct size mismatch: %u (expected %u)\n", config.structSize, sizeof(DeviceConfig));
    return false;
  }

  uint16_t expectedCrc = computeCRC(config);
  if (config.crc16 != expectedCrc) {
    Serial.printf("[STORAGE] Checksum failure: CRC 0x%04X != 0x%04X\n", config.crc16, expectedCrc);
    return false;
  }

  return true;
}

bool StorageManager::saveConfig(const DeviceConfig& config) {
  if (!sdAvailable) return false;

  // Release TFT CS before SD write
  digitalWrite(CS_TFT, HIGH);

  DeviceConfig cfgToSave = config;
  cfgToSave.magic = NX_CONFIG_MAGIC;
  cfgToSave.version = NX_CONFIG_VERSION;
  cfgToSave.reserved1 = 0;
  cfgToSave.structSize = sizeof(DeviceConfig);
  cfgToSave.crc16 = computeCRC(cfgToSave);

  // 1. Atomic write to temporary file
  File f = SD.open("/sys/config.tmp", FILE_WRITE);
  if (!f) {
    Serial.println("[STORAGE] Failed to open /sys/config.tmp for writing.");
    digitalWrite(CS_SD, HIGH);
    return false;
  }

  size_t written = f.write((const uint8_t*)&cfgToSave, sizeof(DeviceConfig));
  f.flush();
  f.close();

  if (written != sizeof(DeviceConfig)) {
    Serial.println("[STORAGE] Truncated write to /sys/config.tmp.");
    SD.remove("/sys/config.tmp");
    digitalWrite(CS_SD, HIGH);
    return false;
  }

  // 2. Safe atomic replacement: remove old config and rename .tmp to .bin
  if (SD.exists("/sys/config.bin")) {
    SD.remove("/sys/config.bin");
  }

  bool renamed = SD.rename("/sys/config.tmp", "/sys/config.bin");
  digitalWrite(CS_SD, HIGH);

  if (!renamed) {
    Serial.println("[STORAGE] Failed to rename /sys/config.tmp to /sys/config.bin.");
    return false;
  }

  activeConfig = cfgToSave;
  Serial.println("[STORAGE] Atomic write complete: /sys/config.bin updated.");
  return true;
}
