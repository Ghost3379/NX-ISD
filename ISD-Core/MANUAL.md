# ISD-Core // User & Navigation Manual

The **ISD-Core** operating system turns the **NX-ISD** into an intuitive, tactile wearable device. Built on an amber-orange retro-modern vector aesthetic (inspired by the TVA / Pip-Boy / military instrumentation), ISD-Core avoids dense data-dump spreadsheets, organizing all capabilities into **6 human-centered application decks** driven by hardware interrupts and real-time physical feedback.

---

## 1. Physical Hardware Controls

The watch is operated using four primary hardware controls: a 3-way tactile navigation lever and a dedicated system button.

```text
               ┌───────────────────────┐
               │    240x240 Display    │
               │                       │
               │        [ HUD ]        │
               │                       │
               └───────────────────────┘
                                   [ BTN ] (GPIO 13) ── System Back / Wake
               ┌───────────────────────┐
[ LEVER LEFT ] │    [ LEVER PUSH ]     │ [ LEVER RIGHT ]
  (GPIO 16)    │      (GPIO 15)        │   (GPIO 14)
  Prev / Dec   │  Select / 1.2s Charge │   Next / Inc
```

### Control Bindings:

| Control | Hardware | Gesture | Global Action |
| :--- | :--- | :--- | :--- |
| **LEVER LEFT** | GPIO 16 | **Click** | Previous card, scroll left, or decrement setting value. |
| | | **Hold (≥475ms)** | Smooth continuous repeat (160ms cycle) for fast dialing. |
| **LEVER RIGHT** | GPIO 14 | **Click** | Next card, scroll right, or increment setting value. |
| | | **Hold (≥475ms)** | Smooth continuous repeat (160ms cycle) for fast dialing. |
| **LEVER PUSH** | GPIO 15 | **Click** | Select, confirm active card, toggle settings, or enter Quickpanel. |
| | | **Hold (1.2s)** | **Tactical Compass Charge:** Charges an energy arc on the dial to launch the 3D App Menu. |
| **BTN** | GPIO 13 | **Click** | **Back / Exit:** Returns to previous view, cancels action, or wakes display from Standby. |

---

## 2. Navigation Architecture & View Hierarchy

ISD-Core uses a strict 3-tier hierarchical navigation structure. You are never more than two clicks away from the main watchface.

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
│   (Pressing BTN at any time pops back up to the Level 2 Launcher or Level 1 HUD)       │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Level 1: Primary Shell Screens

### A. Watchface HUD (Default Screen)
The home screen delivers immediate situational awareness without visual clutter:
* **Header Bar:** Shows active view name (`NX-ISD`) and real-time battery percentage with `[CHG]` status.
* **Large Digital Time:** Crisp hours, minutes, and seconds (`HH:MM:SS`) synced to the RV-3028 RTC.
* **Telemetry Badges:** Ambient temperature (°C) and relative humidity (%) continuously updated from the BME680.
* **Dynamic Compass Reticle:** Rotating compass ring with cardinal points ($N, E, S, W$) and an internal **2D Spirit Bubble** tracking wrist pitch and roll.
* **Circular Battery Arc:** Radial gauge framing the compass showing remaining battery capacity.

### B. Quickpanel (Flick Lever Right from Home)
A 2×3 grid of tactical rounded action tiles (Android Wear style) providing 1-click access to core controls:
1. **SETTINGS:** Quick jump to system settings.
2. **HRM:** One-tap spot measurement of heart rate.
3. **ECO MODE:** Toggle power-saving mode (reduced sensor poll rates).
4. **BRIGHTNESS:** Opens the interactive circular dial (10% to 100% in 5% steps).
5. **SILENT:** Toggles buzzer and audio feedback.
6. **SHUTDOWN:** Enters the Power Menu (Standby / Deep Sleep / Reboot).

### C. Notifications Panel (Flick Lever Left from Home)
Chronological notification stack for incoming alerts, alarms, and NX-AIS advisory messages. Pressing `BTN` dismisses or archives items.

---

## 4. Level 2: 3D Cover Flow App Menu

Accessible from the Watchface HUD by **holding `LEVER_PUSH` for 1.2 seconds**. An amber circular charge meter sweeps along the compass reticle before springing into the 3D Launcher.

