# ISD-Core

ISD-Core is the embedded firmware for the NX-ISD intelligent sensor device. It is built with PlatformIO, the Arduino framework, and FreeRTOS on an ESP32-S3.

The current firmware provides a hardware bring-up and diagnostics console for the NX-ISD board. It initializes the connected sensors and peripherals, performs a boot self-check, continuously collects telemetry, and presents the results through the TFT display.

## Current Capabilities

- ESP32-S3 firmware running on the NX-ISD board
- TFT display initialization and diagnostics user interface
- Boot-time self-check for sensors, storage, and peripherals
- FreeRTOS task scheduling across both ESP32-S3 cores
- Thread-safe shared sensor telemetry
- Fast polling for motion and pulse data
- Slow polling for environmental and battery data
- Fuel-gauge readings including voltage, percentage, and charge rate
- NeoPixel matrix power control and animations
- RTC time and date support
- NAND-SD read/write test
- Buzzer feedback for startup, navigation, and successful checks

## Software Architecture

```text
+--------------------------------------------------------------------------+
|                              ISD-Core                                   |
|                                                                          |
|  +---------------------+        +-------------------------------+       |
|  | Boot and self-check |        | FreeRTOS runtime               |       |
|  |                     |        |                               |       |
|  | - I2C / SPI setup   |        | +---------------------------+ |       |
|  | - TFT startup       |        | | vUITask                   | |       |
|  | - sensor init       |        | | Core 1, about 50 Hz       | |       |
|  | - peripheral tests  |        | | menu, input, display       | |       |
|  +----------+----------+        | +-------------+-------------+ |       |
|             |                   |               |               |       |
|             |                   | +-------------v-------------+ |       |
|             |                   | | Shared SensorState         | |       |
|             |                   | | mutex protected            | |       |
|             |                   | +-------------^-------------+ |       |
|             |                   |               |               |       |
|             |                   | +-------------+-------------+ |       |
|             |                   | | vFastSensorTask           | |       |
|             |                   | | Core 0, about 50 Hz       | |       |
|             |                   | | BNO085 and MAX30102       | |       |
|             |                   | +---------------------------+ |       |
|             |                   |                               |       |
|             |                   | +---------------------------+ |       |
|             |                   | | vSlowSensorTask           | |       |
|             |                   | | Core 0, about 1 Hz        | |       |
|             |                   | | battery, light, climate  | |       |
|             |                   | +---------------------------+ |       |
|             |                   +-------------------------------+       |
|             |                                                          |
|             +--------------------------+-------------------------------+
|                                        |
|                                        v
|  +--------------------------------------------------------------------+
|  | Sensors and peripherals                                             |
|  | BNO085 | MAX30102 | BME690 | OPT3001 | MAX17048 | RV-3028 | SD | TFT |
|  | NeoPixel matrix | buzzer | buttons | lever switch                  |
|  +--------------------------------------------------------------------+
+--------------------------------------------------------------------------+
```

### Boot sequence

1. Start the serial monitor at 115200 baud.
2. Configure power-domain control, display control, buttons, and interrupts.
3. Start the I2C and SPI buses.
4. Initialize the TFT display and show the loading screen.
5. Initialize each sensor and peripheral and display `OK` or `FAIL`.
6. Initialize the buzzer and create the shared-state mutex.
7. Wait for the user to press the lever or main button.
8. Start the UI and sensor tasks.
9. Delete the default Arduino loop task to release its resources.

## FreeRTOS Tasks

### UI task

`vUITask()` runs on Core 1 and updates the diagnostic menu at approximately 50 Hz. It handles button input, menu navigation, screen rendering, and user-triggered peripheral actions.

### Fast sensor task

`vFastSensorTask()` runs on Core 0 and polls motion and pulse sensors at approximately 50 Hz:

- BNO085 rotation vector and linear acceleration
- MAX30102 red and infrared readings

The task writes its results to `SensorState` while holding the shared mutex.

### Slow sensor task

`vSlowSensorTask()` runs on Core 0 and polls lower-rate measurements approximately once per second:

- MAX17048 battery voltage, percentage, and charge rate
- OPT3001 ambient light
- BME690 temperature, humidity, pressure, and gas resistance

## Diagnostic Menu

The display menu currently provides these diagnostic pages:

1. I2C bus scanner
2. Display and LEDs
3. Real-time clock
4. Fuel gauge and charging
5. Environment and ambient light
6. IMU 9-DOF motion
7. Pulse biometrics
8. NAND-SD storage and buzzer

The lever navigates the menu. Pressing the lever selects an item. The main button returns to the menu from a diagnostic page.

## Hardware Interfaces

| Device | Function | Interface / Address |
| --- | --- | --- |
| BNO085 | Motion and orientation | I2C, `0x4A` |
| MAX30102 | Pulse and optical data | I2C, `0x57` |
| BME690 | Temperature, humidity, pressure, gas | I2C, `0x76` |
| OPT3001 | Ambient light | I2C, `0x45` |
| MAX17048 | Battery fuel gauge | I2C, `0x36` |
| RV-3028-C7 | Real-time clock | I2C, `0x52` |
| ST7789 display | User interface | SPI, `CS_TFT` |
| NAND-SD storage | Storage test | SPI, `CS_SD` |
| WS2812B matrix | 16-pixel output | Single-wire, `IO18` |
| Buzzer | Audio feedback | GPIO, `IO10` |

The exact GPIO assignments are centralized in `src/pins.h`.

## Project Structure

```text
ISD-Core/
|-- platformio.ini        PlatformIO environment and dependencies
|-- src/
|   |-- main.cpp          Boot sequence and FreeRTOS tasks
|   |-- pins.h            Board GPIO definitions
|   |-- SensorState.h     Shared telemetry structure
|   |-- DiagnosticMenu.h  Display menu and input handling
|   |-- TestSensors.h     Sensor wrappers and read operations
|   |-- TestPeripherals.h RTC, storage, NeoPixel, and buzzer support
|-- include/              Project include directory
|-- lib/                  Project-local libraries
|-- test/                 Test directory
```

## Building and Uploading

Install PlatformIO in VS Code, open the repository root, and select the `4d_systems_esp32s3_gen4_r8n16` environment.

Typical PlatformIO commands:

```text
pio run
pio run --target upload
pio device monitor
```

The serial monitor uses:

```text
115200 baud
```

The board configuration, display build flags, and library dependencies are defined in `platformio.ini`.

## Sensor Data and Calibration

`SensorState` is the central telemetry structure shared between acquisition tasks and the diagnostic UI. It currently stores raw or directly converted measurements, including:

- Battery voltage, percentage, and charge rate
- Ambient light in lux
- Temperature, humidity, pressure, and gas resistance
- IMU roll, pitch, yaw, and linear acceleration
- MAX30102 red and infrared samples

The current firmware is focused on acquisition and diagnostics. Sensor calibration, drift tracking, filtering, environmental compensation, and higher-level sensor fusion are part of the planned software evolution. Measurements should therefore be treated as device telemetry rather than final medical or scientific values until those processing layers are implemented and validated.

## Development Direction

Planned software work includes:

- Local and intelligent processing of sensor data
- Sensor-specific calibration and drift compensation
- Filtering and quality scoring for biometric measurements
- Sensor fusion for motion and contextual data
- More power-aware sampling and peripheral control
- Persistent data logging and analysis
- Expanded automated tests for hardware interfaces and data processing

## License

The ISD-Core firmware is licensed under the GNU General Public License v3.0. See the repository `LICENSE` file for the full license text.
