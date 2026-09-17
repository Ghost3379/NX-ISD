# NX-ISD

## Overview
The **NX-ISD** is a custom, highly integrated wearable and embedded development board inspired by the Artemis Watch. It serves as the hardware foundation for the **ISD-Core** operating system.

<p align="center">
  <img src="docs/images/nx_isd_v1p3_top.png" alt="NX-ISD v1p3 Top Component View" width="49%">
  <img src="docs/images/nx_isd_v1p3_bottom.png" alt="NX-ISD v1p3 Bottom Silkscreen View" width="49%">
</p>
<p align="center">
  <em>NX-ISD Hardware Revision v1p3 (KiCad Raytraced 3D Render) — No Battery & No Display mounted</em>
</p>

## Naming & Structure

```text
N X - I S D
│     │
│     └─► Intelligent Sensor Device
│         (The hardware foundation)
│
└─► Prototyping Series
    (Always in development, not a closed product, open for everyone)

      │
      ▼
  ISD-Core
      │
      ├─► Custom Operating System (Dual-Core FreeRTOS based)
      ├─► Built on PlatformIO / Arduino Framework
      │
      ├──► NX-MSF: Mathematical Sensor Fusion (Physics Models & Kalman Filters)
      ├──► NX-AIS: Advanced Information System (Contextual Advisory Engine)
      └──► NX-SDS: Self-Diagnostic System (Hardware Integrity & Proof-Testing)
```


## Hardware Specifications

### Core & Storage
* **Microcontroller:** ESP32-S3 N16R8
* **Storage:** Additional Onboard NAND-SD (ZDSD02GLGEAG -> 2 Gbit = 250 MByte)

### Sensors
* **Motion/IMU:** BNO085 (9-DOF Motion Co-Processor)
* **Environment:** BME690 (Temperature, Humidity, Pressure, Gas)
* **Ambient Light:** OPT3001
* **Biometrics:** MAX30102 (Heart Rate / SpO2)
* **Timing:** RV-3028-C7 Real-Time Clock (RTC)

### Power Management
* **Charging:** USB-C (BQ25170 LiPo Charger, 1200mAh form factor)
* **Monitoring:** MAX17048 Fuel Gauge
* **Voltage Regulation:** Synchronous step-down buck converter (TLV62568, ~95% efficiency) for 3.3V system rail; dedicated low-noise LDOs (RT9193-18GB/28GB) for 1.8V and 2.8V sensor rails

### Peripherals & I/O
* **Display:** 10-Pin 0.5mm FPC connector for SPI displays
* **LEDs:** 16x WS2812B NeoPixel 4×4 Serpentine Matrix (XL-1010RGBC) with PMOS high-side power cutoff
* **Audio:** Onboard SMD Buzzer
* **Input:** Multi-Directional Lever-Switch and Push-Button

## Software & Core Systems
The board is programmed via **PlatformIO** (Arduino Framework) and is optimized to run the custom **ISD-Core** operating system. The software stack is composed of three core modular systems:

* **[ISD-Core](ISD-Core/):** The core dual-core FreeRTOS firmware layer handling task scheduling (Core 1 UI @ 50Hz, Core 0 Sensors @ 50Hz/1Hz), thread-safe `SensorState` telemetry, and peripheral drivers. See [`ISD-Core/README.md`](ISD-Core/README.md) for the software architecture diagram and build guide.
* **NX-MSF (Mathematical Sensor Fusion):** The analytical physics and math engine executing 1D Kalman filter variometer tracking, Magnus-Tetens dew point calculation, barometric storm gradients, RMSSD heart rate variability, and software energy accounting.
* **NX-AIS (Advanced Information System):** The autonomous on-device contextual advisor translating raw multi-sensor telemetry into proactive health, environmental, and tactical notifications.
* **NX-SDS (Self-Diagnostic System):** The internal hardware integrity and health assurance engine providing continuous I2C bus recovery, on-demand in-situ proof-testing, and predictive battery/sensor aging analytics.

## Features & Mathematical Modeling Roadmap
See [FEATURE_LIST.md](FEATURE_LIST.md) for the comprehensive technical specifications, mathematical formulas, and implementation roadmap for **NX-MSF**, **NX-AIS**, and **NX-SDS**.

## License
The software/firmware in the [ISD-Core] directory is licensed under the GNU General Public License v3.0 (GPLv3).

The hardware design files in the [ISD-PCB] directory are licensed under the CERN Open Hardware Licence Version 2 - Strongly Reciprocal (CERN-OHL-S).
