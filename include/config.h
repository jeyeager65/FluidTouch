#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define FLUIDTOUCH_VERSION "1.0.4"

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

// Preferences namespaces
#define PREFS_NAMESPACE "fluidtouch"        // Machine configurations
#define PREFS_SYSTEM_NAMESPACE "ft_system"  // System flags (clean_shutdown, etc.)

// Screenshot server configuration
#define ENABLE_SCREENSHOT_SERVER true

// SD Card Configuration
#ifdef HARDWARE_ADVANCE
// Advance: SPI mode SD card
#define SD_MOSI  6
#define SD_MISO  4
#define SD_CLK   5
#define SD_CS    0  // Not actually connected - CS tied to GND in hardware (per Elecrow example)
#else
// Basic: SPI mode SD card
#define SD_MOSI  11
#define SD_MISO  13
#define SD_CLK   12
#define SD_CS    10
#endif

// Upload Configuration
#define FLUIDNC_UPLOAD_PATH "/fluidtouch/uploads/"  // Automatically created if missing

// Battery Monitor Configuration (MAX17048 I2C Fuel Gauge)
// The MAX17048 connects to the I2C bus (shared with touch controller).
// It auto-detects on init - if not found, battery monitoring is disabled gracefully.
// Comment out BATTERY_ENABLED to disable battery monitoring entirely.
#define BATTERY_ENABLED
#define MAX17048_ADDR           0x36      // MAX17048 I2C address (fixed)
#define BATTERY_VOLTAGE_MAX     4.2f      // Fully charged voltage (Li-ion/LiPo)
#define BATTERY_VOLTAGE_MIN     3.0f      // Empty voltage (Li-ion cutoff)
#define BATTERY_CHARGE_RATE_THRESHOLD 0.5f // %/hour above which we consider "charging"

#endif // CONFIG_H
