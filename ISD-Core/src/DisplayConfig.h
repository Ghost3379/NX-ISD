#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins.h"

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789      _panel_instance;
  lgfx::Bus_SPI           _bus_instance;
  lgfx::Light_PWM         _light_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host   = SPI2_HOST;     // FSPI on ESP32-S3
      cfg.spi_mode   = 0;             // Mode 0
      cfg.freq_write = 16000000;      // 16 MHz: Rock-solid through TXB0106 level shifter
      cfg.freq_read  = 10000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk   = SPI_SCK;       // GPIO 40
      cfg.pin_mosi   = SPI_MOSI;      // GPIO 42
      cfg.pin_miso   = SPI_MISO;      // GPIO 41
      cfg.pin_dc     = TFT_RS;        // GPIO 21

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs           = CS_TFT;  // GPIO 48
      cfg.pin_rst          = TFT_RST; // GPIO 38
      cfg.pin_busy         = -1;

      cfg.panel_width      = 240;
      cfg.panel_height     = 240;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;    // 1.54" IPS panels require color inversion ON
      cfg.rgb_order        = false;   // RGB
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;    // Shared SPI bus with SD card

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();

      cfg.pin_bl      = TFT_PWM;      // GPIO 1
      cfg.invert      = false;        // Active HIGH via N-MOSFET Q4
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