```text
       \ ──────────────────────────────────────────────────────── /  <-- Top Cradle Rail
         ┌──────┐              ┌──────────────┐              ┌──────┐
         │ VIT  │              │ ENVIRONMENT  │              │ CLK  │
         │ (3D) │              │  Hero Card   │              │ (3D) │
         │      │              │ (Center 1:1) │              │      │
         └──────┘              └──────────────┘              └──────┘
       / ──────────────────────────────────────────────────────── \  <-- Bottom Cradle Rail
```

* **Interactive Carousel:** Moving the lever Left or Right smoothly spins the deck with spring-damper physics (`scrollPos += diff * 0.48f`).
* **Trapezoidal 3D Depth:** Neighboring cards scale down and distort with perspective triangles to simulate rotation in 3D space.
* **Mechanical Cradle Trays:** Slanted floor and ceiling rails frame the viewport, giving the launcher a tactile hardware bay feel.
* **Launching an App:** Click **`LEVER_PUSH`** on the centered hero card to enter.
* **Exiting:** Press **`BTN`** to instantly exit back to the Watchface HUD.

---

## 5. Level 3: The 6 Core Application Decks

Each app is a structured suite of functional tools. Math and sensor fusion (**NX-MSF**) and autonomous intelligence (**NX-AIS**) operate silently under the hood to deliver finished insights rather than raw data dumps.

```text
┌───────────────────┬───────────────────┬───────────────────┐
│     PHYSIOLOGY    │     TEMPORAL      │      SILICON      │
│    [ VITALS ]     │    [ CLOCK ]      │    [ DEVICE ]     │
├───────────────────┼───────────────────┼───────────────────┤
│    ENVIRONMENT    │      ACTION       │      CONTROL      │
│  [ ENVIRONMENT ]  │    [ TOOLS ]      │   [ SETTINGS ]    │
└───────────────────┴───────────────────┴───────────────────┘
```

---

### App 1: VITALS (Human Physiology)
*Focus: Personal physical health, recovery, and stress.*

* **Last Measured Spot-Check:** Displays last recorded Heart Rate (BPM) and $SpO_2$ (%) alongside the exact time elapsed since measurement (e.g., `72 BPM • 14m ago`).
* **Live Optical Measurement:** Initiates a live pulse/oxygen scan using the MAX30102 sensor. Displays animated ECG trace and real-time pulse waveform.
* **Autonomic Stress Score:** Utilizes **NX-MSF** to evaluate Inter-Beat Intervals ($R\text{-}R$ intervals) and RMSSD heart-rate variability, outputting a 0–100 Stress Index (Relaxed, Balanced, High Tension).
* **Cyclic Measurement Reminders:** Configure automated measurement prompts (e.g., every 30m, 1h, 2h). Alerts can be routed via subtle 4×4 NeoPixel matrix patterns, a soft buzzer warble, or both.

---

### App 2: ENVIRONMENT (External Ambience)
*Focus: Meteorological monitoring, ambient comfort, and alpine predictive safety.*

* **Atmospheric Dashboard:** Consolidated readout of Temperature (°C/°F), Relative Humidity (%), Barometric Pressure (hPa), and Photopic Ambient Light (Lux via OPT3001).
* **Air Quality & Gas Resistance:** Evaluates BME680 metal-oxide sensor resistance ($R_{\text{gas}}$) to report VOC levels and indoor air ventilation status.
* **Storm Predictor (NX-MSF):** Analyzes rolling 3-hour barometric pressure tendencies ($\Delta P / \Delta t$). Displays incoming storm warnings if pressure falls $> 2.5\,\text{hPa}/3\text{h}$.
* **Dew Point & Fog Intel:** Computes dew point ($T_{\text{dew}}$) via the Magnus-Tetens formula. Warns hikers of trail condensation or mountain fog immersion when $(T - T_{\text{dew}}) \le 1.0^\circ\text{C}$.
* **Thermal Strain & Humidex:** Combines heat and humidity into an intuitive perceived comfort rating.

---

### App 3: CLOCK (Temporal Operations)
*Focus: Timekeeping, intervals, alarms, and non-volatile persistence.*

* **Technical Stopwatch:** Millisecond precision chronograph with split-lap logging and digital lap history.
* **Countdown Timer:** Quick-dial timer with acoustic alarm and pulsing amber NeoPixel matrix alert upon completion.
* **Multi-Slot Alarms:** Configurable recurring or one-shot alarms synced to the battery-backed RV-3028 RTC.
* **World Clock:** Track secondary UTC / time-zone offsets.
* **Non-Volatile State Persistence:** Automatically commits active timers, alarms, and clock preferences to on-board NAND Flash, ensuring seamless state recovery even if the battery runs completely flat.

