// Simulator replacement for include/core/display_driver.h.
//
// Same public API as the hardware driver, but renders into an SDL window
// (sim/src/display_driver_sim.cpp). Backlight brightness is emulated with a
// dark overlay so dim/sleep power states are visible.
#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <lvgl.h>
#include "config.h"

// Opaque on the simulator - there is no LovyanGFX device
class LGFX;

class DisplayDriver {
public:
    DisplayDriver();
    bool init();
    lv_display_t* getDisplay() { return disp; }

    LGFX* getLCD() { return nullptr; }

    void setBacklight(uint8_t brightness_percent);
    void setBacklightOn();
    void setBacklightOff();

    void powerDown();

    void setRotation(uint8_t rotation);
    uint8_t getRotation() const;

private:
    lv_display_t *disp;
    lv_obj_t *backlight_overlay;
    uint8_t current_rotation;
};

#endif // DISPLAY_DRIVER_H
