#include "core/touch_driver.h"

// Static touch point data
static struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} touchPoint = {0, 0, false};

// Constructor
TouchDriver::TouchDriver() : indev(nullptr) {
}

// Initialize touch controller
bool TouchDriver::init() {
    // Initialize I2C for GT911
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000); // 100kHz for GT911
    
    Serial.println("Initializing touch controller...");
    
    // Reset touch controller
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(10);
    
    Serial.println("Touch controller GT911 I2C initialized at 100kHz");
    
    // Initialize GT911
    Serial.println("Initializing GT911 touch controller...");
    if (gt911Init()) {
        Serial.println("Touch Controller: GT911 READY");
    } else {
        Serial.println("Touch Controller: GT911 FAILED");
        return false;
    }
    
    // Register touch controller with LVGL
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    
    return true;
}

// GT911 initialization
bool TouchDriver::gt911Init() {
    // Read Product ID to verify communication
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(GT911_PRODUCT_ID >> 8);
    Wire.write(GT911_PRODUCT_ID & 0xFF);
    if (Wire.endTransmission() != 0) {
        Serial.println("GT911: Failed to communicate!");
        return false;
    }
    
    Wire.requestFrom(GT911_ADDR, 4);
    if (Wire.available() >= 4) {
        char productId[5] = {0};
        productId[0] = Wire.read();
        productId[1] = Wire.read();
        productId[2] = Wire.read();
        productId[3] = Wire.read();
        Serial.printf("GT911 Product ID: %s\n", productId);
    }
    
    return true;
}

// Read GT911 touch coordinates
bool TouchDriver::readGT911Touch(uint16_t *x, uint16_t *y) {
    // Read touch status register
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(GT911_POINT_INFO >> 8);
    Wire.write(GT911_POINT_INFO & 0xFF);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    Wire.requestFrom(GT911_ADDR, 1);
    if (!Wire.available()) {
        return false;
    }
    
    uint8_t status = Wire.read();
    uint8_t touches = status & 0x0F;
    
    if (touches == 0 || !(status & 0x80)) {
        // Clear the ready flag
        Wire.beginTransmission(GT911_ADDR);
        Wire.write(GT911_POINT_INFO >> 8);
        Wire.write(GT911_POINT_INFO & 0xFF);
        Wire.write(0x00);
        Wire.endTransmission();
        return false;
    }
    
    // Read first touch point coordinates
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(GT911_POINT_1 >> 8);
    Wire.write(GT911_POINT_1 & 0xFF);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    Wire.requestFrom(GT911_ADDR, 6);
    if (Wire.available() >= 6) {
        uint8_t trackId = Wire.read();
        uint8_t xl = Wire.read();
        uint8_t xh = Wire.read();
        uint8_t yl = Wire.read();
        uint8_t yh = Wire.read();
        uint8_t size = Wire.read();
        
        *x = xl | (xh << 8);
        *y = yl | (yh << 8);
        
        // Clear the ready flag
        Wire.beginTransmission(GT911_ADDR);
        Wire.write(GT911_POINT_INFO >> 8);
        Wire.write(GT911_POINT_INFO & 0xFF);
        Wire.write(0x00);
        Wire.endTransmission();
        
        return true;
    }
    
    return false;
}

// LVGL touchpad read callback
void TouchDriver::my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    
    if (readGT911Touch(&x, &y)) {
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
