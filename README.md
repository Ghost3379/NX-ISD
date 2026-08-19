# NX-ISD

## Overview
The **NX-ISD** is a custom, highly integrated wearable and embedded development board inspired by the Artemis Watch[cite: 3]. It serves as the hardware foundation for the **ISD-Core** operating system[cite: 3].

## Nameing & Structure

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


## Hardware Specifications

### Core & Storage
* **Microcontroller:** ESP32-S3 N16R8[cite: 3]
* **Storage:** Additional Onboard NAND-SD (ZDSD32GLGEAG)[cite: 3]

### Sensors
* **Motion/IMU:** BNO085 (9-DOF Motion Co-Processor)[cite: 3]
* **Environment:** BME280 (Temperature, Humidity, Pressure)[cite: 3]
* **Ambient Light:** OPT3001[cite: 3]
* **Biometrics:** MAX30102 (Heart Rate / SpO2)[cite: 3]
* **Timing:** RV-3028-C7 Real-Time Clock (RTC)[cite: 3]

### Power Management
* **Charging:** USB-C (BQ25170 LiPo Charger)[cite: 3]
* **Monitoring:** MAX17048 Fuel Gauge[cite: 3]
* **Voltage:** Dedicated LDOs (RT9193-18GB/28GB & NCP167) for clean 1.8V, 2.8V, and 3.3V rails[cite: 3]

### Peripherals & I/O
* **Display:** 10-Pin 0.5mm FPC connector for SPI displays[cite: 3]
* **LEDs:** 9x WS2812B NeoPixel Matrix (XL-1010RGBC)[cite: 3]
* **Audio:** Onboard SMD Buzzer[cite: 3]
* **Input:** Multi-Directional Lever-Switch and Push-Button[cite: 3]

## Software
The board is programmed via **PlatformIO** (Arduino Framework) and is optimized to run the custom **ISD-Core** firmware[cite: 3].

## License
This project is licensed under the MIT License[cite: 3]. See the LICENSE file for details[cite: 3].
