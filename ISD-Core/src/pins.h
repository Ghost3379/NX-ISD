#pragma once

// ==============================================================================
// NX-ISD Hardware Revision Selection
// ==============================================================================
// Define the target hardware revision:
//   NX_ISD_HW_V1P4 : v1.4 Production PCB (Dedicated 3.3V Display SPI, 4-bit SDMMC, updated GPIOs)
//   NX_ISD_HW_V1P3 : v1.3 Prototype PCB (Shared SPI, TXB0106 level shifter, ZDSD NAND Flash)
//
// Can be selected via platformio.ini build_flags (e.g. -D NX_ISD_HW_V1P4 or -D NX_ISD_HW_V1P3).
// If neither is passed by compiler flag, defaults to v1.4 below:
#if !defined(NX_ISD_HW_V1P3) && !defined(NX_ISD_HW_V1P4)
  #define NX_ISD_HW_V1P4
#endif

// ==============================================================================
// 1. Common Hardware Pins (Identical on v1.3 & v1.4)
// ==============================================================================

// I2C Bus (Wire @ 400kHz Fast Mode)
#define I2C_SDA         8
#define I2C_SCL         9

// Power Control
#define PWR_NPM         17  // Switched VN PMOS High-Side Gate (Active HIGH via Q1)

// Peripherals
#define NPM             18  // NeoPixel Matrix Serial Data Out (G1 DIN stream)
#define BUZZER          10  // Electromagnetic Buzzer PWM Gate (Q3 AO3400A)

// Charging & USB Detection
#define USB_DETECT      12  // USB 5V VBUS Presence Divider (HIGH when plugged in)
#define BAT_STAT        11  // BQ25170 Charger Status (LOW = Charging, HIGH/Hi-Z = Done)

// Primary Button
#define BTN             13  // Main User Tactile Push-Button (External pull-up)

// Sensors & Clocks (Common Interrupt Lines)
#define INT_RTC         6   // RV-3028-C7 Alarm/Timer Interrupt (Active LOW)
#define INT_HR          5   // MAX30102 PPG Sample FIFO Ready Interrupt (Active LOW)
#define ALERT           4   // MAX17048 Fuel Gauge Battery Alert (Active LOW)

// Navigation Center Push
#define LEVER_PUSH      15  // Navigation Lever Center Push (Active LOW)


// ==============================================================================
// 2. Hardware Revision Specific Pin Mappings
// ==============================================================================

#if defined(NX_ISD_HW_V1P4)
// ------------------------------------------------------------------------------
// Hardware Revision v1.4 (Production Board)
// ------------------------------------------------------------------------------
#define HW_REVISION_STRING "v1.4"

// Display SPI Bus (Dedicated 3.3V Bus directly to J2 - No TXB0106 Level Shifter)
#define TFT_SCK         38  // Display Serial Clock (FSPI SCK)
#define TFT_MOSI        39  // Display Master-Out-Slave-In (FSPI MOSI)
#define TFT_RS          21  // Display Command / Data (DC)
#define TFT_RST         40  // Display Hardware Reset (Active LOW)
#define CS_TFT          48  // Display Chip Select (Active LOW)
#define TFT_PWM         47  // Backlight PWM (Q4 AO3400A Gate)

// Display Bus Configuration
// Direct 3.3V connection allows full 40 MHz SPI write clock (~43.5 FPS)
#define TFT_SPI_FREQ    40000000
#define TFT_BUS_SHARED  false // Dedicated SPI bus to display

// Display SPI Bus Compatibility Aliases
#define SPI_SCK         TFT_SCK
#define SPI_MOSI        TFT_MOSI
#define SPI_MISO        (-1) // Display is write-only; no MISO needed on dedicated bus

// MicroSD Storage (Dedicated 4-Bit SDMMC / SPI via GPIO Matrix)
#define SD_CLK          41
#define SD_CMD          43  // TXD0 pin on ESP32-S3
#define SD_DAT0         44  // RXD0 pin on ESP32-S3
#define SD_DAT1         42
#define SD_DAT2         1
#define SD_DAT3         2   // CS_SD in 1-bit SPI fallback mode
#define CS_SD           SD_DAT3

// Navigation Lever (Physical orientation corrected on v1.4 PCB layout)
#define LEVER_LEFT      14  // Lever tilted left
#define LEVER_RIGHT     16  // Lever tilted right

// Interrupts
#define INT_DOF         7   // BNO085 9-DOF IMU Motion Interrupt (Active LOW)
#define INT_ALS         (-1)// OPT3001 polled over I2C; interrupt omitted on v1.4

#elif defined(NX_ISD_HW_V1P3)
// ------------------------------------------------------------------------------
// Hardware Revision v1.3 (Prototype Board)
// ------------------------------------------------------------------------------
#define HW_REVISION_STRING "v1.3"

// Shared High-Speed SPI Bus (Display via TXB0106 Level Shifter + ZDSD NAND Flash)
#define SPI_SCK         40
#define SPI_MISO        41
#define SPI_MOSI        42

// Display Pins
#define CS_TFT          48
#define TFT_RS          21  // Command / Data
#define TFT_RST         38
#define TFT_PWM         1   // Backlight PWM
#define CS_SD           47  // ZDSD NAND Flash Chip Select

// Display SPI Aliases
#define TFT_SCK         SPI_SCK
#define TFT_MOSI        SPI_MOSI

// Display Bus Configuration & Bandwidth Restriction:
// NOTE: The TXB0106 auto-direction-sensing level shifter (U15) on v1.3 cannot hold up
// with the intended 40 MHz SPI clock. Signal edge rounding, propagation delay, and capacitive
// loading cause severe pixel jitter, missed clock cycles, and periodic display corruption at 40 MHz.
// We strictly cap v1.3 to 16 MHz for clean, reliable, flicker-free operation (~17.4 FPS).
#define TFT_SPI_FREQ    16000000
#define TFT_BUS_SHARED  true  // Shared SPI bus with ZDSD NAND Flash

// Navigation Lever (Rotated 180° assembly on physical v1.3 carrier board)
#define LEVER_LEFT      16  // Physical Left maps to GPIO 16
#define LEVER_RIGHT     14  // Physical Right maps to GPIO 14

// Interrupts
#define INT_DOF         2   // BNO085 9-DOF IMU Motion Interrupt (Active LOW)
#define INT_ALS         39  // OPT3001 Light Sensor Interrupt (Active LOW)

#endif