# ISD-Core Software Roadmap & Feature Specification

Roadmap and application concepts for the ISD-Core firmware (ESP32-S3).  
This document serves as the persistent specification across all development environments (Work & Home).

Each primary application maps 1:1 to a card in the **3D Cover Flow App Deck** (`AppMenu.h`):

| App ID | Menu Tag | Directory | Status |
| :--- | :--- | :--- | :--- |
| `APP_VITALS` | `VIT` | `src/apps/vitals/` | Planned (Sensor Ready) |
| `APP_ENV` | `ENV` | `src/apps/enviroment/` | Planned (Sensor Ready) |
| `APP_CLOCK` | `CLK` | `src/apps/clock/` | Planned (RTC Ready) |
| `APP_DEVICE` | `DEV` | `src/apps/device/` | Planned (Telemetry Ready) |
| `APP_TOOLS` | `TLS` | `src/apps/tools/` | **Partially Implemented (2/6)** |
| `APP_SETTINGS`| `SET` | `src/apps/settings/` | Planned (PWM/Audio Ready) |

---

## 1. Vitals (`VIT`)
> Target Hardware: **MAX30105** Optical Sensor (I2C `0x57`), Core 0 PPG sampling.

- [ ] **Heart Rate Monitor (HRM):** Real-time BPM calculation with signal confidence scoring and PPG waveform view.
- [ ] **SpO2 Blood Oxygen:** Red & Infrared ratio calculation ($R = \frac{AC_{red}/DC_{red}}{AC_{ir}/DC_{ir}}$).
- [ ] **MSF Calculated Stress:** Heart Rate Variability (HRV) / RMSSD calculation mapped to a stress index (0–100).
- [ ] **Measurement Reminders:** Configurable background intervals (e.g., every 30m, 1h, 2h) with notification dispatch.

---

## 2. Environment (`ENV`)
> Target Hardware: **BME680** (I2C `0x76`), **OPT3001** (I2C `0x45`), **BNO085** (I2C `0x4A`).

- [ ] **Atmospheric Pressure:** Live hPa pressure trends, barometric altimeter, and weather tendency.
- [ ] **Ambient Temperature:** Accurate ambient temperature readout (°C/°F) with self-heating offset compensation.
- [ ] **Relative Humidity:** % RH environmental humidity and dew point calculation.
- [ ] **Tactical 3D Compass:** Dynamic cardinal heading with tilt compensation via BNO085 rotation vectors.
- [ ] **Ambient Light Intensity:** Precision lux measurement with automatic display brightness integration.
- [ ] **MSF Calculated Weather Forecast:** Local short-term barometric pressure trend analysis (Zambretti forecaster algorithm).

---

## 3. Clock (`CLK`)
> Target Hardware: **RV-3028-C7** Ultra-Low-Power RTC (I2C `0x52`), Hardware Timers.

- [ ] **Countdown Timer:** Quick-dial timer with progress bar, completion chime, and matrix alert.
- [ ] **Stopwatch:** Millisecond-accurate split lap timing with pause/resume and lap list.
- [ ] **World Clock:** Dual/multi-time zone display with configurable UTC offsets.
- [ ] **Alarms:** Recurring and one-shot alarms with customizable audio melodies and NeoPixel wakeup patterns.
- [ ] **Date, Time & Timezone Settings:** Manual RTC calibration and automatic NTP sync when connected to Wi-Fi.

---

## 4. Device (`DEV`)
> Target Hardware: **MAX17048** Fuel Gauge (I2C `0x36`), **BQ25170** Charger, ESP32-S3 Flash/PSRAM.

- [ ] **Advanced Battery Telemetry:** Cell voltage, state of charge (%), charge/discharge rate (%/h), estimated runtime, and battery health tracking.
- [ ] **NX-SDS (Self-Diagnostic Suite):** Comprehensive hardware self-test for I2C buses, SPI bus, sensors, flash, and PSRAM integrity.
- [ ] **Storage Intelligence:** NAND flash / SD partition inspection, filesystem usage, and log file viewer.
- [ ] **Firmware & OTA Updates:** Version status, changelog display, and over-the-air firmware updates via [NX-Uplink](https://nx-uplink.base44.app).

---

## 5. Tools (`TLS`)
> Sub-app container: `src/apps/tools/AppTools.h`

- [x] **2D Digital Spirit Level:** Precision bullseye bubble reticle, calibrated pitch/roll display, and zero-horizon tare lock with audio feedback.
- [x] **NPM (NeoPixel Matrix) & NX-Uplink Bridge:** USB-CDC 115200 telemetry stream to [nx-uplink.base44.app](https://nx-uplink.base44.app) + 4×4 WS2812B matrix animation engine (`CYBER_RADAR`, `MATRIX_RAIN`, `SPECTRUM_PLASMA`, `NEON_TRACER`, `GLYPH_BREATH`, `QUANTUM_RIPPLE`).
- [ ] **ESP-Deauth:** 802.11 management packet detection and defensive analysis tool.
- [ ] **Wi-Fi Scanner & Analysis:** 2.4 GHz channel survey, SSID signal RSSI graph, and network security inspection.
- [ ] **BLE Scanner & Analysis:** Bluetooth Low Energy beacon detector, RSSI tracker, and device discovery list.
- [ ] **Oracle Network Integration:** Decentralized sensor data uplink and cryptographic node verification ([oracle-network.tech](https://oracle-network.tech)).

---

## 6. Settings (`SET`)
> Target Subsystems: Core 1 Display engine, Core 0 Power manager, PWM buzzer, NPM matrix.

- [ ] **Notification Manager:** Granular routing configuration (Buzzer, NeoPixel Matrix, Screen Popup) per app event.
- [ ] **Display Settings:** Automatic ambient dimming (via OPT3001), manual brightness slider, display sleep timeout, and tilt-to-wake gesture sensitivity.
- [ ] **Power Save Mode (Eco):** Manual toggle or automatic low-battery threshold; configurable throttling of sensor polling rates, screen refresh, and LED power rails.
- [ ] **Audio / Buzzer Config:** Volume intensity, frequency curves, mute toggle, and UI navigation haptic chirps.
- [ ] **NX-AIS Toggle:** Master switch for onboard Autonomous Intelligence / sensor edge analytics.
