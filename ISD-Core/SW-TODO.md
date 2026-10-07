# ISD-Core Software Roadmap & Feature Specification

This document tracks implementation status across the 6 core application decks, system services, and hardware subsystems.

---

## Application Decks & Core Concepts

### 1. Vitals (Internal Physiology)
* [x] **HRM:** Spot and live optical PPG sampling (`MAX30102`)
* [x] **SpO2:** Blood oxygen calculation and perfusion index
* [x] **MSF Calculated Stress:** Autonomic stress score via RMSSD HRV analysis
* [x] **HRM Measuring Reminders:** Scheduled cyclic measurement prompts (`OFF`, `30m`, `1h`, `2h`) configured in Settings

### 2. Environment (External Ambience)
* [x] **Pressure:** Live barometric readout (`BME690` via I2C `0x76`) & 16-bar isobar history
* [x] **Temperature:** Ambient temperature sensing with capillary mercury tube & min/max bounds
* [x] **Humidity:** Relative humidity sensing with teardrop hygrometer
* [x] **Light Intensity:** Photopic ambient illuminance (`OPT3001` via I2C `0x44`/`0x45`) with 6-blade optical aperture
* [x] **MSF Calculated Weather Info:** 3-hour $\Delta P/\Delta t$ barometric storm prediction & Magnus-Tetens dew point calculation
* [x] **3D Vector Earth Engine:** 60 FPS real-time rotating planetary wireframe with 23.4° tilt and continents
* [x] **5-Card Vertical Hierarchy:** Master HUD + 4 dedicated deep-dive sensor analysis cards
* [x] **Altimeter:** Barometric elevation calculation with relative zero and QNH calibration

### 3. Clock (Temporal Operations)
* [ ] **Timer:** Countdown timer with progress bar and acoustic/NPM alert
* [ ] **Stopwatch:** Millisecond chronograph with lap logging
* [ ] **World Clock:** Dual UTC / time-zone offset tracking
* [ ] **Alarms:** Battery-backed hardware alarms via `RV-3028` interrupt line
* [x] **Date, Time & Timezone Sync:** High-accuracy RTC timekeeping on Watchface HUD (`RV-3028` via I2C `0x52`)
* [x] **NAND State Persistence:** Storage subsystem ready to persist clock/alarm states

### 4. Device (Silicon & Hardware Integrity)
* [x] **Battery Info:** Real-time cell voltage, capacity percentage, charge/discharge rate via `MAX17048` (`0x36`)
* [ ] **Deep Battery Analytics:** Charge cycle tracking, runtime estimation, and cell internal resistance ($R_{\text{int}}$)
* [ ] **NX-SDS (Self-Diagnostic System):** Continuous I2C 9-clock recovery, hardware proof-testing, and audit logging (`/sds/audit_log.txt`)
* [x] **Storage Subsystem:**
  * [x] Onboard ZDSD NAND Flash driver (`CS_SD = GPIO 47`) over high-speed SPI
  * [x] 64-byte packed binary struct (`/sys/config.bin`) with CRC-16 CCITT validation
  * [x] Zero-wear save policy (RAM-only dialing, write on `[PUSH]` confirmation or menu exit)
  * [x] Atomic staging writes (`/sys/config.tmp` $\to$ `/sys/config.bin`)
  * [x] Standalone PC inspection & JSON conversion CLI tool (`tools/nx_config_tool.py`)
* [ ] **Firmware Version & OTA Updates:** Wireless OTA sync via [NX-Uplink](https://nx-uplink.base44.app)

### 5. Tools (Tactical & Field Operations)
* [x] **Spirit Level:** 2D interactive bubble level reticle driven by `BNO085` accelerometer on Watchface HUD
* [ ] **Dedicated Spirit Level App:** Fullscreen precision leveling with numeric pitch/roll angles
* [ ] **WiFi Analysis:** 2.4 GHz channel scan and RSSI meter
* [ ] **BLE Analysis:** BLE advertisement detection and proximity RSSI tracking
* [ ] **ESP-Deauth:** Wireless penetration testing / network resilience tool
* [x] **NX-Uplink Bridge:** Companion serial/wireless packet protocol framework
* [ ] **Oracle Network Integration:** Web3 / decentralized oracle bridge ([oracle-network.tech](https://oracle-network.tech))

### 6. Settings (System Orchestration) — *Fully Implemented*
* [x] **6-Category Architecture:** Strict 4-box viewport with custom vertical scrollbar track and thumb
* [x] **Display Settings:**
  * [x] Manual Dimming: 360° circular radial dial with orbital satellite pip (10% to 100% in 5% steps)
  * [x] Automatic Dimming: Real-time photopic light scaling via `OPT3001`
  * [x] Tilt to Wake: Dedicated 4-card overview menu (`OFF`, `SENSITIVE`, `BALANCED`, `SLUGGISH`)
  * [x] Display Timeout: 360° circular radial dial (5s to 300s, or `NEVER` / Always On)
  * [x] Wrist Cover Standby: Optical occlusion sleep gesture toggle
  * [x] Backlight Fade Animation: 360° circular radial dial (0ms to 2000ms in 50ms steps) with smoothstep ($t^2 \times (3 - 2t)$) curve for waking and sleeping
* [x] **Notification Settings:**
  * [x] Delivery Method: Dedicated 4-card overview menu (`SILENT`, `ALL`, `SOUND ONLY`, `LIGHTS ONLY`)
  * [x] Matrix LED Flashing: Toggle 4×4 NeoPixel optical alerts
  * [x] HRM Reminder Interval: Cycle reminder frequency (`OFF`, `30m`, `1h`, `2h`)
* [x] **Power Save Mode:**
  * [x] Eco Mode: CPU throttling and reduced sensor polling
  * [x] Auto Standby: Automatic screen sleep on inactivity timeout
  * [x] Sensor Sleep: Power down IMU/environmental rails during standby
* [x] **Buzzer Settings (Acoustic Orchestration):**
  * [x] Master Audio Mute/Unmute toggle
  * [x] Volume: 360° circular radial dial with soundwave arcs (10% to 100% duty cycle)
  * [x] Tick Duration: 360° circular radial dial with square-wave pulse width graphic (2ms to 30ms)
  * [x] Tone Pitch: 360° circular radial dial with animated sine wave graphic (1600 Hz to 4400 Hz in 100 Hz steps)
* [x] **NPM Settings (4×4 NeoPixel Matrix):**
  * [x] High-Side PMOS Power Gate Toggle (`GPIO 17`, 0µA standby cutoff)
  * [x] Brightness: 360° circular radial dial with matrix icon (10% to 100%)
  * [x] Real-Time Animation Selector (`RADAR`, `RAIN`, `PLASMA`, `TRACER`, `BREATH`, `OFF`)
  * [x] Test Pulse: Immediate high-vis amber verification strobe
* [x] **NX-AIS Settings:**
  * [x] Background Co-Processor Supervisor toggle
  * [x] Adaptive Environmental Context toggle
  * [x] Sensor Diagnostics check

