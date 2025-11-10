#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>
#include "config.h"

// LovyanGFX configuration for Elecrow CrowPanel 7"
class LGFX : public lgfx::LGFX_Device
{
public:
  lgfx::Bus_RGB     _bus_instance;
  lgfx::Panel_RGB   _panel_instance;
  lgfx::Touch_GT911 _touch_instance;

  LGFX(void);
};

// Display driver class
class DisplayDriver {
public:
    DisplayDriver();
    bool init();
    lv_display_t* getDisplay() { return disp; }
    
    // Direct screen buffer access for screenshots
    LGFX* getLCD() { return &lcd; }
    
    // Backlight control (brightness 0-255, or use setBacklightOn/Off for simple on/off)
    void setBacklight(uint8_t brightness);
    void setBacklightOn();
    void setBacklightOff();
    
    // Power management - deep sleep preparation
    void powerDown();
    
private:
    LGFX lcd;
    lv_display_t *disp;
    lv_color_t *disp_draw_buf;
    lv_color_t *disp_draw_buf2;
    
    static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
};

#endif // DISPLAY_DRIVER_H
