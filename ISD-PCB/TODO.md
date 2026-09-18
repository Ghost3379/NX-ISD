# NX-ISD PCB Revision TODO

Upcoming hardware revision tasks for the NX-ISD PCB.

## v1p1

* [x] Change the LDO to a DC/DC buck converter
* [x] Fix PMOS direction for high-side `VN` switching
* [x] Change to the new USB-C port
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

* [ ] Fix the version tag position on the bottom silkscreen
* [ ] Reposition the left-side mounting hole
* [ ] Change OPT3001 for a multispectral light sensor

## Future Ideas for a vXpY

> NOTE: The higher up a point is the more likely I m planning to integrate it in a future verison.

* [ ] PCB varient for SD-CARD isntead of Nand flash?
* [ ] Full speed flash storage, SD mode instead of pure SPI?
* [ ] Add infrared transceiver and receiver?
* [ ] MEMS Microphone?
* [ ] Add I2C Secure Element (e.g., ATECC608B)?
* [ ] Change to dual MCU architecture (ESP32 & NRF52)?
* [ ] Add bright LED at the Sensor head: Flashlight usage?
* [ ] Add Lidar?
* [ ] Add NFC?
* [ ] Wireless charging coil at second plate under battery?
