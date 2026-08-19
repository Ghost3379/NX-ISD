# NX-ISD

## Overview
The **NX-ISD** is a custom, highly integrated wearable and embedded development board inspired by the Artemis Watch. It serves as the hardware foundation for the **ISD-Core** operating system.

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
      ├─► Custom Operating System
      └─► Built on PlatformIO / Arduino Framework
```


## Hardware Specifications

### Core & Storage
* **Microcontroller:** ESP32-S3 N16R8
* **Storage:** Additional Onboard NAND-SD (ZDSD32GLGEAG)

### Sensors
* **Motion/IMU:** BNO085 (9-DOF Motion Co-Processor)
* **Environment:** BME280 (Temperature, Humidity, Pressure)
* **Ambient Light:** OPT3001
* **Biometrics:** MAX30102 (Heart Rate / SpO2)
* **Timing:** RV-3028-C7 Real-Time Clock (RTC)

### Power Management
* **Charging:** USB-C (BQ25170 LiPo Charger)
* **Monitoring:** MAX17048 Fuel Gauge
* **Voltage:** Dedicated LDOs (RT9193-18GB/28GB & NCP167) for clean 1.8V, 2.8V, and 3.3V rails

### Peripherals & I/O
* **Display:** 10-Pin 0.5mm FPC connector for SPI displays
* **LEDs:** 9x WS2812B NeoPixel Matrix (XL-1010RGBC)
* **Audio:** Onboard SMD Buzzer
* **Input:** Multi-Directional Lever-Switch and Push-Button

## Software
The board is programmed via **PlatformIO** (Arduino Framework) and is optimized to run the custom **ISD-Core** firmware.

## License
This project is licensed under the MIT License. See the LICENSE file for details.
