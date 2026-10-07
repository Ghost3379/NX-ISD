# ISD-Core // User & Navigation Manual

Welcome to the **ISD-Core User & Navigation Manual** for the **NX-ISD** (Intelligent Sensor Device).

ISD-Core is built on a central design philosophy: **it is an intuitive, tactile smartwatch operating system—not a wearable spreadsheet of raw sensor dumps.** The user interface draws inspiration from retro-futuristic amber vector displays (TVA, Pip-Boy, and aerospace instrumentation), organized into **6 human-centered application decks** driven by physical controls, zero input lag, and real-time physical feedback.

> [!NOTE]
> 🛠️ **Looking for the Developer & Firmware Architecture Guide?**  
> For dual-core FreeRTOS scheduling, Octal PSRAM buffers, hardware bus maps, pinouts, and PlatformIO compilation, see [**README.md**](README.md).

---

## Table of Contents
1. [Physical Hardware Controls & Gestures](#1-physical-hardware-controls--gestures)
2. [Operating System Navigation Map](#2-operating-system-navigation-map)
3. [Level 1: Primary Shell Screens](#3-level-1-primary-shell-screens)
   - [Level 1A: Watchface HUD (Default Screen)](#level-1a-watchface-hud-default-screen)
   - [Level 1B: Quickpanel Action Grid](#level-1b-quickpanel-action-grid)
   - [Level 1C: Notifications Stack](#level-1c-notifications-stack)
4. [Level 2: 3D Cover Flow App Launcher](#4-level-2-3d-cover-flow-app-launcher)
5. [Level 3: The 6 Core Application Decks](#5-level-3-the-6-core-application-decks)
   - [App 1: VITALS (Internal Physiology)](#app-1-vitals-internal-physiology)
   - [App 2: ENVIRONMENT (External Ambience)](#app-2-environment-external-ambience)
   - [App 3: CLOCK (Temporal Operations)](#app-3-clock-temporal-operations)
   - [App 4: DEVICE (Silicon & Hardware Integrity)](#app-4-device-silicon--hardware-integrity)
   - [App 5: TOOLS (Tactical & Field Operations)](#app-5-tools-tactical--field-operations)
   - [App 6: SETTINGS (System Orchestration)](#app-6-settings-system-orchestration)
6. [Interactive Control Paradigms](#6-interactive-control-paradigms)
   - [360° Circular Radial Dials](#360-circular-radial-dials)
   - [4-Card Tactical Overview Menus](#4-card-tactical-overview-menus)
   - [Cinematic Smoothstep Backlight Transitions](#cinematic-smoothstep-backlight-transitions)
7. [Persistent Storage & Zero-Wear Policy](#7-persistent-storage--zero-wear-policy)
   - [How Your Settings Are Saved](#how-your-settings-are-saved)
   - [Offline PC Configuration Tool](#offline-pc-configuration-tool)
8. [Summary Navigation Quick-Reference](#8-summary-navigation-quick-reference)

---

## 1. Physical Hardware Controls & Gestures

The NX-ISD hardware eliminates touchscreens in favor of high-reliability physical controls that can be operated blind, with gloves, or in adverse field conditions:

```text
                     ┌───────────────────────────┐
                     │    ST7789 IPS Display     │
                     │         (240x240)         │
                     │          [ HUD ]          │
                     └───────────────────────────┘
                                         [ BTN ] (GPIO 13) ── Back / Exit / Wake
                     ┌───────────────────────────┐
      [ LEVER LEFT ] │      [ LEVER PUSH ]       │ [ LEVER RIGHT ]
        (GPIO 16)    │        (GPIO 15)          │   (GPIO 14)
       Prev / Dec    │    Select / 1.2s Charge   │   Next / Inc
```

### Control Bindings & Timing Parameters

| Control | Hardware | Gesture | Timing | Action |
| :--- | :---: | :--- | :---: | :--- |
| **LEVER LEFT** | `GPIO 16` | **Flick / Click** | $< 475\,\text{ms}$ | Previous card, scroll left, or decrement value. |
| | | **Hold** | $\ge 475\,\text{ms}$ | **Hold-to-repeat:** Cycles every $160\,\text{ms}$ for fast numeric dialing. |
| **LEVER RIGHT** | `GPIO 14` | **Flick / Click** | $< 475\,\text{ms}$ | Next card, scroll right, or increment value. |
| | | **Hold** | $\ge 475\,\text{ms}$ | **Hold-to-repeat:** Cycles every $160\,\text{ms}$ for fast numeric dialing. |
| **LEVER PUSH** | `GPIO 15` | **Click** | $< 1.2\,\text{s}$ | Select, toggle setting, confirm hero card, or enter Quickpanel. |
| | | **Tactical Charge** | $\ge 1.2\,\text{s}$ | **Hold Charge Gesture:** Sweeps energy arc to launch the 3D App Menu. |
| **BTN** | `GPIO 13` | **Click** | Any | **Global Back / Cancel:** Pops up one level; wakes display from Standby. |

* **Zero Input Lag:** Lever inputs trigger dedicated IRAM ISRs with $< 1\,\mu\text{s}$ latching and a $40\,\text{ms}$ software debouncing window.

---

## 2. Operating System Navigation Map

ISD-Core implements a strict **3-Tier Hierarchical Navigation Structure**. You are never trapped in deeply nested submenus and are never more than two clicks away from the home watchface:

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

## 3. Level 1: Primary Shell Screens

### Level 1A: Watchface HUD (Default Screen)

The home screen delivers immediate situational awareness without visual clutter:

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

* **Header Bar:** Shows active view name and live battery percentage with charging badge (`[CHG]`).
* **Digital Clock:** Synchronized with the ultra-low-power RV-3028 RTC ($0.05\,\mu\text{A}$ timekeeping).
* **Telemetry Badges:** Real-time environmental readings filtered to eliminate sensor jitter.
* **Compass & Spirit Bubble:** Dynamic 360° rotating azimuth reticle with shortest-path circular interpolation, framing a real-time 2D bubble level driven by the BNO085 accelerometer.
* **Tactical Charge Ring:** When `LEVER_PUSH` is held, an amber radial energy meter sweeps clockwise around the compass ring. Completing the 1.2s charge triggers an audible chime and launches the 3D App Menu.

---

### Level 1B: Quickpanel Action Grid

Accessed by flicking **`LEVER_RIGHT`** from the Watchface HUD. A 2×3 grid of tactical rounded action tiles:

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

## 4. Level 2: 3D Cover Flow App Launcher

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
* **Launching an App:** Click **`LEVER_PUSH`** on the centered hero card to enter. Press **`BTN`** to spring back to the Watchface HUD.

---

## 5. Level 3: The 6 Core Application Decks

ISD-Core groups all functionality into **6 dedicated, human-centered decks**:

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

### App 5: TOOLS (Tactical & Field Operations)
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
*Focus: OS customization, hardware configuration, and tactile acoustic orchestration.*

```text
+-----------------------------------+
| ISD-Core // SETTINGS        [98%] |  Header with real-time battery status
| --------------------------------- |
|  +-----------------------------+  |
|  | DISPLAY                  >  |  |  Category Card 1
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | NOTIFICATIONS            >  |  |  Category Card 2
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | POWER SAVE               >  |  |  Category Card 3
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | BUZZER                   >  |  |  Category Card 4
|  +-----------------------------+  |
| --------------------------------- |
| [PUSH] SELECT  < LEVER > NAV [BTN]|  Navigation context bar
+-----------------------------------+
```

Settings is structured into **6 distinct categories** adhering to a strict **4-box viewport rule** with an active vertical scrollbar track and thumb. The list never cramps options; exactly 4 cards are shown at a time, smoothly scrolling as you navigate:

1. **DISPLAY:**
   * **BRIGHTNESS:** 360° circular radial dial with orbital satellite pip (10% to 100% in 5% steps).
   * **AUTO DIM:** Toggles real-time ambient light compensation via the OPT3001 sensor.
   * **TILT TO WAKE:** Opens a dedicated **4-Card Overview Menu** (`OFF`, `SENSITIVE`, `BALANCED`, `SLUGGISH`).
   * **TIMEOUT:** 360° circular dial from 5s to 300s (or `NEVER` / Always-On at 0s).
   * **WRIST COVER:** Toggles instant standby upon optical sensor occlusion.
   * **FADE ANIM:** 360° circular dial for display on/off backlight transition duration (0ms to 2000ms in 50ms steps).
2. **NOTIFICATIONS:**
   * **METHOD:** Dedicated 4-card overview menu (`SILENT`, `ALL`, `SOUND ONLY`, `LIGHTS ONLY`).
   * **MATRIX LED:** Toggle 4×4 NeoPixel matrix optical alert flashes.
   * **HRM REMINDER:** Cycle cyclic spot-measurement prompts (`OFF`, `30m`, `1h`, `2h`).
3. **POWER SAVE:**
   * **ECO MODE:** Toggle reduced sensor poll rates and standby CPU throttling.
   * **AUTO STANDBY:** Enable/disable automatic screen-off sleep upon timeout.
   * **SENSOR SLEEP:** Power down environmental/IMU sensor rails during standby.
4. **BUZZER (Acoustic Orchestration):**
   * **MASTER AUDIO:** Quick toggle for silent / unmuted system state.
   * **VOLUME:** 360° circular dial with acoustic soundwave arcs (10% to 100% duty cycle).
   * **TICK DURATION:** 360° circular dial with digital square-wave pulse width graphic (2ms to 30ms).
   * **TONE PITCH:** 360° circular dial with animated sine-wave graphic (1600 Hz to 4400 Hz in 100 Hz steps).
5. **NPM (4×4 NeoPixel Matrix):**
   * **MATRIX POWER:** High-side PMOS power gate toggle (0 µA quiescent draw when OFF).
   * **BRIGHTNESS:** 360° circular dial with 4×4 matrix icon (10% to 100% in 5% steps).
   * **ANIMATION:** Cycle real-time patterns (`RADAR`, `RAIN`, `PLASMA`, `TRACER`, `BREATH`, `OFF`).
   * **TEST PULSE:** Fires an instantaneous high-vis amber verification pulse.
6. **NX-AIS (Autonomous Intelligence):**
   * **CO-PROCESSOR:** Enable/disable background algorithmic supervisor.
   * **ADAPTIVE:** Toggle dynamic environmental context thresholds.
   * **DIAGNOSTICS:** Hardware & sensor fusion diagnostics.

---

## 6. Interactive Control Paradigms

### 360° Circular Radial Dials

Whenever adjusting continuous numerical values (such as Brightness, Volume, Pitch, or Timeouts), ISD-Core uses an aerospace-style 360° circular radial dial:

```text
+-----------------------------------+
| DISPLAY // BRIGHTNESS             |  View title
| --------------------------------- |
|             . - ~ - .             |  Outer circular graduated ring
|         . '           ' .         |
|       /                   \       |
|      |        [ 65% ]      * |    |  Center Value Readout & Orbital Pip (*)
|       \                   /       |
|         . .           . .         |
|             ' - ~ - '             |
| --------------------------------- |
| < LEVER > ADJ   [PUSH] CONFIRM    |  Prompt: Lever dials, Push commits
+-----------------------------------+
```

* **Orbital Satellite Pip:** A bright circular marker orbits around the ring proportional to the active value.
* **Contextual Center Graphic:**
  * **Volume:** Emits radiating acoustic speaker arcs.
  * **Tick Duration:** Displays a digital square-wave pulse width graphic.
  * **Tone Pitch:** Displays an animated sine-wave waveform whose wavelength scales with frequency.
* **Zero Input Lag:** Turning the lever continuously adjusts the value in RAM with instant feedback.
* **Hold-to-Repeat:** Holding the lever Left or Right spins the dial smoothly at 160ms intervals.

---

### 4-Card Tactical Overview Menus

For multi-choice settings with 3–4 discrete states, ISD-Core uses a dedicated full-screen 4-card overview menu:

```text
+-----------------------------------+
| TILT TO WAKE                [ 2 ] |  Menu Title & Active Index
| --------------------------------- |
|  +-----------------------------+  |
|  | ( ) OFF                     |  |  Card 0: Manual wake only
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | ( ) SENSITIVE (12 - 80 deg) |  |  Card 1: Light wrist glance
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | (*) BALANCED  (22 - 72 deg) |  |  Card 2: Standard natural glance (Active)
|  +-----------------------------+  |
|  +-----------------------------+  |
|  | ( ) SLUGGISH  (35 - 65 deg) |  |  Card 3: Deliberate heavy raise
|  +-----------------------------+  |
| --------------------------------- |
| [PUSH] SELECT  < LEVER > NAV [BTN]|
+-----------------------------------+
```

* **Radio Selector (`(*)`):** An active indicator shows the currently applied setting.
* **Instant Confirmation:** Flicking the lever moves the highlight box; clicking `[PUSH]` selects the new mode and saves it to NAND flash.

---

### Cinematic Smoothstep Backlight Transitions

Rather than abruptly cutting off or ramping linearly, the display backlight fades smoothly using a mathematical **smoothstep curve**:

```text
Intensity
  100% ┼                                   .───────
       │                               . ─
       │                            . '
       │                         . '
       │                      . '
       │                   . '
       │               . '
       │          . ─ '
    0% ┴─────────'─────────────────────────────────
       0ms                   t                  FadeAnimMs
```

* **Cubic Hermite Interpolation:** $f(t) = t^2 \times (3 - 2t)$ for $t \in [0, 1]$.
* **Eye-Friendly:** Soft gradual start and end prevents optical fatigue when waking up in dark environments.
* **Customizable:** Adjust from **0 ms** (instant snap) to **2000 ms** (luxurious fade) via `Settings -> Display -> Fade Anim`.

---

## 7. Persistent Storage & Zero-Wear Policy

### How Your Settings Are Saved
All settings persist across power cuts and full battery drains on the onboard **ZDSD NAND Flash** in `/sys/config.bin`:

1. **Zero-Wear Policy:** Rotating radial dials modifies settings exclusively in RAM. Spinning a dial 1,000 times performs **0 writes** to flash, preserving NAND cell lifespan indefinitely.
2. **Commit on `[PUSH]`:** Pressing the lever down to confirm a setting flushes the new configuration to flash.
3. **Commit on Exit:** Pressing `[BTN]` to back out of a menu automatically flushes any pending changes once.
4. **Atomic Safety:** Data is written to `/sys/config.tmp` first; once fully flushed, it atomically renames to `/sys/config.bin`. If power cuts out mid-write, your previous configuration remains 100% intact.
5. **Fail-Safe Boot:** If the NAND storage is absent or corrupted, the system boots into factory defaults immediately without hanging.

### Offline PC Configuration Tool
You can inspect, backup, and modify configuration files offline using the included Python CLI tool:

```bash
# Read and inspect a config.bin file:
python tools/nx_config_tool.py read config.bin

# Export binary configuration to human-readable JSON:
python tools/nx_config_tool.py to-json config.bin -o config.json

# Compile modified JSON back to binary with verified CRC-16:
python tools/nx_config_tool.py from-json config.json -o config.bin

# Generate fresh factory default binary config:
python tools/nx_config_tool.py create-default -o config.bin
```

---

## 8. Summary Navigation Quick-Reference

| To accomplish this... | Do this from the current screen... |
| :--- | :--- |
| **Open Quickpanel** | Flick **`LEVER_RIGHT`** from Watchface HUD. |
| **Open Notifications** | Flick **`LEVER_LEFT`** from Watchface HUD. |
| **Open 3D App Menu** | Press and hold **`LEVER_PUSH` (1.2s)** on Watchface HUD. |
| **Browse App Cards** | Flick or hold **`LEVER_LEFT`** / **`LEVER_RIGHT`** in the 3D App Menu. |
| **Enter an App** | Click **`LEVER_PUSH`** on any centered hero card. |
| **Adjust Radial Dial** | Flick or hold **`LEVER_LEFT`** / **`LEVER_RIGHT`** inside any dial screen. |
| **Confirm & Save Setting** | Click **`LEVER_PUSH`** inside any dial screen or overview menu. |
| **Go Back / Exit / Save** | Press **`BTN`** (GPIO 13) at any time. |
| **Instant Screen Standby** | Open Quickpanel $\to$ `[SHUTDOWN]` $\to$ Select `STANDBY`. |
| **Wake from Standby** | Press **`BTN`**, flick any lever, tilt wrist (if enabled), or plug USB power. |
