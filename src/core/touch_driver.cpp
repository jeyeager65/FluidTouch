#include "core/touch_driver.h"
#include "core/display_driver.h"
#include "core/power_manager.h"

// Static touch point data
static struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
    bool was_pressed;  // Track previous state for edge detection
    bool waking_from_dim;  // Suppress the wakeup touch from reaching LVGL
} touchPoint = {0, 0, false, false, false};

// Static LCD instance pointer (set during init)
static LGFX *lcd_instance = nullptr;

// Constructor
TouchDriver::TouchDriver() : indev(nullptr) {
}

// Initialize touch controller
bool TouchDriver::init(LGFX *lcd) {
    Serial.println("Initializing touch controller...");
    
    // Store LCD instance for touch reading
    lcd_instance = lcd;
    
    Serial.printf("Touch I2C pins: SDA=%d, SCL=%d (managed by LovyanGFX)\n", TOUCH_SDA, TOUCH_SCL);
    Serial.println("Touch Controller: GT911 initialized by LovyanGFX");
    
    // Register touch controller with LVGL
    // The my_touchpad_read callback will call LovyanGFX's getTouch() method
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    Serial.println("Touch driver registered with LVGL");
    return true;
}

// LVGL touchpad read callback
void TouchDriver::my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (!lcd_instance) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    
    uint16_t x, y;
    
    // Use LovyanGFX's getTouch() method
    if (lcd_instance->getTouch(&x, &y)) {
        touchPoint.x = x;
        touchPoint.y = y;
        touchPoint.pressed = true;
    } else {
        touchPoint.pressed = false;
    }
    
    // Detect touch press edge (transition from not pressed to pressed)
    if (touchPoint.pressed && !touchPoint.was_pressed) {
        // If display is not at full brightness, this touch is a wake-up tap —
        // restore brightness but suppress the touch from reaching LVGL
        if (PowerManager::getCurrentState() != PowerManager::FULL_BRIGHTNESS) {
            touchPoint.waking_from_dim = true;
        }
        // Notify power manager of user activity (restores brightness)
        PowerManager::onUserActivity();
    }
    
    // Clear the waking flag once the finger is lifted
    if (!touchPoint.pressed) {
        touchPoint.waking_from_dim = false;
    }
    
    touchPoint.was_pressed = touchPoint.pressed;
    
    if (touchPoint.pressed && !touchPoint.waking_from_dim) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchPoint.x;
        data->point.y = touchPoint.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
