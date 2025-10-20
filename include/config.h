#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Display settings
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// Touch controller pins and I2C address
#define TOUCH_SDA  19
#define TOUCH_SCL  20
#define TOUCH_RST  38
#define GT911_ADDR 0x5D

// GT911 register addresses
#define GT911_POINT_INFO  0x814E
#define GT911_POINT_1     0x814F
#define GT911_CONFIG_REG  0x8047
#define GT911_PRODUCT_ID  0x8140

// UI Layout constants
#define STATUS_BAR_HEIGHT 60
#define TAB_BUTTON_HEIGHT 50

// Display buffer configuration
#define BUFFER_LINES 160  // 1/3 screen buffer (160 lines) for good performance without excessive memory use

// Timing constants
#define SPLASH_DURATION_MS 2500

// Preferences namespace
#define PREFS_NAMESPACE "fluidtouch"

// Screenshot server configuration
#define ENABLE_SCREENSHOT_SERVER true

#endif // CONFIG_H
