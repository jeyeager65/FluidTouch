#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <lvgl.h>
#include <Wire.h>
#include "config.h"

// Touch driver class
class TouchDriver {
public:
    TouchDriver();
    bool init();
    lv_indev_t* getInputDevice() { return indev; }
    
private:
    lv_indev_t *indev;
    
    static bool gt911Init();
    static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);
    static bool readGT911Touch(uint16_t *x, uint16_t *y);
    static void writeGT911Register(uint16_t reg, uint8_t *data, uint8_t len);
    static void readGT911Register(uint16_t reg, uint8_t *data, uint8_t len);
};

#endif // TOUCH_DRIVER_H
