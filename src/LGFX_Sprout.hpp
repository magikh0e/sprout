// LovyanGFX board config for the Sunton ESP32-3248S035R (3.5" CYD).
//
// Pins are the ones measured on real hardware in the pueo project
// (docs/pueo/build-guide.md + the TFT_eSPI "User_Setup cyd.h"):
//
//   Display ST7796 on HSPI:  SCLK 14  MOSI 13  MISO 12  CS 15  DC 2  RST -1
//   Backlight:               GPIO 27  (active HIGH)  <-- NOT 21
//   Touch XPT2046:           CS 33, shares the display SPI bus
//   Colour order:            BGR, inversion OFF
//
// Everything else on this board (RGB LED 4/16/17, LDR 34, speaker 26) is
// plain GPIO and is driven from main.cpp, not here.

#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_Sprout : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_XPT2046 _touch;

public:
  LGFX_Sprout() {
    {  // --- SPI bus (HSPI, shared by panel + touch) ---
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;   // HSPI
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;    // 40 MHz is a safe bring-up clock
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_dc      = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {  // --- ST7796 panel ---
      auto cfg = _panel.config();
      cfg.pin_cs          = 15;
      cfg.pin_rst         = -1;      // tied to the ESP32 reset line
      cfg.pin_busy        = -1;
      cfg.panel_width     = 320;
      cfg.panel_height    = 480;
      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = true;
      cfg.invert          = false;   // TFT_INVERSION_OFF
      cfg.rgb_order       = false;   // BGR (false = RGB order flag off => BGR panel)
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = true;
      _panel.config(cfg);
    }

    {  // --- Backlight on GPIO 27 ---
      auto cfg = _light.config();
      cfg.pin_bl      = 27;
      cfg.invert      = false;       // active HIGH
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    {  // --- Touch XPT2046 (on the same HSPI bus, CS 33) ---
      auto cfg = _touch.config();
      cfg.x_min      = 300;          // raw ADC range; calibrate later
      cfg.x_max      = 3900;
      cfg.y_min      = 300;
      cfg.y_max      = 3900;
      cfg.pin_int    = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.spi_host   = SPI2_HOST;
      cfg.freq       = 2500000;      // 2.5 MHz, from the proven setup
      cfg.pin_sclk   = 14;
      cfg.pin_mosi   = 13;
      cfg.pin_miso   = 12;
      cfg.pin_cs     = 33;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }

    setPanel(&_panel);
  }
};
