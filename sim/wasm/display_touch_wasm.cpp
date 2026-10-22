// Browser DisplayDriver + TouchDriver: LVGL renders the whole 800x480 screen
// into g_fb (RGB565, DIRECT mode); the page reads it through ft_framebuffer()
// whenever ft_frame() changes. Touch comes from the page via ft_touch().
// Mirrors sim/src/display_driver_sim.cpp and touch_driver_sim.cpp.

#include "core/display_driver.h"
#include "core/touch_driver.h"
#include "core/power_manager.h"

#include <emscripten.h>

#include <atomic>

namespace {
uint16_t g_fb[SCREEN_WIDTH * SCREEN_HEIGHT];
std::atomic<uint32_t> g_frame{0};

std::atomic<bool> g_touch_down{false};
std::atomic<int> g_touch_x{0};
std::atomic<int> g_touch_y{0};

struct {
    bool was_pressed;
    bool waking_from_dim;
} touchState = {false, false};

void flushCb(lv_display_t *disp, const lv_area_t *, uint8_t *) {
    if (lv_display_flush_is_last(disp)) g_frame.fetch_add(1);
    lv_display_flush_ready(disp);
}
} // namespace

extern "C" {
// Page API (called from the browser's main thread)
EMSCRIPTEN_KEEPALIVE uint16_t *ft_framebuffer() { return g_fb; }
EMSCRIPTEN_KEEPALIVE uint32_t ft_frame() { return g_frame.load(); }
EMSCRIPTEN_KEEPALIVE int ft_width() { return SCREEN_WIDTH; }
EMSCRIPTEN_KEEPALIVE int ft_height() { return SCREEN_HEIGHT; }

EMSCRIPTEN_KEEPALIVE void ft_touch(int x, int y, int down) {
    g_touch_x = x;
    g_touch_y = y;
    g_touch_down = down != 0;
}
}

// ---------------------------------------------------------------------------
// DisplayDriver
// ---------------------------------------------------------------------------
DisplayDriver::DisplayDriver()
    : disp(nullptr), backlight_overlay(nullptr), current_rotation(0) {}

bool DisplayDriver::init() {
    Serial.println("[SIM] DisplayDriver: 800x480 framebuffer for the page");
    lv_init();

    disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!disp) return false;
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, g_fb, nullptr, sizeof(g_fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flushCb);

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
    Serial.printf("[SIM] Display rotation set to %d degrees (not applied in simulator)\n", rotation * 90);
}

uint8_t DisplayDriver::getRotation() const { return current_rotation; }

// ---------------------------------------------------------------------------
// TouchDriver
// ---------------------------------------------------------------------------
TouchDriver::TouchDriver() : indev(nullptr) {}

bool TouchDriver::init(LGFX *) {
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    Serial.println("[SIM] TouchDriver: touch from the page (ft_touch)");
    return true;
}

void TouchDriver::my_touchpad_read(lv_indev_t *, lv_indev_data_t *data) {
    bool pressed = g_touch_down;
    int x = g_touch_x, y = g_touch_y;

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