---

### App 4: DEVICE (Silicon & Hardware Integrity)
*Focus: Internal self-diagnostics, energy accounting, and system health.*

* **NX-SDS (Self-Diagnostic Suite):**
  * **I2C Bus Audit:** Probes all bus peripherals and executes automated 9-clock bus un-wedging if a line is held low.
  * **Sensor Proof-Testing:** On-demand self-tests for the BNO085 IMU, BME680 heater plate, and OPT3001.
  * **Oscillator Watchdog:** Checks the RV-3028 Oscillator Stop Flag (`OSF`) to verify crystal stability and rail voltage adequacy.
* **Battery Intelligence (MAX17048):** Reports cell terminal voltage ($V_{\text{cell}}$), discharge/charge rate (%/hr), estimated internal cell resistance ($R_{\text{int}}$), and health cycle statistics.
* **Memory & Storage Gauges:** Visual bar meters showing Octal PSRAM utilization (8MB), FreeRTOS heap watermarks, and NAND Flash storage wear metrics.
* **Firmware Version:** Displays ISD-Core release tag (`v0p3`), build timestamp, and ESP32-S3 chip revision.

---

### App 5: TOOLS (Tactical & Wireless Operations)
*Focus: Field utilities, wireless reconnaissance, and companion sync.*

* **NX-Uplink Companion Bridge:** Manages wireless pairing and synchronization with the companion web dashboard ([www.nx-uplink.base44.app](https://www.nx-uplink.base44.app)) for telemetry export and firmware profiles.
* **Wireless Sniffer & RSSI Meter:** Scans local 2.4 GHz WiFi channels and Bluetooth Low Energy (BLE) advertisements, rendering signal strength RSSI gradient meters.
* **Tactical 2D Spirit Level:** High-precision digital bubble level utilizing the BNO085 accelerometer to balance flat surfaces with numeric pitch/roll degree readouts.
* **Hardware Pin Monitor:** Real-time state inspector for onboard digital rails and charger status lines (`/PG`, `/STAT`).

---

### App 6: SETTINGS (System Orchestration)
*Focus: OS customization, power management, and advisory intensity.*

* **Time & Date Setup:** Manual RTC synchronization and 12h/24h format selection.
* **Display & Gestures:** 
  * Select Auto-Brightness (regulated continuously by the OPT3001 light sensor) or fixed manual level.
  * Toggle **Wrist-Wake Gesture** (detects inward wrist rotation using IMU angular velocity).
* **Power Management:**
  * Configure Eco Mode behaviors.
  * Set Auto-Standby inactivity timeouts (30s, 60s, 2m, Never).
* **NX-AIS Intelligence Supervisor:**
  * Toggle automated contextual advisory messages.
  * Set notification intensity: **Full** (Screen popup + audio + glyphs), **Subtle** (4×4 matrix glyphs only), or **Muted**.
* **NeoPixel Matrix & Audio:**
  * Choose notification chime profiles and buzzer tick volume.
  * Select matrix ambient brightness and standby animations.
* **Notification Routing:** Granular permissions matrix defining which subsystems can trigger popups, audio chimes, or matrix glyphs.

---

## 6. Summary Navigation Quick-Reference

| To accomplish this... | Do this from the current screen... |
| :--- | :--- |
| **Open Quickpanel** | Flick **`LEVER_RIGHT`** from Watchface HUD. |
| **Open Notifications** | Flick **`LEVER_LEFT`** from Watchface HUD. |
| **Open 3D App Menu** | Press and hold **`LEVER_PUSH` (1.2s)** on Watchface HUD. |
| **Browse App Cards** | Flick or hold **`LEVER_LEFT`** / **`LEVER_RIGHT`** in the 3D App Menu. |
| **Enter an App** | Click **`LEVER_PUSH`** on any centered hero card. |
| **Go Back / Exit** | Press **`BTN`** (GPIO 13) at any time. |
| **Quick Standby (Screen Off)** | Open Quickpanel $\to$ SHUTDOWN $\to$ Select STANDBY. |
| **Wake from Standby** | Press **`BTN`**, flick any lever, or connect USB power. |
