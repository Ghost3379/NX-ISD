# ISD-Core // Operating System & User Manual

Welcome to **ISD-Core**, the custom dual-core wearable operating system designed for the **NX-ISD** (Intelligent Sensor Device) powered by the ESP32-S3.

ISD-Core is built from the ground up on a central design philosophy: **it is an intuitive, tactile smartwatch operating system—not a wearable Excel spreadsheet of raw sensor dumps.** Mathematical sensor fusion (**NX-MSF**), autonomous contextual advisory (**NX-AIS**), and hardware self-diagnostics (**NX-SDS**) operate silently under the hood, distilling complex telemetry into high-value insights, glanceable widgets, and physical feedback.

The user interface draws inspiration from retro-futuristic amber vector displays (TVA, Pip-Boy, and aerospace instrumentation), rendered at ~42 FPS on a 240×240 IPS display using zero-copy Octal PSRAM double-buffering.

---

## Table of Contents
1. [Physical Hardware Controls & Gestures](#1-physical-hardware-controls--gestures)
2. [Operating System Architecture & Navigation](#2-operating-system-architecture--navigation)
3. [Screen Layouts & Visual Appearance](#3-screen-layouts--visual-appearance)
   - [Level 1A: Watchface HUD (Home)](#level-1a-watchface-hud-home)
   - [Level 1B: Quickpanel Action Grid](#level-1b-quickpanel-action-grid)
   - [Level 1C: Notifications Stack](#level-1c-notifications-stack)
   - [Level 2: 3D Cover Flow App Launcher](#level-2-3d-cover-flow-app-launcher)
4. [The 6 Core Application Decks](#4-the-6-core-application-decks)
   - [App 1: VITALS (Internal Physiology)](#app-1-vitals-internal-physiology)
   - [App 2: ENVIRONMENT (External Ambience)](#app-2-environment-external-ambience)
   - [App 3: CLOCK (Temporal Operations)](#app-3-clock-temporal-operations)
   - [App 4: DEVICE (Silicon & Hardware Integrity)](#app-4-device-silicon--hardware-integrity)
   - [App 5: TOOLS (Tactical & Wireless Operations)](#app-5-tools-tactical--wireless-operations)
   - [App 6: SETTINGS (System Orchestration)](#app-6-settings-system-orchestration)
5. [Navigation Quick-Reference](#5-navigation-quick-reference)
6. [Firmware Architecture & Hardware Layer](#6-firmware-architecture--hardware-layer)
7. [Building & Flashing](#7-building--flashing)

---

## 1. Physical Hardware Controls & Gestures

The NX-ISD hardware eliminates touchscreens in favor of high-reliability physical controls that can be operated blind, with gloves, or in adverse field conditions. It is controlled via a **3-way navigation lever** and a **dedicated system pushbutton**:

```text
                     ┌───────────────────────────┐
                     │      240x240 Display      │
                     │                           │
                     │          [ HUD ]          │
                     │                           │
                     └───────────────────────────┘
                                         [ BTN ] (GPIO 13) ── Back / Exit / Wake
                     ┌───────────────────────────┐
      [ LEVER LEFT ] │      [ LEVER PUSH ]       │ [ LEVER RIGHT ]
        (GPIO 16)    │        (GPIO 15)          │   (GPIO 14)
       Prev / Dec    │    Select / 1.2s Charge   │   Next / Inc
```

### Control Bindings & Timing Parameters

| Control | GPIO Pin | Gesture | Timing | Action |
| :--- | :---: | :--- | :---: | :--- |
| **LEVER LEFT** | `16` | **Flick / Click** | $< 475\,\text{ms}$ | Previous card, scroll left, or decrement value. |
| | | **Hold** | $\ge 475\,\text{ms}$ | **Hold-to-repeat:** Cycles every $160\,\text{ms}$ for fast numeric dialing. |
| **LEVER RIGHT** | `14` | **Flick / Click** | $< 475\,\text{ms}$ | Next card, scroll right, or increment value. |
| | | **Hold** | $\ge 475\,\text{ms}$ | **Hold-to-repeat:** Cycles every $160\,\text{ms}$ for fast numeric dialing. |
| **LEVER PUSH** | `15` | **Click** | $< 1.2\,\text{s}$ | Select, toggle setting, confirm hero card, or enter Quickpanel. |
| | | **Tactical Charge** | $\ge 1.2\,\text{s}$ | **Hold Charge Gesture:** Sweeps energy arc to launch the 3D App Menu. |
| **BTN** | `13` | **Click** | Any | **Global Back / Cancel:** Pops back up one level; wakes display from Standby. |

* **Hardware Interrupts & Latching:** Lever inputs trigger dedicated IRAM ISRs on `FALLING` edges with a $40\,\text{ms}$ hardware debounce window, ensuring zero input lag ($<1\,\mu\text{s}$ latching) while rejecting switch release chatter.

---

## 2. Operating System Architecture & Navigation

ISD-Core implements a strict **3-Tier Hierarchical Navigation Structure**. You are never trapped in nested submenus and are never more than two clicks away from the home watchface:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ LEVEL 1: PRIMARY OS SHELL                                                             │
│                                                                                        │
│   [ Notifications ]  <───── LEVER L ────  [ Watchface HUD ]  ──── LEVER R ─────>  [ Quickpanel ]
│                                                  │                                     │
│                                                  │ 1.2s Charge Hold                    │ Click PUSH
│                                                  │ (LEVER_PUSH)                        │ (on Tile)
│                                                  ▼                                     │
├──────────────────────────────────────────────────┼─────────────────────────────────────┤
│ LEVEL 2: 3D APP LAUNCHER                         │                                     │
│                                                  │                                     │
│   ┌──────────────────────────────────────────────┴─────────────────────────────────┐   │
│   │                          3D Cover Flow Deck                                    │   │
│   │   [VITALS] ── [ENVIRONMENT] ── [CLOCK] ── [DEVICE] ── [TOOLS] ── [SETTINGS]    │   │
│   └──────────────────────────────────────────────┬─────────────────────────────────┘   │
│                                                  │                                     │
│                                                  │ Click PUSH                          │
│                                                  │ (on active card)                    │
│                                                  ▼                                     ▼
├────────────────────────────────────────────────────────────────────────────────────────┤
│ LEVEL 3: DEDICATED APPS & MENUS                                                        │
│                                                                                        │
│   • Vitals Monitor (HRM/SpO2)   • Device & NX-SDS Health       • Brightness Dial       │
│   • Environment & Weather       • Tactical Tools & Uplink      • Power & Reboot Menu   │
│   • Chrono & Timers             • System & NX-AIS Settings     • Quick Actions         │
│                                                                                        │
│   (Pressing BTN at any time immediately pops up to Level 2 Launcher or Level 1 HUD)    │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Screen Layouts & Visual Appearance

The interface is styled in a **pure black and amber-orange vector theme**:
* **Background:** Absolute Black (`#000000`, RGB `0, 0, 0`) for infinite contrast.
* **Primary Foreground:** Amber-Orange High-Vis (`#FF7300`, RGB `255, 115, 0`).
* **Secondary Foreground:** Amber Mid-Tone (`#B85000`) for structural borders and inactive meters.
* **Muted / Inactive:** Haired Amber Dim (`#4A2000`) for gridlines and background card geometry.

---

### Level 1A: Watchface HUD (Home)

The primary home screen displays complete situational awareness at a single glance:

```text
+-----------------------------------+  (0, 0)
| NX-ISD                 [===] 98%  |  Header: System status & Battery meter
| --------------------------------- |  Hairline divider
|             14:32:08              |  Large crisp digital time (RV-3028 RTC)
|                                   |
|   [ 24.3 C ]         [ 48 %RH ]   |  BME680 Temperature & Humidity badges
|                                   |
|               ( N )               |  Rotating Compass Reticle with
|             /   |   \             |  Cardinal Points (N, E, S, W)
|            W --(o)-- E            |  Central 2D Spirit Bubble Level
|             \   |   /             |  Outer Battery Arc Gauge
|               ( S )               |
|                                   |
|          HDG: 042  P: +02 R: -01  |  Numeric heading, pitch, and roll
+-----------------------------------+  (239, 239)
```

* **Header Bar:** Shows active mode identifier and live battery percentage with charging badge (`[CHG]`).
* **Digital Clock:** Synchronized with the ultra-low-power RV-3028 RTC ($0.05\,\mu\text{A}$ timekeeping).
* **Telemetry Badges:** Real-time environmental readings filtered to eliminate sensor jitter.
* **Compass & Spirit Bubble:** Dynamic 360° rotating azimuth reticle with shortest-path circular interpolation (`angleDiff`), framing a real-time 2D bubble level driven by the BNO085 accelerometer.
* **Tactical Charge Ring:** When `LEVER_PUSH` is held, an amber radial energy meter sweeps clockwise around the compass ring. Completing the 1.2s charge triggers an audible chime and launches the 3D Menu.

---

### Level 1B: Quickpanel Action Grid

Accessed by flicking **`LEVER_RIGHT`** from the Watchface HUD. A 2×3 grid of tactical rounded action tiles (inspired by modern smartwatch quick toggles):

```text
+-----------------------------------+
| QUICK ACCESS             [ X ]    |  Header with close hint (BTN)
| --------------------------------- |
|  +-------------+  +-------------+ |
|  | [SETTINGS]  |  |   [ HRM ]   | |  Settings shortcut / Spot heart-rate scan
|  +-------------+  +-------------+ |
|  +-------------+  +-------------+ |
|  | [ECO MODE]  |  | [BRIGHTNESS]| |  Power toggle / Interactive brightness dial
|  +-------------+  +-------------+ |
|  +-------------+  +-------------+ |
|  |  [SILENT]   |  | [SHUTDOWN]  | |  Audio & NeoPixel mute / Standby & Power
|  +-------------+  +-------------+ |
+-----------------------------------+
```

* **Navigation:** Lever Left/Right cycles active tile focus (highlighted by an inverted amber fill). Clicking `LEVER_PUSH` triggers the action.
* **Brightness Dial:** Clicking `[BRIGHTNESS]` opens a circular dial with live screen dimming from 10% to 100% in 5% increments.
* **Shutdown / Standby:** Clicking `[SHUTDOWN]` opens a power sheet offering **Standby** (instant display shutoff, $<15\,\text{mA}$, wake on any key) or **Reboot**.

---

### Level 1C: Notifications Stack

Accessed by flicking **`LEVER_LEFT`** from the Watchface HUD:

```text
+-----------------------------------+
| NOTIFICATIONS (2)        [CLR]    |  Header: Unread count & Clear-all
| --------------------------------- |
| +-------------------------------+ |
| | [!] NX-AIS ADVISORY     14:15 | |  Card 1: High priority alert
| | Rapid pressure drop detected. | |
| | Storm risk: DeltaP > 2.5 hPa  | |
| +-------------------------------+ |
| +-------------------------------+ |
| | [*] VITALS REMINDER     13:30 | |  Card 2: Routine reminder
| | Scheduled hourly HRM check.   | |
| +-------------------------------+ |
+-----------------------------------+
```

* Displays chronological notifications, NX-AIS intelligence advisories, and timer alerts.
* Pressing `LEVER_PUSH` expands the selected notification; pressing `BTN` dismisses it.

---

### Level 2: 3D Cover Flow App Launcher

Triggered by the **1.2-second Tactical Charge Gesture** on the Watchface HUD:

```text
+-----------------------------------+
| \ ============================= / |  Top Slanted Cradle Rail
|                                   |
|   +----+      +-------------+     |
|  /     /     /| ENVIRONMENT |     |  Flanking Cards: 3D Perspective Scaling
| / VIT /     | |   HERO CARD |     |  Hero Card: Full 1:1 Scale & Bright Focus
| \     \     | | Temp Lux Gas|     |  Trapezoidal Depth Projection
|  \     \    | | Baro Storm  |     |
|   +----+    | +-------------+     |
|              \|             |     |
|                                   |
| / ============================= \ |  Bottom Slanted Cradle Rail
| [<<] LEVER: SELECT   PUSH: ENTER  |  Tactile bottom navigation prompt
+-----------------------------------+
```

* **True 3D Perspective Projection:** Cards smoothly scale and distort trapezoidally as they approach the center of the display.
* **Spring-Damper Physics:** Scrolling is fluid and organic (`diff * 0.48f`), snapping securely into the nearest hero card on lever release.
* **Mechanical Hardware Cradle:** Slanted top and bottom structural rails frame the carousel like an aerospace bay.
* **Launching:** Clicking **`LEVER_PUSH`** opens the centered app. Pressing **`BTN`** springs back to the Watchface HUD.

---

## 4. The 6 Core Application Decks

ISD-Core groups all functionality into **6 dedicated, human-centered decks**. Raw math (**NX-MSF**) and contextual algorithms (**NX-AIS**) empower these decks behind the scenes:

```text
┌─────────────────────────┬─────────────────────────┬─────────────────────────┐
│       PHYSIOLOGY        │        TEMPORAL         │         SILICON         │
│       [ VITALS ]        │        [ CLOCK ]        │       [ DEVICE ]        │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│       ENVIRONMENT       │         ACTION          │         CONTROL         │
│     [ ENVIRONMENT ]     │        [ TOOLS ]        │      [ SETTINGS ]       │
└─────────────────────────┴─────────────────────────┴─────────────────────────┘
```

---

### App 1: VITALS (Internal Physiology)
*Focus: Personal health telemetry, recovery metrics, and autonomic stress.*

```text
+-----------------------------------+
| VITALS // MAX30102         [RUN]  |
| --------------------------------- |
|   HEART RATE         SpO2         |
|   [ 72 ] BPM        [ 98 ] %      |  Live / Last measured readings
|   Last: 14m ago     Last: 14m ago |  Elapsed timestamp
| --------------------------------- |
|   /\    /\    /\    /\            |  Real-time animated ECG pulse trace
| --/  \--/  \--/  \--/  \--------- |  during active optical sampling
| --------------------------------- |
|   STRESS INDEX: [ 28 / BALANCED ] |  NX-MSF Autonomic Stress Score (RMSSD)
|   [=====>-------------]           |  Visual stress bar gauge
|   REMAIN STATIONARY FOR MEASURE   |
+-----------------------------------+
```

* **Last Measured Spot-Check:** Immediately presents your last recorded heart rate and blood oxygen saturation ($SpO_2$) along with the exact time elapsed since measurement.
* **Live Optical Measurement:** Initiates live high-speed PPG sampling on the MAX30102 with a scrolling real-time ECG waveform and finger-contact detection.
* **Autonomic Stress Score (NX-MSF):** Analyzes successive $R\text{-}R$ intervals and calculates Root Mean Square of Successive Differences (RMSSD) to output a 0–100 Stress Index (*Relaxed*, *Balanced*, *Elevated*, *High Tension*).
* **Cyclic Measurement Reminders:** Set automatic background prompts (every 30m, 1h, 2h). Alerts route to the 4×4 NeoPixel matrix (gentle pulsing glyph), the buzzer, or both.

---

### App 2: ENVIRONMENT (External Ambience)
*Focus: Ambient comfort, meteorological forecasting, and alpine trail safety.*

```text
+-----------------------------------+
| ENVIRONMENT // BME680      [LOG]  |
| --------------------------------- |
|  TEMP: 24.3 C       HUMID: 48 %RH |  Temperature and Relative Humidity
|  LUX:  850 lx       GAS:  185 kOhm|  OPT3001 Photopic Lux & MOX Gas
|  BARO: 1013.2 hPa   TREND: -0.4hPa|  Barometric pressure & 3h tendency
| --------------------------------- |
|  STORM PREDICTOR (NX-MSF):        |
|  [ STABLE / NO RAPID DROP ]       |  Barometric storm warning
|  DEW POINT: 12.8 C (MARGIN: 11.5C)|  Magnus-Tetens condensation fog risk
|  AIR QUALITY: GOOD (VOC LOW)      |  Gas resistance baseline comparison
+-----------------------------------+
```

* **Consolidated Atmospheric Dashboard:** Simultaneous real-time monitoring of Temperature (°C/°F), Relative Humidity (%), Barometric Pressure (hPa), and photopic ambient light level (Lux via OPT3001).
* **Air Quality & Gas Resistance:** Tracks BME680 metal-oxide sensor resistance ($R_{\text{gas}}$) against baseline to monitor volatile organic compounds (VOCs) and room ventilation.
* **Storm Predictor (NX-MSF):** Evaluates rolling 3-hour pressure differentials ($\Delta P / \Delta t$). Triggers high-priority storm alerts if pressure drops $> 2.5\,\text{hPa}/3\text{h}$.
* **Dew Point & Mountain Fog Intel:** Computes dew point ($T_{\text{dew}}$) using the Magnus-Tetens equation. Warns when $(T - T_{\text{dew}}) \le 1.0^\circ\text{C}$ to alert hikers of incoming trail fog or condensation.
* **Thermal Strain & Perceived Comfort:** Merges temperature and humidity into Humidex ratings to assess heat exhaustion risk.

---

### App 3: CLOCK (Temporal Operations)
*Focus: Precision timekeeping, interval workouts, alarms, and state survival.*

```text
+-----------------------------------+
| CLOCK // CHRONOGRAPH       [MODE] |
| --------------------------------- |
|          00 : 04 : 18 . 42        |  Large millisecond stopwatch display
|                                   |
|   LAP 1: 00:01:12.10              |
|   LAP 2: 00:03:06.32              |  Split-lap logging table
| --------------------------------- |
|   [>] START    [R] RESET   [L] LAP|  Tactile lever control hints
| --------------------------------- |
|   TIMER: 05:00 (IDLE)             |  Quick-switch to countdown timer
|   ALARM 1: 07:00 [ON] (DAILY)     |  Battery-backed hardware alarm
+-----------------------------------+
```

* **Chrono / Stopwatch:** High-precision millisecond stopwatch with split-lap logging and recorded lap history.
* **Countdown Timer:** Quick-dial timer with progress bar, audible warble, and flashing amber NeoPixel matrix alert.
* **Hardware Alarms:** Multi-slot daily and one-shot alarms synced to the RV-3028 RTC hardware interrupt line.
* **World Clock:** Auxiliary display for dual UTC / secondary time-zone offsets.
* **Non-Volatile NAND State Persistence:** Commits active countdown timers, alarms, and settings directly to onboard NAND Flash. If the battery is completely drained, all timers and states restore automatically on the next boot!

---

### App 4: DEVICE (Silicon & Hardware Integrity)
*Focus: Silicon self-diagnostics, energy accounting, and system health.*

```text
+-----------------------------------+
| DEVICE // NX-SDS DIAG     [PASS]  |
| --------------------------------- |
|  I2C BUS: ACK OK (400 kHz Fast)   |  Bus audit & automated 9-clock recovery
|  BNO085: OK    BME680: OK         |  Sensor proof-testing results
|  OPT3001: OK   RV3028: OK [OSF:0] |  Oscillator Stop Flag (OSF) audit
| --------------------------------- |
|  BATTERY: 4.12V  98%  (+0.2 %/h)  |  MAX17048 Fuel Gauge telemetry
|  CELL HEALTH: 99%  R_INT: 85 mOhm |  Internal resistance health metric
| --------------------------------- |
|  PSRAM: [====>--------] 2.1/8.0 MB|  Octal PSRAM heap watermark
|  HEAP:  182 kB FREE   FLASH: 16 MB|  FreeRTOS core memory
+-----------------------------------+
```

* **NX-SDS Self-Diagnostic Suite:**
  * **I2C Bus Audit:** Probes all bus addresses; automatically executes 9-clock SCL pulse trains to recover hung slave lines.
  * **Sensor Proof-Testing:** On-demand self-tests for BNO085 internal co-processor, BME680 hotplate, and OPT3001 conversion registers.
  * **Oscillator Watchdog:** Inspects the RV-3028 `OSF` (Oscillator Stop Flag) to detect brownouts, crystal failure, or invalid RTC timing.
* **MAX17048 Fuel Gauge Telemetry:** Live cell terminal voltage ($V_{\text{cell}}$), charge/discharge rate (%/hr), estimated internal cell resistance ($R_{\text{int}}$), and health cycle counters.
* **Memory & Storage Gauges:** Visual bar meters showing Octal PSRAM usage (8 MB pool), FreeRTOS heap watermarks, and NAND Flash storage wear.

---

### App 5: TOOLS (Tactical & Wireless Operations)
*Focus: Field instrumentation, wireless scanning, and companion bridge.*

```text
+-----------------------------------+
| TOOLS // SPIRIT LEVEL      [2D]   |
| --------------------------------- |
|              +-------+            |  2D Bubble Level target reticle
|              |   o   |            |  Dynamic bubble rendered via BNO085
|              +-------+            |
|       PITCH: +01.2  ROLL: -00.4   |  Precision numeric degree readouts
| --------------------------------- |
|  [WIFI/BLE SNIFFER]               |  2.4 GHz signal strength scanner
|  NET_HOME_5G  [-42 dBm] ========= |
|  BLE_TAG_04   [-68 dBm] =====     |
| --------------------------------- |
|  NX-UPLINK: PAIRED (base44.app)   |  Companion web dashboard bridge
+-----------------------------------+
```

* **Tactical 2D Spirit Level:** High-precision surface leveling tool utilizing the BNO085 accelerometer, featuring a responsive central bubble and digital pitch/roll readouts.
* **Wireless Scanner & RSSI Meter:** Scans local 2.4 GHz WiFi channels and BLE advertisements, rendering dynamic signal strength RSSI gradient meters.
* **NX-Uplink Companion Bridge:** Manages wireless pairing and synchronization with the companion web dashboard ([www.nx-uplink.base44.app](https://www.nx-uplink.base44.app)) for telemetry export and firmware updates.
* **Hardware Pin Monitor:** Inspects charger state lines (`/PG` USB power, `/STAT` active charging) and system GPIO rails in real time.

---

### App 6: SETTINGS (System Orchestration)
*Focus: OS customization, power management, and intelligence intensity.*

```text
+-----------------------------------+
| SETTINGS // SYSTEM         [SAVE] |
| --------------------------------- |
| > TIME & DATE SETUP               |  RTC manual set and 12h/24h toggle
|   DISPLAY: AUTO-LUX (OPT3001)     |  Auto vs. manual brightness dial
|   WRIST-WAKE GESTURE: [ ENABLED ] |  BNO085 wrist-rotation wake trigger
|   AUTO-STANDBY: [ 60 SECONDS ]    |  Power-saving screen timeout
|   NX-AIS ADVISOR: [ FULL INTEL ]  |  Advisor intensity (Full / Subtle / Mute)
|   PIEZO AUDIO TICKS: [ ENABLED ]  |  Navigation acoustic feedback
|   NEOPIXEL MATRIX: [ DIM (15%) ]  |  4x4 matrix ambient brightness
+-----------------------------------+
```

* **Display & Gestures:** Toggle automatic backlight scaling (continuously adjusted by the OPT3001 light sensor) or manual brightness; enable/disable the IMU-driven **Wrist-Wake Gesture**.
* **Power Management:** Configure Eco Mode sensor poll rates and auto-standby timeouts (30s, 60s, 2m, Never).
* **NX-AIS Advisory Intensity:** Set intelligence supervisor behavior:
  * **Full:** On-screen popups, audio chimes, and 4×4 NeoPixel glyphs.
  * **Subtle:** 4×4 NeoPixel matrix glyphs only (silent).
  * **Muted:** Completely silent background logging only.
* **Notification Routing Matrix:** Granular control over which subsystems may trigger audio chimes, screen interrupts, or NeoPixel alerts.

---

## 5. Navigation Quick-Reference

| Goal | Action | Screen Context |
| :--- | :--- | :--- |
| **Open Quickpanel** | Flick **`LEVER_RIGHT`** | From Watchface HUD |
| **Open Notifications** | Flick **`LEVER_LEFT`** | From Watchface HUD |
| **Open 3D App Menu** | Press & hold **`LEVER_PUSH` (1.2s)** | From Watchface HUD |
| **Browse Apps** | Flick or hold **`LEVER_LEFT`** / **`LEVER_RIGHT`** | Inside 3D App Menu |
| **Enter App** | Click **`LEVER_PUSH`** | On highlighted Hero Card |
| **Adjust Value / Scroll** | Flick or hold **`LEVER_LEFT`** / **`LEVER_RIGHT`** | Inside any App or Setting |
| **Go Back / Exit** | Press **`BTN`** (GPIO 13) | Anywhere in the OS |
| **Instant Screen Standby** | Open Quickpanel $\to$ `[SHUTDOWN]` $\to$ Standby | Level 1 Shell |
| **Wake Screen** | Press **`BTN`**, flick any lever, or plug USB | Standby Mode |

---

## 6. Firmware Architecture & Hardware Layer

ISD-Core utilizes the ESP32-S3 dual-core asymmetric processing model under FreeRTOS to guarantee zero frame drops and sub-microsecond input latency:

```text
+-----------------------------------------------------------------------------+
|                                  ISD-Core                                   |
|                                                                             |
|  +-------------------------+          +----------------------------------+  |
|  |   Boot & Bring-Up       |          |         FreeRTOS Runtime         |  |
|  +-------------------------+          +----------------------------------+  |
|  | * Display Bring-up      |          |   CORE 1 (APP CPU)               |  |
|  | * Staged Bootloader     |          |   +--------------------------+   |  |
|  | * Core 0 Background     |          |   | Non-Blocking UI (~42 Hz) |   |  |
|  |   Hardware Init (1.8s)  |          |   | Watchface • 3D App Menu  |   |  |
|  +------------+------------+          |   +------------+-------------+   |  |
|               |                       |                | (Read <1 µs)    |  |
|               |                       |   +------------v-------------+   |  |
|               | Hand-off              |   |   Shared SensorState     |   |  |
|               | to Runtime            |   | (Mutex-Protected Bridge) |   |  |
|               |                       |   +------------^-------------+   |  |
|               |                       |                | (Write <1 µs)   |  |
|               |                       |   CORE 0 (PRO CPU)               |  |
|               |                       |   +--------------------------+   |  |
|               |                       |   | Sensor Task (10 - 100 Hz)|   |  |
|               |                       |   | 50 Hz IMU • Power/USB    |   |  |
|               |                       |   | Slow I2C Telemetry (1 Hz)|   |  |
|               |                       |   +--------------------------+   |  |
|               |                       +-----------------+----------------+  |
|               |                                         |                   |
|               +--------------------+--------------------+                   |
|                                    |                                        |
|                                    v                                        |
|  +-----------------------------------------------------------------------+  |
|  |                         Hardware Layer                                |  |
|  |  Sensors (I2C @ 400kHz): BNO085 • BME680 • OPT3001 • MAX17048 • RV3028|  |
|  |  Display (SPI @ 40MHz):  ST7789 IPS 240x240 (Double-Buffered PSRAM)  |  |
|  |  Inputs (IRAM ISRs):     BTN (GP13) • LEVER L/P/R (GP16/15/14)        |  |
|  +-----------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------+
```

### Hardware Bus Map

| Peripheral | Bus | Address / Pin | Speed | Role |
| :--- | :--- | :---: | :---: | :--- |
| **ST7789 IPS** | SPI (VSPI) | MOSI 11, SCLK 12, CS 3, DC 46 | 40 MHz | 240×240 RGB display (PSRAM double-buffered) |
| **BNO085** | I2C (`Wire`) | `0x4A` | 400 kHz | 9-DOF IMU (50 Hz rotation vectors & compass) |
| **BME680** | I2C (`Wire`) | `0x76` | 400 kHz | Temperature, Humidity, Pressure, Gas resistance |
| **OPT3001** | I2C (`Wire`) | `0x45` | 400 kHz | Photopic precision ambient light sensor (Lux) |
| **MAX17048** | I2C (`Wire`) | `0x36` | 400 kHz | LiPo fuel gauge (voltage, %, charge rate) |
| **RV-3028-C7**| I2C (`Wire`) | `0x52` | 400 kHz | Ultra-low power real-time clock (RTC) |
| **MAX30102** | I2C (`Wire`) | `0x57` | 400 kHz | Optical heart rate & pulse oximetry sensor |
| **Buzzer** | GPIO / PWM | `GPIO 10` | 2.7–4.0 kHz | Acoustic feedback & UI tick chimes |
| **NeoPixels** | GPIO | `GPIO 18` | 800 kHz | 4×4 WS2812B auxiliary matrix |
| **BQ25170** | GPIO | `/PG` 21, `/STAT` 47 | - | Hardware charger USB sense & charging status |

---

## 7. Building & Flashing

ISD-Core is built using **PlatformIO** targeting the ESP32-S3 with 16MB Flash and 8MB Octal PSRAM (`OPI_OPI` mode):

```bash
# Clone the repository
git clone https://github.com/Ghost3379/NX-ISD.git
cd NX-ISD/ISD-Core

# Compile firmware
pio run

# Flash to target board via USB-CDC
pio run --target upload

# Open real-time serial monitor (115200 baud)
pio device monitor
```

---

## License

ISD-Core is licensed under the GNU General Public License v3.0. See the root `LICENSE` file for details.
