#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define FLUIDTOUCH_VERSION "1.0.0-dev"

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
#define TAB_BUTTON_HEIGHT 60

// Display buffer configuration
#define BUFFER_LINES 480  // Full screen buffer for smooth rendering (with 8MB PSRAM available)

// Timing constants
#define SPLASH_DURATION_MS 2500

// Preferences namespace
#define PREFS_NAMESPACE "fluidtouch"

// Screenshot server configuration
#define ENABLE_SCREENSHOT_SERVER true

#endif // CONFIG_H
