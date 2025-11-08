#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define FLUIDTOUCH_VERSION "0.9.2"

// Display settings
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// Hardware-specific pin configurations
#ifdef HARDWARE_ADVANCE
// CrowPanel 7" Advance - per Elecrow example code
// https://www.elecrow.com/pub/wiki/ESP32_Display-7.0_inch%28Advance_Series%29wiki.html
#define TOUCH_SDA  15
#define TOUCH_SCL  16
#define TOUCH_RST  -1  // Reset handled by STC8H1K28 microcontroller via I2C
#define TOUCH_INT  -1  // Not used
#else
// CrowPanel 7" Basic
#define TOUCH_SDA  19
#define TOUCH_SCL  20
#define TOUCH_RST  38
#define TOUCH_INT  -1
#endif

// Touch controller I2C address
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
