# ISD-Core // Architecture, Stats & Developer Guide

Welcome to **ISD-Core**, the custom dual-core wearable operating system designed for the **NX-ISD** (Intelligent Sensor Device) powered by the ESP32-S3.

ISD-Core is built on a central design philosophy: **it is an intuitive, tactile smartwatch operating system—not a wearable spreadsheet of raw sensor dumps.** Mathematical sensor fusion (**NX-MSF**), autonomous contextual advisory (**NX-AIS**), and hardware self-diagnostics (**NX-SDS**) operate silently under the hood, distilling complex multi-sensor telemetry into high-value insights, glanceable widgets, and physical feedback.

The user interface draws inspiration from retro-futuristic amber vector displays (TVA, Pip-Boy, and aerospace instrumentation), rendered at **~42 FPS** on a 240×240 IPS display using zero-copy Octal PSRAM double-buffering.

> [!TIP]
> 📖 **Looking for the User & Screen Navigation Manual?**  
> For the visual guide, ASCII screen mockups, physical gesture timings, and application walkthroughs, see [**MANUAL.md**](MANUAL.md).

---

## Table of Contents
1. [Firmware Architecture & FreeRTOS Tasks](#1-firmware-architecture--freertos-tasks)
2. [Memory Architecture & Rendering Pipeline](#2-memory-architecture--rendering-pipeline)
3. [System Performance Stats & Engineering Metrics](#3-system-performance-stats--engineering-metrics)
4. [Hardware Bus Map & Centralized Pinout](#4-hardware-bus-map--centralized-pinout)
5. [Centralized I2C Bus Address Map](#5-centralized-i2c-bus-address-map)
6. [Persistent Storage Subsystem (`StorageManager`)](#6-persistent-storage-subsystem-storagemanager)
7. [Mathematical Models & Graphics Curves](#7-mathematical-models--graphics-curves)
8. [Building, Flashing & Development Workflow](#8-building-flashing--development-workflow)
9. [License](#9-license)

---

## 1. Firmware Architecture & FreeRTOS Tasks

ISD-Core leverages the asymmetric dual-core architecture of the ESP32-S3 (Xtensa LX7 @ 240 MHz) under FreeRTOS. UI rendering and high-frequency sensor acquisition are decoupled onto independent CPU cores to guarantee **zero frame drops** and **sub-microsecond input latching**:

```text
+-----------------------------------------------------------------------------+
|                                  ISD-Core                                   |
|                                                                             |
|  +-------------------------+          +----------------------------------+  |
|  |   Boot & Bring-Up       |          |         FreeRTOS Runtime         |  |
|  +-------------------------+          +----------------------------------+  |
|  | * Display & Backlight   |          |   CORE 1 (APP CPU)               |  |
|  | * Octal PSRAM Buffer    |          |   +--------------------------+   |  |
|  | * Staged Splashloader   |          |   | Non-Blocking UI (~42 Hz) |   |  |
|  | * Core 0 Sensor Task    |          |   | Watchface • 3D App Menu  |   |  |
|  | * StorageManager Probe  |          |   | StorageManager (Atomic)  |   |  |
|  +------------+------------+          |   +------------+-------------+   |  |
|               |                       |                | (Read <1 µs)    |  |
|               |                       |   +------------v-------------+   |  |
|               | Hand-off              |   |   Shared SensorState     |   |  |
|               | to Runtime            |   | (Mutex-Protected Bridge) |   |  |
|               |                       |   +------------^-------------+   |  |
|               |                       |                | (Write <1 µs)   |  |
|               |                       |   CORE 0 (PRO CPU)               |  |
|               |                       |   +--------------------------+   |  |
|               |                       |   | Fast Sensors (50 - 100Hz)|   |  |
|               |                       |   | BNO085 IMU • Compass     |   |  |
|               |                       |   | Slow Sensors (1 Hz)      |   |  |
|               |                       |   | BME680 • OPT3001 • MAX   |   |  |
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
|  |  Storage (SPI @ 20MHz):  ZDSD NAND Flash (CS=47, /sys/config.bin)     |  |
|  |  Inputs (IRAM ISRs):     BTN (GP13) • LEVER L/P/R (GP16/15/14)        |  |
|  +-----------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------+
```

### Core Assignment Breakdown

| Core | Task Name | Priority | Frequency | Responsibilities |
| :--- | :--- | :---: | :---: | :--- |
| **Core 1** | `vUITask` (Main Loop) | `1` | **~42 Hz** | LovyanGFX display rendering, 3D Cover Flow animation, Watchface HUD, radial dial physics, input event consumption, atomic NAND storage commit. |
| **Core 0** | `vFastSensorTask` | `5` | **50–100 Hz** | High-speed BNO085 9-DOF IMU rotation vector polling, step counting, compass heading calculation, wrist-flick gesture detection. |
| **Core 0** | `vSlowSensorTask` | `2` | **1 Hz** | Low-frequency I2C polling: BME680 (climate/gas), OPT3001 (lux), MAX17048 (fuel gauge), RV-3028 (RTC sync), USB VBUS sense. |
| **ISRs** | Hardware Edge ISRs | High (IRAM) | *Event* | Lever Left/Push/Right and Button falling-edge capture into thread-safe atomic latch bitmasks with 40 ms software debounce. |

### Inter-Process Communication (IPC)
Core 0 and Core 1 communicate through a centralized, thread-safe telemetry cache defined as `SensorState`:
* **Mutex Protection:** A FreeRTOS mutex (`xSensorMutex`) guards write transactions from Core 0 and read transactions from Core 1.
* **Exchange Latency:** Sensor snapshot acquisitions take $< 1\,\mu\text{s}$, preventing UI pipeline stalls.
* **Staleness Tracking:** Each telemetry group carries millisecond timestamps (`lastReadMs`) allowing the UI to flag stale sensor values gracefully.

---

## 2. Memory Architecture & Rendering Pipeline

The NX-ISD uses the **ESP32-S3-WROOM-1-N16R8** module configured with:
* **16 MB Quad SPI Flash** (firmware binary, assets, partition table).
* **8 MB Octal PSRAM** (`OPI_OPI` mode clocked at 80 MHz).

```text
0x3F800000 ┌────────────────────────────────────────────────────────┐
           │ Octal PSRAM Pool (8.0 MB Total)                        │
           │                                                        │
           │ ┌────────────────────────────────────────────────────┐ │
           │ │ Canvas 1: Primary DMA Framebuffer (240x240x16b)    │ │ ~115 kB
           │ ├────────────────────────────────────────────────────┤ │
           │ │ Canvas 2: Double-Buffer Sprite (240x240x16b)       │ │ ~115 kB
           │ ├────────────────────────────────────────────────────┤ │
           │ │ Dynamic Sprite Cache (Icons, Glyphs, Gauges)       │ │ ~256 kB
           │ ├────────────────────────────────────────────────────┤ │
           │ │ FreeRTOS Task Stacks & Dynamic Allocation Pool     │ │ ~7.5 MB
           │ └────────────────────────────────────────────────────┘ │
0x40000000 └────────────────────────────────────────────────────────┘
```

### Zero-Copy Double-Buffered Pipeline
1. **Back-Buffer Composition:** The active UI view draws vector primitives, anti-aliased arcs, and text directly onto an internal PSRAM-backed `LGFX_Sprite`.
2. **Push Transaction:** The sprite is pushed to the ST7789 display controller over the 40 MHz SPI bus (`SPI2_HOST`) in a single high-speed burst.
3. **Zero Tearing:** Eliminates screen flicker, partial draw artifacts, and rolling raster lines without requiring a hardware VSYNC pin.

---

## 3. System Performance Stats & Engineering Metrics

Key real-world operational benchmarks measured on hardware revision **v1p3**:

| Metric | Measured Value | Notes & Context |
| :--- | :---: | :--- |
| **Display Frame Rate** | **~42 FPS** | Continuous vector rendering on 240×240 IPS (23.8 ms frame budget) |
| **Frame Draw Time** | **18.2 ms** | PSRAM back-buffer render time on Core 1 |
| **SPI Display Throughput** | **40 MHz** | ~5.6 ms transfer time for 240×240 16-bit color frame |
| **Input Latch Latency** | **$< 1\,\mu\text{s}$** | Direct hardware IRAM interrupt edge capture |
| **Debounce Window** | **40 ms** | Software timing window rejecting switch release bounce |
| **Hold-to-Repeat Cycle** | **160 ms** | Fast numeric dialing after 475 ms initial hold |
| **Tactical Charge Duration**| **1200 ms** | Deliberate energy hold to enter 3D App Menu |
| **Backlight PWM Frequency** | **5.0 kHz** | High-frequency flicker-free dimming via N-MOSFET `Q4` |
| **Storage Config Struct** | **64 Bytes** | Cache-aligned packed binary record (`/sys/config.bin`) |
| **Dial NAND Write Count** | **0 Writes** | Turning radial dials modifies RAM only; 1,000 dial clicks = 0 writes |
| **NAND Commit Latency** | **~12 ms** | Atomic write (`config.tmp` $\to$ `config.bin`) with CRC-16 check |
| **System Idle Current** | **~15 mA** | Screen off, ESP32 light sleep, sensors idle |
| **Full Operational Current**| **~95–130 mA** | Display 60%, 4×4 matrix dynamic animation, BNO085 fusion active |

---

## 4. Hardware Bus Map & Centralized Pinout

Defined in [`ISD-Core/src/pins.h`](src/pins.h):

```text
                      ┌───────────────────────────┐
                      │    ST7789 IPS Display     │
                      │         (240x240)         │
                      └───────────────────────────┘
                                          [ BTN ] (GPIO 13) ── Back / Exit / Wake
                      ┌───────────────────────────┐
       [ LEVER LEFT ] │      [ LEVER PUSH ]       │ [ LEVER RIGHT ]
         (GPIO 16)    │        (GPIO 15)          │   (GPIO 14)
        Prev / Dec    │    Select / 1.2s Charge   │   Next / Inc
```

### Complete GPIO Pinout Matrix

| GPIO Pin | Pin Name | Direction | Electrical Domain | Connected Peripheral / Signal |
| :---: | :--- | :---: | :---: | :--- |
| **1** | `PWM_TFT` | Output | `+3V3` | Display Backlight PWM (gate of N-MOSFET `Q4`, active HIGH) |
| **2** | `INT_DOF` | Input | `+3V3` | BNO085 Motion Interrupt (active LOW) |
| **4** | `ALERT` | Input | `+3V3` | MAX17048 Fuel Gauge Alert (active LOW) |
| **5** | `INT_HR` | Input | `+1V8` | MAX30102 Optical Heart Rate Interrupt (active LOW) |
| **6** | `INT_RTC` | Input | `+3V3` | RV-3028 RTC Alarm / Periodic Timer (active LOW) |
| **8** | `I2C_SDA` | Bi-dir | `+3V3` | I2C Data Line (10k pull-up `R18` to `+3V3`) |
| **9** | `I2C_SCL` | Output | `+3V3` | I2C Clock Line (10k pull-up `R19` to `+3V3`) |
| **10** | `BUZZER` | Output | `+3V3` | Electromagnetic Buzzer PWM (gate of N-MOSFET `Q3`) |
| **11** | `BAT_STAT` | Input | `+3V3` | BQ25170 Charger Status (LOW = Charging, HIGH = Complete) |
| **12** | `USB_DETECT`| Input | `+3V3` | Resistor divider from USB 5V VBUS (HIGH when plugged in) |
| **13** | `BTN` | Input | `+3V3` | Main Tactile Push-Button (Active LOW, debounced) |
| **14** | `LEVER_RIGHT`| Input | `+3V3` | Navigation Lever Right (Active LOW, hardware-mirrored) |
| **15** | `LEVER_PUSH` | Input | `+3V3` | Navigation Lever Center Click (Active LOW) |
| **16** | `LEVER_LEFT` | Input | `+3V3` | Navigation Lever Left (Active LOW, hardware-mirrored) |
| **17** | `PWR_NPM` | Output | `+3V3` | NeoPixel Matrix Power Gate (HIGH = ON, LOW = 0µA cutoff) |
| **18** | `NPM` | Output | `VBAT` | WS2812B Serial Data Stream (800 kHz NZR) |
| **21** | `TFT_RS` | Output | `+2V8` | ST7789 Command / Data selection (via TXB0106) |
| **38** | `TFT_RST` | Output | `+2V8` | ST7789 Hardware Reset Line (active LOW) |
| **39** | `INT_ALS` | Input | `+3V3` | OPT3001 Light Sensor Threshold Interrupt (active LOW) |
| **40** | `SPI_SCK` | Output | `+3V3` | SPI Bus Serial Clock (shared between Display & Storage) |
| **41** | `SPI_MISO`| Input | `+3V3` | SPI Bus Data In (from ZDSD NAND Flash) |
| **42** | `SPI_MOSI`| Output | `+3V3` | SPI Bus Data Out (to Display & ZDSD NAND Flash) |
| **47** | `CS_SD` | Output | `+3V3` | ZDSD NAND Flash Chip Select (active LOW) |
| **48** | `CS_TFT` | Output | `+2V8` | ST7789 TFT Display Chip Select (active LOW) |

### SPI Bus Arbitration
The ST7789 IPS display controller and the ZDSD NAND Flash share `SPI2_HOST` (SCK 40, MISO 41, MOSI 42). To prevent display corruption during file I/O:
1. `StorageManager` explicitly drives `CS_TFT` (`GPIO 48`) **HIGH** (deselected) before asserting `CS_SD` (`GPIO 47`) **LOW**.
2. Display SPI transactions run at **40 MHz**; SD/NAND transactions run at **20 MHz**.
3. Upon completing a NAND write/read transaction, `CS_SD` is released **HIGH** before handing the bus back to LovyanGFX.

---

## 5. Centralized I2C Bus Address Map

The I2C bus (`Wire`) runs at **400 kHz Fast Mode**:
* **SDA:** `GPIO 8`
* **SCL:** `GPIO 9`

| Device | Part # | 7-Bit Address | Voltage Domain | HW Interrupt Line | Primary Function |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **BNO085** | `U11` | **`0x4A`** *(alt `0x4B`)* | `+3V3` | `GPIO 2` (`!INT_DOF`, active LOW) | 9-DOF IMU, AR/VR fusion, 50 Hz compass, pedometer |
| **OPT3001** | `U10` | **`0x44`** / **`0x45`** | `+3V3` | `GPIO 39` (`!INT_ALS`, active LOW) | Precision photopic ambient light sensor (Lux) |
| **MAX17048** | `U6` | **`0x36`** | `+3V3` | `GPIO 4` (`!ALERT`, active LOW) | ModelGauge™ LiPo fuel gauge ($V_{\text{cell}}$, %, CRATE) |
| **RV-3028-C7** | `U12` | **`0x52`** | `+3V3` | `GPIO 6` (`!INT_RTC`, active LOW) | Extreme low-power RTC ($45\,\text{nA}$), hardware alarms |
| **MAX30102** | `U8` | **`0x57`** | `+1V8` *(via PCA9306 `U9`)* | `GPIO 5` (`!INT_HR`, active LOW) | Optical PPG biometric pulse & $SpO_2$ oximetry |
| **BME680/690** | `U7` | **`0x76`** *(alt `0x77`)* | `+3V3` | *Polled* | Temperature, Humidity, Barometer (hPa), MOX Gas ($R_{\text{gas}}$) |

---

## 6. Persistent Storage Subsystem (`StorageManager`)

Configuration parameters and user preferences persist on the onboard **ZDSD NAND Flash** (`CS_SD = GPIO 47`) in `/sys/config.bin`.

### Binary Struct Layout (`DeviceConfig`)
A cache-aligned **64-byte packed binary struct**:

```text
Offset  Field              Type      Size  Description
──────  ─────────────────  ────────  ────  ───────────────────────────────────────────
0x00    magic              uint16_t  2 B   Magic signature (0x584E = "NX" in ASCII)
0x02    version            uint8_t   1 B   Schema version (Current: 1)
0x03    size               uint8_t   1 B   Struct byte size (Current: 64)
0x04    crc16              uint16_t  2 B   CCITT CRC-16 (poly 0x1021, init 0xFFFF)
0x06    reservedHeader     uint16_t  2 B   Alignment padding
0x08    brightness         uint8_t   1 B   Display backlight brightness (10–100%)
0x09    autoDim            uint8_t   1 B   Ambient light compensation (0 = Off, 1 = On)
0x0A    tiltToWake         uint8_t   1 B   Tilt mode (0=Off, 1=Sens, 2=Bal, 3=Slug)
0x0B    screenTimeout      uint16_t  2 B   Timeout in seconds (0 = Never / Always On)
0x0D    wristCoverStandby  uint8_t   1 B   Cover gesture (0 = Off, 1 = On)
0x0E    fadeAnimMs         uint16_t  2 B   Backlight fade duration (0–2000 ms)
0x10    notifMethod        uint8_t   1 B   0=Silent, 1=All, 2=Sound, 3=Lights
0x11    matrixNotif        uint8_t   1 B   4x4 NeoPixel alert flash (0/1)
0x12    hrmReminder        uint8_t   1 B   0=Off, 1=30m, 2=1h, 3=2h
0x13    ecoMode            uint8_t   1 B   Power save throttle (0/1)
0x14    autoStandby        uint8_t   1 B   Auto screen sleep (0/1)
0x15    sensorSleep        uint8_t   1 B   Deep sleep sensors on standby (0/1)
0x16    buzzerEnabled      uint8_t   1 B   Master audio toggle (0/1)
0x17    buzzerVolume       uint8_t   1 B   Duty cycle intensity (10–100%)
0x18    buzzerTickDuration uint16_t  2 B   Pulse width (2–30 ms)
0x1A    buzzerTonePitch    uint16_t  2 B   Acoustic frequency (1600–4400 Hz)
0x1C    npmPower           uint8_t   1 B   NeoPixel PMOS gate (0/1)
0x1D    npmBrightness      uint8_t   1 B   Matrix brightness (10–100%)
0x1E    npmAnimation       uint8_t   1 B   Active pattern ID (0–5)
0x1F    aisEnabled         uint8_t   1 B   NX-AIS co-processor (0/1)
0x20    aisAdaptive        uint8_t   1 B   Adaptive context threshold (0/1)
0x21    aisDiagnostics     uint8_t   1 B   Diagnostics logging (0/1)
0x22    reservedBuffer     uint8_t   26 B  Zero-migration expansion buffer
0x3C    reservedBuffer     (cont.)   4 B   Pad to exactly 64 bytes total
```

### Zero-Wear Save Policy
1. **RAM-Only Dialing:** Turning radial dials modifies RAM variables and sets `isDirty = true`. Rotating a dial 10,000 times produces **0 disk writes**.
2. **Commit on `[PUSH]`:** Pressing the lever down to confirm a setting triggers an atomic write and resets `isDirty = false`.
3. **Commit on Exit:** Pressing `[BTN]` to back out of Settings flushes any dirty state once.
4. **Atomic Staging:** Data writes to `/sys/config.tmp` first. Once verified, it atomically renames to `/sys/config.bin`. If power cuts out mid-write, the existing `/sys/config.bin` remains uncorrupted.
5. **Fail-Safe Boot:** If the NAND storage is unformatted, corrupt, or CRC fails, `StorageManager` loads factory defaults immediately without crashing or blocking boot.

### Standalone PC Inspection Utility (`tools/nx_config_tool.py`)
A dedicated Python CLI tool allows inspecting, validating, editing, and compiling configuration binaries offline:

```bash
# Read and validate binary config:
python tools/nx_config_tool.py read config.bin

# Export binary configuration to human-readable JSON:
python tools/nx_config_tool.py to-json config.bin -o config.json

# Compile modified JSON back to binary with recalculated CRC-16:
python tools/nx_config_tool.py from-json config.json -o config.bin

# Generate fresh factory default binary config:
python tools/nx_config_tool.py create-default -o config.bin
```

---

## 7. Mathematical Models & Graphics Curves

### 1. Backlight Smoothstep Curve
Backlight fade transitions utilize a cubic Hermite **smoothstep curve** instead of a linear ramp:

$$f(t) = t^2 \times (3 - 2t) \quad \text{for } t \in [0, 1]$$

This produces zero first-derivative velocity at start and end ($f'(0) = f'(1) = 0$), mirroring natural human pupil dilation and eliminating harsh visual snapping.

### 2. 3D Cover Flow Trapezoidal Projection
Cards flanking the center hero card in the 3D App Menu are projected with geometric perspective scaling:

$$\text{scale} = \max\left(0.55, 1.0 - 0.45 \times \frac{|d|}{180}\right)$$

Flanking card vertical offsets use trapezoidal corner adjustments (`fillTriangle`) to produce true 3D spatial depth.

### 3. Spring-Damper Carousel Physics
The carousel scroll position snaps to the centered hero card using an underdamped spring-damper easing formula:

$$\text{pos}_{k+1} = \text{pos}_k + (\text{target} - \text{pos}_k) \times 0.48$$

This provides a fluid, mechanical feel that snaps securely into slot on lever release.

### 4. Real-Time 3D Vector Earth Engine
The Environment deck renders a live 3D rotating planetary sphere directly on Core 1 at 60 FPS without bitmap textures:
* **Planetary Projection:** Spherical latitude/longitude coordinates $(lat, lon)$ are rotated around Earth's $23.4^\circ$ tilted polar axis using Euler coordinate transformations:
  $$x_0 = \cos(lat) \sin(lon + \theta), \quad y_0 = \sin(lat), \quad z_0 = \cos(lat) \cos(lon + \theta)$$
  $$x = x_0 \cos(\alpha) - y_0 \sin(\alpha), \quad y = x_0 \sin(\alpha) + y_0 \cos(\alpha), \quad z = z_0$$
* **Hemispherical Depth Culling:** Only vertices and coastline edges with $z > 0$ are projected onto the ST7789 display, while backside polygons are culled.
* **Compact Footprint:** Continental coastlines are defined using compact `GeoNode` integer pairs ($< 1\,\text{kB}$ flash), executing in $< 30\,\mu\text{s}$ per frame on the ESP32-S3 hardware FPU.

---

## 8. Building, Flashing & Development Workflow

ISD-Core is compiled using **PlatformIO** targeting the ESP32-S3 with 16MB Flash and 8MB Octal PSRAM:

```bash
# 1. Clone repository
git clone https://github.com/Ghost3379/NX-ISD.git
cd NX-ISD/ISD-Core

# 2. Compile firmware
pio run

# 3. Upload to target board via USB CDC (COM9 / ttyACM0)
pio run --target upload

# 4. Open serial terminal monitor (115200 baud)
pio device monitor
```

### Compiler Configurations (`platformio.ini`)
* **Board:** `esp32-s3-devkitc-1`
* **PSRAM:** `board_build.arduino.memory_type = opi_opi`
* **Flash Mode:** `qio`, 80 MHz, 16 MB
* **Optimization:** `-O2`, `-DCORE_DEBUG_LEVEL=0`

---

## 9. License

The software in this directory is licensed under the **GNU General Public License v3.0 (GPLv3)**.  
The hardware design files in [`ISD-PCB`](../ISD-PCB) are licensed under the **CERN-OHL-S v2**.
