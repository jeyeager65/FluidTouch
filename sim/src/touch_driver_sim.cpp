// Simulator TouchDriver: the mouse acts as the touchscreen (left button =
// finger down). Mirrors src/core/touch_driver.cpp, including notifying the
// PowerManager and swallowing the touch that wakes a dimmed screen.

#include "core/touch_driver.h"
#include "core/display_driver.h"
#include "core/power_manager.h"

#include <SDL2/SDL.h>

namespace sim {
extern bool g_touch_override;
extern bool g_touch_pressed;
extern int g_touch_x;
extern int g_touch_y;
}

static struct {
    bool was_pressed;
    bool waking_from_dim;
} touchState = {false, false};

TouchDriver::TouchDriver() : indev(nullptr) {}

bool TouchDriver::init(LGFX *) {
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    Serial.println("[SIM] TouchDriver: mouse left button = touch");
    return true;
}

void TouchDriver::my_touchpad_read(lv_indev_t *, lv_indev_data_t *data) {
    // SDL events are pumped by LVGL's SDL window driver; this just samples
    // the resulting mouse state (window coordinates == screen pixels).
    int x = 0, y = 0;
    bool pressed;
    if (sim::g_touch_override) {
        // --script input
        pressed = sim::g_touch_pressed;
        x = sim::g_touch_x;
        y = sim::g_touch_y;
    } else {
        uint32_t buttons = SDL_GetMouseState(&x, &y);
        pressed = (buttons & SDL_BUTTON_LMASK) != 0 && SDL_GetMouseFocus() != nullptr;
    }

    if (pressed && !touchState.was_pressed) {
        if (PowerManager::getCurrentState() != PowerManager::FULL_BRIGHTNESS) {
            touchState.waking_from_dim = true;
        }
        PowerManager::onUserActivity();
    }
    if (!pressed) touchState.waking_from_dim = false;
    touchState.was_pressed = pressed;

    if (pressed && !touchState.waking_from_dim) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (int32_t)LV_CLAMP(0, x, SCREEN_WIDTH - 1);
        data->point.y = (int32_t)LV_CLAMP(0, y, SCREEN_HEIGHT - 1);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
