# NX-ISD Hardware Specification (Revision v1p3)

This directory contains the schematic design, PCB layout, fabrication outputs, and Bill of Materials (BOM) for the **NX-ISD** (Intelligent Sensor Device) wearable carrier board.

<p align="center">
  <img src="../docs/images/nx_isd_v1p3_top.png" alt="NX-ISD v1p3 Top View" width="49%">
  <img src="../docs/images/nx_isd_v1p3_bottom.png" alt="NX-ISD v1p3 Bottom View" width="49%">
</p>
<p align="center">
  <em>NX-ISD Hardware Revision v1p3 (KiCad Raytraced 3D Render)</em>
</p>

---

## 1. Board Specifications & Physical Dimensions

| Parameter | Specification | Notes |
| :--- | :--- | :--- |
| **Dimensions** | **77.00 mm × 48.60 mm** | Measured outer bounding box on `Edge.Cuts` |
| **Board Thickness** | **1.2 mm** | Standard FR-4 rigid substrate |
| **Layer Count** | **4 Layers** | Optimized signal integrity and solid reference planes |
| **Copper Weight** | 1 oz Outer (35 µm), 1 oz Inner | Standard manufacturing yield |
| **Surface Finish** | ENIG (Electroless Nickel Immersion Gold) | Recommended for fine-pitch BGA/QFN sensors |
| **Strap Mounts** | Dual integrated strap slots | Accommodates standard 20 mm – 22 mm watch straps |
| **Antenna Clearance** | Dedicated bottom keep-out zone | `RF-AREA KEEP FREE` silkscreen below ESP32-S3 trace |
| **Primary Connector** | 16-Pin Mid-Mount USB Type-C | USB 2.0 Full Speed + 5V 600mA charging |
| **Battery Header** | JST PH 2.0mm 2-Pin (`P1`) | Standard 1S LiPo (1200 mAh form factor) |

### 4-Layer PCB Stackup

```text
┌────────────────────────────────────────────────────────────────────────┐
│ Layer 1: Top (F.Cu)       High-density signal routing, IC pads, GND    │
├────────────────────────────────────────────────────────────────────────┤
│ Layer 2: Inner 1 (In1.Cu) Solid uninterrupted Ground Plane (GND)       │
├────────────────────────────────────────────────────────────────────────┤
│ Layer 3: Inner 2 (In2.Cu) Power Planes (+3V3, VBAT, +1V8, +2V8, VN)    │
├────────────────────────────────────────────────────────────────────────┤
│ Layer 4: Bottom (B.Cu)    Secondary routing, test pads, silkscreen art │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Power Architecture & Voltage Domains

```text
               ┌───────────────────────┐
  USB-C (5V) ─►│ BQ25170 Charger (U5)  ├──► VBAT (3.7V - 4.2V, 1S LiPo)
               │ (600mA, 10k NTC TH1)  │         │
               └───────────────────────┘         ├─► MAX17048 Fuel Gauge (U6)
                                                 │
  ┌──────────────────────────────────────────────┘
  │
  ├──► TLV62568 Buck Converter (U3) ──► +3V3 (System Rail, ~95% Efficiency)
  │    (2.2µH L1, 10µF filtering)         │
  │                                       ├─► ESP32-S3 Microcontroller (U13)
  │                                       ├─► BNO085 9-DOF IMU (U11)
  │                                       ├─► BME690 Environmental (U7)
  │                                       ├─► OPT3001 Ambient Light (U10)
  │                                       ├─► RV-3028-C7 RTC (U12)
  │                                       └─► ZDSD02GLGEAG NAND Flash (U14)
  │
  ├──► RT9193-18GB LDO (U1) ──────────► +1V8 (Ultra-Low-Noise Sensor Rail)
  │    (Ultra-Low Noise, 300mA)           └─► MAX30102 PPG / Biometrics (U8)
  │
  ├──► RT9193-28GB LDO (U4) ──────────► +2V8 (Display & Shifter Rail)
  │    (Low Noise, 300mA)                 ├─► ST7789 IPS Display Logic (J2)
  │                                       └─► TXB0106 Level Shifter (U15)
  │
  └──► AO3401A PMOS Gate (Q2) ────────► VN   (Switched Matrix Power)
       (Driven by IO17 via BC847W Q1)     └─► 16x NeoPixel Matrix (G1..G16)
                                              [Guarantees 0µA standby draw]
