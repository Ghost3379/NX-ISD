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
  │                                       ├─► ZDSD02GLGEAG NAND Flash (U14)
  │                                       ├─► ST7789 Backlight PWM (via Q4)
  │                                       └─► Buzzer B1 (via Q3)
  │
  ├──► RT9193-18GB LDO (U1) ──────────► +1V8 (Ultra-Low-Noise Sensor Rail)
  │    (Ultra-Low Noise, 300mA)           └─► MAX30102 PPG / Biometrics (U8)
  │
  ├──► RT9193-28GB LDO (U4) ──────────► +2V8 (Display & Shifter Rail)
  │    (Low Noise, 300mA)                 ├─► ST7789 IPS Display Logic (J2)
  │                                       └─► TXB0106 Level Shifter (U15)
  │
  └──► AO3401A PMOS Gate (Q2) ────────► VN   (Switched VBAT Rail, 3.0V - 4.2V)
       (Driven by IO17 via BC847W Q1)     └─► 16x NeoPixel Matrix (G1..G16)
                                              [Guarantees 0µA standby draw]
```

### Voltage Rail Details

| Rail | Nominal Voltage | Regulator / Source | Max Current | Consumers |
| :--- | :--- | :--- | :--- | :--- |
| **`VBUS`** | 5.0 V (4.75–5.25V) | USB Type-C Receptacle (`J1`) | 1.5 A | BQ25170 Charger, USB detect divider |
| **`VBAT`** | 3.7 V (3.0–4.2V) | 1S LiPo via JST PH (`P1`) | 2.0 A peak | Buck converter, LDOs, Fuel Gauge, VN PMOS switch |
| **`+3V3`** | 3.3 V (±1.5%) | TLV62568DBV Synchronous Buck (`U3`)| 1.0 A | ESP32-S3, BNO085, BME690, OPT3001, RTC, NAND-SD, Backlight, Buzzer |
| **`+1V8`** | 1.8 V (±2.0%) | RT9193-18GB Low-Noise LDO (`U1`) | 300 mA | MAX30102 logic core, PCA9306 low-side |
| **`+2V8`** | 2.8 V (±2.0%) | RT9193-28GB Low-Noise LDO (`U4`) | 300 mA | ST7789 display VDD, TXB0106 B-port |
| **`VN`** | Switched `VBAT` | AO3401A High-Side PMOS (`Q2`) | 1.0 A | 16x XL-1010RGBC NeoPixels (0µA cutoff) |

---

## 3. Power Consumption Budget & Battery Calculation

Comprehensive current consumption analysis across operating states, accounting for conversion efficiencies and real-world loads:

### 1. Subsystem Current Breakdown

#### A. 16× NeoPixel RGB Matrix (`G1`–`G16`, on Switched `VN` / `VBAT`)
* **Driver Constant-Current Sink:** $5.0\,\text{mA}$ per color channel (R, G, B).
* **Per LED at 100% White:** $(3 \times 5.0\,\text{mA}) + 0.35\,\text{mA}$ (internal logic) $\approx \mathbf{15.35\,\text{mA}}$.
* **100% White Matrix Stress Test (16 LEDs):** $16 \times 15.35\,\text{mA} \approx \mathbf{245.6\,\text{mA}}$ (drawn directly from `VBAT` through PMOS `Q2`).
* **Dynamic Cyberpunk Animations (`CYBER RADAR`, `NEON TRACER`, etc.):** 2 to 5 active LEDs at 30–60% intensity $\approx \mathbf{15\text{ to }35\,\text{mA}}$ average.
* **Standby / Off State (`PWR_NPM` = LOW):** High-side PMOS `Q2` is gated off by pull-up $\implies \mathbf{0.00\,\mu\text{A}}$ (completely eliminates NeoPixel quiescent leakage).

#### B. ESP32-S3 Dual-Core Microcontroller (`U13`, on `+3V3`)
* **Active Wi-Fi TX (Max power + Dual-Core processing):** $\approx 240\text{ to }350\,\text{mA}$ (transient peaks up to $\approx 450\,\text{mA}$).
* **Active BLE Connected / Advertising:** $\approx 35\text{ to }55\,\text{mA}$.
* **Normal Dual-Core Processing (No RF, 240 MHz):** $\approx 40\text{ to }65\,\text{mA}$.
* **Frequency-Scaled Run (80 MHz / 160 MHz):** $\approx 20\text{ to }35\,\text{mA}$.
* **FreeRTOS Light-Sleep (Between 50Hz/1Hz sensor ticks):** $\approx 1.5\text{ to }3.0\,\text{mA}$.
* **Deep-Sleep (ULP / RTC wake only):** $\approx 15\text{ to }25\,\mu\text{A}$.

#### C. ST7789 IPS Display (`J2`, on `+3V3` & `+2V8`)
* **Backlight LEDs (via `+3V3` switched by N-MOSFET `Q4`):**
  * 100% Brightness: $\approx 35\text{ to }45\,\text{mA}$.
  * 40% Indoor Brightness: $\approx 15\text{ to }20\,\text{mA}$.
* **Display Controller Logic (via `+2V8` LDO `U4`):** $\approx 5\text{ to }10\,\text{mA}$.
* **Display Sleep Mode (`SLPIN` command):** $< 20\,\mu\text{A}$.
* **Total Display Active:** $\approx 20\text{ to }55\,\text{mA}$.

#### D. Sensors, Audio & Storage Peripherals
* **MAX30102 Biometrics (`U8`, on `+1V8` LDO `U1`):**
  * Time-averaged optical pulse current (Red + IR LEDs @ 50–100 Hz): $\approx 10\text{ to }18\,\text{mA}$ (peaks of $50\,\text{mA}$ during $400\,\mu\text{s}$ pulses).
  * Standby / Shutdown: $< 1\,\mu\text{A}$.
* **BNO085 9-DOF IMU (`U11`, on `+3V3`):**
  * Active 9-axis sensor fusion (internal ARM Cortex-M0+ running at 100 Hz): $\approx 15\text{ to }18\,\text{mA}$.
  * Low-power tap / step-detector only: $\approx 0.8\text{ to }1.5\,\text{mA}$.
* **BME690 Environmental (`U7`, on `+3V3`):**
  * Base climate sampling (T/P/H): $< 1\,\text{mA}$.
  * MOX Gas Heater Pulse (heats to $320^\circ\text{C}$ for $\approx 30\,\text{ms}$): $\mathbf{+12\text{ to }14\,\text{mA}}$ transient burst.
* **B1 Electromagnetic Buzzer (`B1`, on `+3V3` via `Q3`):**
  * $16\,\Omega$ coil impedance at $3.3\,\text{V}$: Peak current $\approx 206\,\text{mA}$.
  * 50% PWM Duty Cycle acoustic tone: $\approx \mathbf{80\text{ to }100\,\text{mA}}$ average (only when sounding alert/alarm).
* **ZDSD02GLGEAG SPI NAND Flash (`U14`, on `+3V3`):**
  * Standby: $\approx 10\,\mu\text{A}$.
  * High-speed SPI Read/Write bursts: $\approx 15\text{ to }25\,\text{mA}$.
* **OPT3001 Ambient Light (`U10`, on `+3V3`):** $\approx 2\text{ to }4\,\mu\text{A}$ (quiescent).
* **RV-3028-C7 RTC (`U12`, on `+3V3`):** $\approx 45\,\text{nA}$ (running continuously).
* **LDO Quiescent Currents (`U1`, `U4`):** $\approx 90\,\mu\text{A}$ each.

---

### 2. Supply Rail Power Budget & Buck Efficiency

#### `+3V3` System Rail (TLV62568 Synchronous Buck `U3`):
* **Normal Operation (Display on, sensors active, Wi-Fi off):** $\approx 80\text{ to }130\,\text{mA}$.
* **Full Load (Wi-Fi TX active, display 100%, all sensors active, BME690 heater):** $\approx \mathbf{320\text{ to }460\,\text{mA}}$.
* **Absolute Worst-Case Transient (+ Buzzer beeping):** $\approx 550\text{ to }640\,\text{mA}$.
* **Regulator Headroom:** TLV62568 is rated for **$1.0\,\text{A}$** continuous current $\implies$ **$> 35\%\text{--}50\%$ thermal and electrical safety margin** under simultaneous peak load.

#### Effective Battery Draw (from 1S LiPo, $V_{\text{BAT}} \approx 3.7\,\text{V}$):
The TLV62568 buck operates at $\approx 90\text{--}95\%$ efficiency ($\eta$):
$$P_{\text{in}} = \frac{V_{\text{out}} \times I_{\text{out}}}{\eta} \implies I_{\text{BAT,buck}} \approx \frac{3.3\,\text{V} \times I_{\text{3V3}}}{3.7\,\text{V} \times 0.92} \approx 0.97 \times I_{\text{3V3}}$$

* **Buck Current from Battery:** $\approx 310\text{ to }445\,\text{mA}$ (during Wi-Fi TX).
* **Matrix Current from Battery:** $\approx 245\,\text{mA}$ (100% white stress test).
* **LDOs from Battery (+1V8 and +2V8):** $\approx 15\text{ to }25\,\text{mA}$.
* **Total Battery Current at 100% Synthetic Stress Test:** $\mathbf{\approx 570\text{ to }715\,\text{mA}}$ (short peaks up to $\approx 780\,\text{mA}$ with buzzer).
* **Battery Health Assessment:** On a **1200 mAh 1S LiPo**, $715\,\text{mA}$ represents a discharge rate of only $\approx \mathbf{0.6\text{ C}}$ (standard 1S LiPo cells tolerate $1.0\text{ C}$ continuous / $2.0\text{ C}$ pulse), ensuring zero risk of excessive cell heating or premature degradation.

---

### 3. Realistic Operating Profiles & Expected Battery Life (1200 mAh LiPo)

| Profile | Description / Active Subsystems | Average Battery Current | Estimated Runtime |
| :--- | :--- | :--- | :--- |
| **Profile 1: Synthetic Stress Test** | 100% White Matrix, Wi-Fi TX active, Display 100%, all sensors | $\approx 650\,\text{mA}$ | **$\approx 1.8\text{ Hours}$** |
| **Profile 2: Continuous "Cyberpunk" Active** | Display 60% (~18mA), 4×4 Matrix dynamic animation (~25mA), BNO085 fusion, PPG pulse, BLE advertising | $\approx 95\text{ to }130\,\text{mA}$ | **$\approx 9\text{ to }12.5\text{ Hours}$** (Continuous screen-on) |
| **Profile 3: Daily Smartwatch Wear** | Display auto-off after 10s, Matrix off, Raise-to-wake active (BNO085), background pedometer & climate logging, periodic BLE sync | $\approx 15\text{ to }25\,\text{mA}$ | **$\approx 48\text{ to }80\text{ Hours}$** (2 to 3.5 Days) |
| **Profile 4: Low-Power Standby / Sleep** | Screen & Matrix off ($0\,\mu\text{A}$), ESP32 light-sleep, RTC active, low-power sensor interrupt monitoring | $\approx 2.0\text{ to }3.5\,\text{mA}$ | **$\approx 340\text{ to }600\text{ Hours}$** (14 to 25 Days) |
| **Profile 5: Shelf Storage (Deep-Sleep)** | Display unpowered, Matrix unpowered, ESP32 deep-sleep, RV-3028 RTC running | $< 35\,\mu\text{A}$ | **$> 3\text{ Years}$** |

---

## 4. Primary Component & IC Roster

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

## 5. Hardware Interfaces & Address Map

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

## 6. Complete GPIO Pinout Matrix

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

## 7. Manufacturing & Assembly Resources

* **Production Package:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3.zip`](NX-ISD/production/NX-ISD_v1p3.zip) (Contains RS-274X Gerbers and Excellon drill files ready for JLCPCB / PCBWay).
* **Interactive HTML BOM:** [`ISD-PCB/NX-ISD/bom/ibom.html`](NX-ISD/bom/ibom.html) (Searchable, interactive visual assembly tool showing component locations per reference designator).
* **Component Placement / CPL:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3_positions.csv`](NX-ISD/production/NX-ISD_v1p3_positions.csv)
* **Production BOM:** [`ISD-PCB/NX-ISD/production/NX-ISD_v1p3_bom.csv`](NX-ISD/production/NX-ISD_v1p3_bom.csv)
* **Schematic PDF:** [`docs/circuit diagrams/NX-ISD_v1p3.pdf`](../docs/circuit%20diagrams/NX-ISD_v1p3.pdf)

---

## 8. Hardware Licensing

The hardware design files in this directory are licensed under the **CERN Open Hardware Licence Version 2 - Strongly Reciprocal (CERN-OHL-S)**. See the root `LICENSE` file for full terms and conditions.
