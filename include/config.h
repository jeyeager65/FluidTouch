#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define FLUIDTOUCH_VERSION "1.0.5"

// Display settings
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Hardware-specific pin configurations
#ifdef HARDWARE_WAVESHARE_7
// Waveshare ESP32-S3-Touch-LCD-7
// Reference: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7
#define TOUCH_SDA 8
#define TOUCH_SCL 9
#define TOUCH_RST -1 // TP_RST is behind CH422G expander; held active by hardware path
#define TOUCH_INT 4

// Waveshare CH422G I/O expander (shared on same I2C bus)
#define CH422G_I2C_ADDR 0x24
#define CH422G_TOUCH_RST_PIN 1
#define CH422G_LCD_BL_PIN 2
#define CH422G_LCD_RST_PIN 3
#define CH422G_SD_CS_PIN 4
#define CH422G_USB_SEL_PIN 5
#define CH422G_BACKLIGHT_ON HIGH
#define CH422G_BACKLIGHT_OFF LOW
#define CH422G_SD_SELECTED LOW
#define CH422G_SD_DESELECTED HIGH
#define CH422G_USB_MODE LOW // LOW = USB mode, HIGH = CAN mode
#elif defined(HARDWARE_ADVANCE)
// CrowPanel 7" Advance - per Elecrow example code
// https://www.elecrow.com/pub/wiki/ESP32_Display-7.0_inch%28Advance_Series%29wiki.html
#define TOUCH_SDA 15
#define TOUCH_SCL 16
#define TOUCH_RST -1 // Reset handled by STC8H1K28 microcontroller via I2C
#define TOUCH_INT -1 // Not used
#else
// CrowPanel 7" Basic
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_RST 38
#define TOUCH_INT -1
#endif

// Touch controller I2C address
#define GT911_ADDR 0x5D

// GT911 register addresses
#define GT911_POINT_INFO 0x814E
#define GT911_POINT_1 0x814F
#define GT911_CONFIG_REG 0x8047
#define GT911_PRODUCT_ID 0x8140

// UI Layout constants
#define STATUS_BAR_HEIGHT 60
#define TAB_BUTTON_HEIGHT 60

// Display buffer configuration
#define BUFFER_LINES 480 // Full screen buffer for smooth rendering (with 8MB PSRAM available)

// Timing constants
#define SPLASH_DURATION_MS 2500
#define LIMIT_SWITCH_HOLD_MS 500 // Duration to keep limit switch indicators visible after trigger clears

// Preferences namespaces
#define PREFS_NAMESPACE "fluidtouch"       // Machine configurations
#define PREFS_SYSTEM_NAMESPACE "ft_system" // System flags (clean_shutdown, etc.)

// Screenshot server configuration
#define ENABLE_SCREENSHOT_SERVER true

// SD Card Configuration
#ifdef HARDWARE_WAVESHARE_7
// Waveshare: SD CS is controlled by CH422G EXIO4 (not a direct ESP32 GPIO)
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CLK 12
#define SD_CS -1
#elif defined(HARDWARE_ADVANCE)
// Advance: SPI mode SD card
#define SD_MOSI 6
#define SD_MISO 4
#define SD_CLK 5
#define SD_CS 0 // Not actually connected - CS tied to GND in hardware (per Elecrow example)
#else
// Basic: SPI mode SD card
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CLK 12
#define SD_CS 10
#endif

// Upload Configuration
#define FLUIDNC_UPLOAD_PATH "/fluidtouch/uploads/" // Automatically created if missing

#endif // CONFIG_H