```

### Voltage Rail Details

| Rail | Nominal Voltage | Regulator / Source | Max Current | Consumers |
| :--- | :--- | :--- | :--- | :--- |
| **`VBUS`** | 5.0 V (4.75–5.25V) | USB Type-C Receptacle (`J1`) | 1.5 A | BQ25170 Charger, USB detect divider |
| **`VBAT`** | 3.7 V (3.0–4.2V) | 1S LiPo via JST PH (`P1`) | 2.0 A peak | Buck converter, LDOs, Fuel Gauge |
| **`+3V3`** | 3.3 V (±1.5%) | TLV62568DBV Synchronous Buck (`U3`)| 1.0 A | ESP32-S3, BNO085, BME690, OPT3001, RTC, NAND-SD |
| **`+1V8`** | 1.8 V (±2.0%) | RT9193-18GB Low-Noise LDO (`U1`) | 300 mA | MAX30102 logic core, PCA9306 low-side |
| **`+2V8`** | 2.8 V (±2.0%) | RT9193-28GB Low-Noise LDO (`U4`) | 300 mA | ST7789 display VDD, TXB0106 B-port |
| **`VN`** | Switched `+3V3` | AO3401A High-Side PMOS (`Q2`) | 1.0 A | 16x XL-1010RGBC NeoPixels (0µA cutoff) |

---

## 3. Primary Component & IC Roster

| Designator | Component | Package | Function | LCSC Part # |
| :--- | :--- | :--- | :--- | :--- |
| **U13** | **ESP32-S3-WROOM-1-N16R8** | Module (18×25.5 mm) | Dual-Core 240MHz MCU, 16MB Flash, 8MB PSRAM | Direct / Espressif |
| **U3** | **TLV62568DBV** | SOT-23-5 | Synchronous Step-Down DC-DC Buck Converter (+3V3) | C967888 |
| **U5** | **BQ25170DSGR** | WSON-8 (2×2 mm) | 800mA Linear LiPo Charger with 10k NTC sensing | C2837330 |
| **U6** | **MAX17048G_T10** | TDFN-8 (2×2 mm) | ModelGauge™ LiPo Fuel Gauge with ALERT output | C2682619 |
| **U1** | **RT9193-18GB** | SOT-23-5 | 300mA Ultra-Low-Noise LDO (+1.8V Sensor Rail) | C27416 |
| **U4** | **RT9193-28GB** | SOT-23-5 | 300mA Low-Noise LDO (+2.8V Display Rail) | C56731 |
| **U11** | **BNO085** | LGA-28 (5.2×3.8 mm) | 9-DOF Motion Co-Processor (AR/VR IMU fusion) | Direct / CEVA |
| **U7** | **BME690** | LGA-8 (3×3 mm) | Temperature, Relative Humidity, Pressure, Gas MOX | Direct / Bosch |
| **U8** | **MAX30102** | OLGA-14 (3.3×5.6 mm) | Pulse Oximetry ($SpO_2$) & Heart Rate Optical Sensor | C83038 |
| **U10** | **OPT3001DNPR** | USON-6 (2×2 mm) | Precision Ambient Light Sensor (Photopic curve) | C113886 |
| **U12** | **RV-3028-C7** | SON-8 (1.5×3.2 mm) | Real-Time Clock, Ultra-Low Power, Factory Calibrated | C2838382 |
| **U14** | **ZDSD02GLGEAG** | LGA-8 (8×6 mm) | 2 Gbit (250 MByte) High-Speed SPI NAND Flash | C2875853 |
| **U9** | **PCA9306DCKR** | VSSOP-8 | Dual Bidirectional I2C Voltage-Level Translator | C7570 |
| **U15** | **TXB0106RGYR** | VQFN-16 (2.5×2.0 mm)| 6-Bit Bidirectional Voltage-Level Shifter for Display| C7213 |
| **U2** | **USBLC6-2P6** | SOT-666 | Ultra-Low Capacitance Rail-to-Rail ESD Protection | C7519 |
| **G1..G16**| **XL-1010RGBC-WS2812B** | 1010 SMD (1×1 mm) | 16x NeoPixel Serpentine 4×4 RGB Matrix | C5349953 |
| **U16** | **TM-2025A** | SMD Multi-Way | Multi-Directional Navigation Lever-Switch | C318949 |
| **S1** | **TS-1101VS-C-A-A** | SMD Button | Tactile Push Button (Main user interaction) | C318884 |
| **B1** | **SMD-8503-3627-16Ω** | SMD (8.5×8.5 mm) | Electromagnetic Buzzer for acoustic notifications | C2685545 |
| **J1** | **UJ20-C-H-G-SMT-1-P16**| 16-Pin Mid-Mount | USB Type-C Receptacle with CC1/CC2 5.1k resistors | C2765186 |
| **J2** | **TE 1-2328702-0** | 10-Pin 0.5mm FPC | Dual-contact back-flip FPC connector for ST7789 | C2837335 |
| **P1** | **JST S2B-PH-SM4-TB** | 2-Pin SMT PH 2.0mm | Polarized LiPo battery connection header | C2875850 |

---

## 4. Hardware Interfaces & Address Map

### I2C Bus (`Wire`, 400 kHz Fast Mode)
* **SDA:** `GPIO 8` (10k pull-up to `+3V3`)
* **SCL:** `GPIO 9` (10k pull-up to `+3V3`)

| Device | Designator | 7-Bit Address | Voltage Domain | Hardware Interrupt Line |
| :--- | :--- | :--- | :--- | :--- |
| **BNO085** | `U11` | `0x4A` | `+3V3` | `GPIO 2` (`!INT_DOF`, active LOW) |
| **MAX30102** | `U8` | `0x57` | `+1V8` (via PCA9306 `U9`) | `GPIO 5` (`!INT_HR`, active LOW) |
| **BME690** | `U7` | `0x76` | `+3V3` | Polled |
| **OPT3001** | `U10` | `0x45` | `+3V3` | `GPIO 39` (`!INT_ALS`, active LOW) |
| **MAX17048** | `U6` | `0x36` | `+3V3` | `GPIO 4` (`!ALERT`, active LOW) |
| **RV-3028-C7**| `U12` | `0x52` | `+3V3` | `GPIO 6` (`!INT_RTC`, active LOW) |

### High-Speed SPI Bus (HSPI, up to 40 MHz)
* **SCK:** `GPIO 40`
* **MOSI:** `GPIO 42`
* **MISO:** `GPIO 41`

| Peripheral | Designator | Chip Select (CS) | Additional Control Lines | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **ST7789 TFT** | `J2` | `GPIO 48` (`CS_TFT`) | `GPIO 21` (`DC`/`RS`)<br>`GPIO 38` (`RST`)<br>`GPIO 1` (`PWM_TFT`) | 240×240 IPS Panel, PWM backlight via Q4 |
| **NAND-SD Flash** | `U14` | `GPIO 47` (`CS_SD`) | Shared SCK/MOSI/MISO | 2 Gbit (250 MB) storage for logs/certs |

---

## 5. Complete GPIO Pinout Matrix

Centralized in [`ISD-Core/src/pins.h`](../ISD-Core/src/pins.h):

| GPIO Pin | Pin Name | Peripheral / Signal | Direction | Electrical Notes |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO 1** | `PWM_TFT` | Display Backlight PWM | Output | Drives gate of N-MOSFET `Q4` (active HIGH) |
| **GPIO 2** | `INT_DOF` | BNO085 Motion Interrupt | Input | Active LOW interrupt from IMU |
| **GPIO 4** | `ALERT` | Fuel Gauge Alert | Input | Active LOW interrupt (internal pull-up enabled) |
| **GPIO 5** | `INT_HR` | MAX30102 PPG Interrupt | Input | Active LOW biometric sample-ready interrupt |
| **GPIO 6** | `INT_RTC` | RV-3028 RTC Alarm/Timer | Input | Active LOW periodic/alarm interrupt |
| **GPIO 8** | `I2C_SDA` | I2C Data Line | Bi-directional | 10k hardware pull-up to `+3V3` (`R18`) |
| **GPIO 9** | `I2C_SCL` | I2C Clock Line | Output | 10k hardware pull-up to `+3V3` (`R19`) |
| **GPIO 10** | `BUZZER` | Electromagnetic Buzzer | Output | Drives gate of N-MOSFET `Q3` (PWM tone output) |
| **GPIO 11** | `BAT_STAT` | BQ25170 Charger Status | Input | LOW = Charging, HIGH/Hi-Z = Charge Complete |
| **GPIO 12** | `USB_DETECT`| USB 5V VBUS Presence | Input | Resistor divider from VBUS (HIGH when plugged in) |
| **GPIO 13** | `BTN` | Main Tactile Push-Button | Input | Active LOW (external pull-up, debounced) |
| **GPIO 14** | `LEVER_LEFT`| Navigation Lever Left | Input | Active LOW (internal pull-up enabled) |
| **GPIO 15** | `LEVER_PUSH`| Navigation Lever Center | Input | Active LOW (internal pull-up enabled) |
| **GPIO 16** | `LEVER_RIGHT`| Navigation Lever Right | Input | Active LOW (internal pull-up enabled) |
| **GPIO 17** | `PWR_NPM` | NeoPixel Matrix Power Gate | Output | HIGH = Turns ON PMOS `Q2`; LOW = 0µA cutoff |
| **GPIO 18** | `NPM` | NeoPixel Serial Data | Output | 800 kHz single-wire NZR stream to `G1` |
| **GPIO 21** | `TFT_RS` | Display Command / Data | Output | LOW = Command, HIGH = Data (via TXB0106) |
| **GPIO 38** | `TFT_RST` | Display Hardware Reset | Output | Active LOW hardware reset line |
| **GPIO 39** | `INT_ALS` | OPT3001 Light Interrupt | Input | Active LOW optical threshold interrupt |
| **GPIO 40** | `SPI_SCK` | SPI Bus Serial Clock | Output | Up to 40 MHz clock |
| **GPIO 41** | `SPI_MISO`| SPI Bus Data In (MISO) | Input | High-speed data input from NAND flash |
| **GPIO 42** | `SPI_MOSI`| SPI Bus Data Out (MOSI) | Output | High-speed data output to Display & NAND |
| **GPIO 47** | `CS_SD` | NAND Flash Chip Select | Output | Active LOW chip select |
| **GPIO 48** | `CS_TFT` | TFT Display Chip Select | Output | Active LOW chip select |

---

## 6. Manufacturing & Assembly Resources

* **Production Package:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3.zip`](NX-ISD/production/NX-ISD_v1p3.zip) (Contains RS-274X Gerbers and Excellon drill files ready for JLCPCB / PCBWay).
* **Interactive HTML BOM:** [`ISD-PCB/NX-ISD/bom/ibom.html`](NX-ISD/bom/ibom.html) (Searchable, interactive visual assembly tool showing component locations per reference designator).
* **Component Placement / CPL:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3_positions.csv`](NX-ISD/production/NX-ISD_v1p3_positions.csv)
* **Production BOM:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3_bom.csv`](NX-ISD/production/NX-ISD_v1p3_bom.csv)
* **Schematic PDF:** [`docs/circuit diagrams/NX-ISD_v1p3.pdf`](../docs/circuit%20diagrams/NX-ISD_v1p3.pdf)

---

## 7. Hardware Licensing

The hardware design files in this directory are licensed under the **CERN Open Hardware Licence Version 2 - Strongly Reciprocal (CERN-OHL-S)**. See the root `LICENSE` file for full terms and conditions.
