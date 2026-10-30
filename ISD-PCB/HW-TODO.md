# NX-ISD PCB Revision TODO

Upcoming hardware revision tasks for the NX-ISD PCB.

## v1p1

* [x] Change the LDO to a DC/DC buck converter
* [x] Fix PMOS direction for high-side `VN` switching
* [x] Change to a new USB-C port
* [x] Adjust the board size for a 1200 mAh battery
* [x] Change the NeoPixel matrix from 3x3 to 4x4

## v1p3

* [x] Add a level shifter for the display SPI interface
* [x] Add a level shifter for the HRM I2C interface
* [x] Change the resistor network to a smaller footprint
* [x] Perform the overall layout changes
* [x] Move the DC/DC buck converter
* [x] Reposition the RTC and add the required capacitors
* [x] Reposition the battery fuel gauge
* [x] Change silkscreen font to cyberpunk style
* [x] Add LiPo polarity marks

## v1p4

* [x] Fix the version tag position on the bottom silkscreen
* [x] Reposition the left-side mounting hole
* [x] Fix Lever button wiring
* [x] Move FPC-connector for the display a bit to the left
* [x] Fix BQ25170 Overheating problem at 600mA charging -> additional GND plating
* [x] Replace NAND SD with Micro SD Card slot (4- Bit SDMMC).
* [x] Change EP pad vias of MAX17048 & BQ25170 to 0.45mm drill to reduce pcb costs
* [x] Clear 2V8 path
* [x] Check all Decouple capacities for right pathing { 3V3 -> Cap+ -> VCC, Cap- -> GND }
* [x] add hardware pull up to BTN (Because of deep sleep)
* [x] Remove U15 (TXB0106) level shifter for display SPI - route ESP32-S3 GPIOs directly to J2 (ST7789 IOVCC supports 3.3V; TXB0106 auto-sensing one-shot oscillates and overheats/fails at 40MHz SPI)
* [x] Add Decouple caps for NPM
* [x] Add plating for GND and VUSB at USB-C Connector
* [x] Perform the overall layout changes (overall plating and better routing)
* [x] Fix Neopixel silkscreeen (Point is pin one, but pin 1 is Data Out -> Use pin 3 GND, since it has a mark on top of the LED, to make production by hand easier)
* [x] Reposition RST and BOOT for better access, and change solder jumper to small buttons 
* [x] Edit USBLC6 footrpint for easier soldering
* [x] Include Supercap as backup for RTC
* [partially] Add addtional cyberpunk silkscreen design

## Future Ideas for a vXpY

> NOTE: The higher up a point is the more likely I m planning to integrate it in a future verison.


* [x] PCB varient for SD-CARD isntead of Nand flash?
* [ ] Change OPT3001 for a multispectral light sensor
* [x] Full speed flash storage, SD mode instead of pure SPI?
* [ ] Add infrared transceiver and receiver?
* [ ] MEMS Microphone?
* [ ] Add I2C Secure Element (e.g., ATECC608B)?
* [ ] Change to dual MCU architecture (ESP32 & NRF52)?
* [ ] Add bright LED at the Sensor head: Flashlight usage?
* [ ] Add Lidar? TOF?
* [ ] Add NFC?
* [ ] Wireless charging coil at second plate under battery?
* [ ] EM proximity sensing coil?
