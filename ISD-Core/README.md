# ISD-Core Firmware

Firmware for the ISD (Integrated Sensor Device) built on the ESP32-S3 (WROOM-1-N16R8, 16MB Flash, 8MB Octal PSRAM).

ISD-Core delivers a high-performance, dual-core wearable operating environment featuring a real-time cyberpunk telemetry watchface, a 3D perspective-projected cover flow application deck, and an interrupt-driven control subsystem.

---

## Software Architecture

The firmware utilizes the ESP32-S3 dual-core asymmetric processing model under FreeRTOS to eliminate frame drops and ensure zero input latency:

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

### 1. Dual-Core Task Allocation

- **Core 1 (APP CPU) - UI & Graphics Subsystem (`loop()`):**
  - Executes non-blocking rendering at ~42 FPS.
  - Zero blocking delays: replaced conventional busy loops with `delay(1)` yields to prevent FreeRTOS watchdog starvation while guaranteeing immediate input evaluation.
  - Double-buffered PSRAM rendering via `TFT_eSprite` pushed over SPI at 40 MHz.
  - Manages UI state machines:
    - **Watchface HUD (`Watchface.h`):** Real-time digital clock, orbital seconds indicator, battery arc gauge, sensor telemetry cards, dynamic compass with shortest-path circular interpolation, and low-power standby mode.
    - **3D App Menu (`AppMenu.h`):** 3D perspective-projected card carousel with dynamic trapezoidal distortion, depth scaling, active card highlighting, and responsive spring physics (`diff * 0.48f`).

- **Core 0 (PRO CPU) - Sensor Telemetry Subsystem (`vSensorTask`):**
  - Dedicated to sensor polling on the Fast-Mode 400 kHz I2C bus (`Wire`).
  - **High-Rate Motion:** Polls BNO085 9-DOF IMU rotation vectors (`SH2_ROTATION_VECTOR`) at 50-100 Hz.
  - **Slow Telemetry:** Non-blocking 1 Hz polling for MAX17048 fuel gauge, RV-3028 RTC, BME680 environmental data, and OPT3001 ambient light.
  - **Sub-Microsecond Mutex Locking:** All slow I2C bus transactions take place in local stack memory *outside* the mutex. `stateMutex` is acquired exclusively for sub-microsecond memory copies into `sharedState`.

- **State Bridge (`SensorState.h`):**
  - Shared thread-safe telemetry container protected by a FreeRTOS binary mutex (`stateMutex`).
  - Core 1 maintains a static persistent local copy (`static SensorState localState`), completely preventing transient data dropouts (such as clock `--:--:--` or compass center snaps) during mutex contention.

---

### 2. Input & Control Subsystem (Interrupt-Driven)

User controls utilize dedicated **hardware interrupts** (`attachInterrupt`) with IRAM-resident ISR handlers triggered on `FALLING` edges:

| Input Pin | Physical Control | Function / Gesture |
| :--- | :--- | :--- |
| **GPIO 13** | Main Pushbutton (`BTN`) | Return to previous screen / Exit menu / Screen wake |
| **GPIO 16** | Navigation Lever Left (`LEVER_LEFT`) | Previous card / Value decrement / Hold-to-repeat |
| **GPIO 15** | Navigation Lever Push (`LEVER_PUSH`) | Select / Confirm / 1.2s Hold Charge Gesture into 3D Menu |
| **GPIO 14** | Navigation Lever Right (`LEVER_RIGHT`) | Next card / Value increment / Hold-to-repeat |

- **Zero-Latency Latching & Bounce Immunity:** Momentary lever flicks (15-25 ms) are latched in hardware within $<1\mu\text{s}$. Physical active-state tracking rejects release chatter, ensuring exactly one transition per click.
- **Hardware Timer Debounce:** ISRs enforce a 40 ms hardware debounce window using `esp_timer_get_time()`, unlatching only after stable physical release.
- **Hold-to-Repeat:** Continuous holds trigger an immediate click, followed by a 600 ms hold delay, then repeat smoothly every 160 ms.

---

## Hardware Interfaces

| Device | Interface | Bus Frequency | Description |
| :--- | :--- | :--- | :--- |
| **ST7789 IPS Display** | SPI (VSPI) | 40 MHz | 240x240 RGB display, double-buffered in Octal PSRAM |
| **BNO085** | I2C (`0x4A`) | 400 kHz | 9-DOF IMU (Rotation vector, compass heading) |
| **RV-3028-C7** | I2C (`0x52`) | 400 kHz | Ultra-low power real-time clock (RTC) |
| **MAX17048** | I2C (`0x36`) | 400 kHz | LiPo fuel gauge (voltage, percentage, charge rate) |
| **BME680** | I2C (`0x76`) | 400 kHz | Environmental sensor (temp, humidity, pressure, gas) |
| **OPT3001** | I2C (`0x45`) | 400 kHz | Precision ambient light sensor (lux) |
| **BQ25170** | GPIO | - | Standalone linear charger (`/PG` USB detect, `/STAT` charging) |
| **Piezo Buzzer** | GPIO 10 | PWM | Acoustic feedback, startup chime, UI navigation ticks |
| **WS2812B NeoPixels**| GPIO 18 | - | 4x4 matrix auxiliary display |

Pin definitions are centralized in `src/pins.h`.

---

## Project Structure

```text
ISD-Core/
|-- platformio.ini        PlatformIO project configuration & ESP32-S3 build flags
|-- README.md             System architecture & technical documentation
|-- src/
    |-- main.cpp          Core initialization, ISR handlers, and FreeRTOS task loops
    |-- pins.h            Hardware GPIO mapping and pin definitions
    |-- DisplayConfig.h   SPI bus clock (40 MHz) and ST7789 display parameters
    |-- HAL.h / HAL.cpp   Hardware Abstraction Layer (pins, power, buzzer chimes)
    |-- SensorState.h     Mutex-protected cross-core telemetry data structure
    |-- Watchface.h       Cyberpunk watchface UI, dynamic compass & HUD rendering
    |-- AppMenu.h         3D perspective cover flow application menu
```

---

## Building & Flashing

ISD-Core is built using PlatformIO targeting the ESP32-S3 with 16MB Flash and 8MB Octal PSRAM (`OPI_OPI` mode).

```bash
# Build firmware
pio run

# Flash to target board via USB CDC
pio run --target upload

# Open serial telemetry monitor (115200 baud)
pio device monitor
```

---

## License

ISD-Core is licensed under the GNU General Public License v3.0. See the `LICENSE` file for details.
