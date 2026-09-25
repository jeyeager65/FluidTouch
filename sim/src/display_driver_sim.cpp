// SDL-backed DisplayDriver for the simulator (see sim/include/core/display_driver.h).

#include "core/display_driver.h"

#include <SDL2/SDL.h>

DisplayDriver::DisplayDriver()
    : disp(nullptr), backlight_overlay(nullptr), current_rotation(0) {}

bool DisplayDriver::init() {
    Serial.println("[SIM] DisplayDriver: creating 800x480 SDL window");
    lv_init();

    disp = lv_sdl_window_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!disp) return false;
    lv_sdl_window_set_title(disp, "FluidTouch Simulator");

    // Full-screen overlay on the system layer that darkens the screen to
    // emulate backlight brightness. It never receives input.
    backlight_overlay = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(backlight_overlay);
    lv_obj_set_size(backlight_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(backlight_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backlight_overlay, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(backlight_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(backlight_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
    return true;
}

void DisplayDriver::setBacklight(uint8_t brightness_percent) {
    if (brightness_percent > 100) brightness_percent = 100;
    if (!backlight_overlay) return;
    // Map 100% -> fully clear, 0% -> fully black. Keep a floor so a very dim
    // (but on) screen is still visibly different from "off".
    lv_opa_t opa = brightness_percent == 0 ? LV_OPA_COVER
                                           : (lv_opa_t)((100 - brightness_percent) * 235 / 100);
    lv_obj_set_style_bg_opa(backlight_overlay, opa, 0);
}

void DisplayDriver::setBacklightOn() { setBacklight(100); }
void DisplayDriver::setBacklightOff() { setBacklight(0); }

void DisplayDriver::powerDown() {
    Serial.println("[SIM] DisplayDriver: powerDown (backlight off)");
    setBacklightOff();
    lv_refr_now(disp);
}

void DisplayDriver::setRotation(uint8_t rotation) {
    if (rotation != 0 && rotation != 2) {
        Serial.printf("Invalid rotation %d, must be 0 or 2\n", rotation);
        return;
    }
    current_rotation = rotation;
    // The panel is physically flipped on hardware; the simulator window
    // always shows the UI upright.
    Serial.printf("[SIM] Display rotation set to %d degrees (not applied in simulator)\n", rotation * 90);
}

uint8_t DisplayDriver::getRotation() const { return current_rotation; }
