#include "core/touch_driver.h"
#include "core/display_driver.h"

// Static touch point data
static struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} touchPoint = {0, 0, false};

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
    
    if (touchPoint.pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchPoint.x;
        data->point.y = touchPoint.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
