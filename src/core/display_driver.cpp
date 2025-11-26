#include "core/display_driver.h"
#include <esp_heap_caps.h>
#include <Wire.h>

// LovyanGFX constructor
LGFX::LGFX(void) {
    {
        auto cfg = _bus_instance.config();
        cfg.panel = &_panel_instance;
        
#ifdef HARDWARE_ADVANCE
        // Advance: IPS LCD (800x480) - Per Elecrow official example
        cfg.pin_d0  = GPIO_NUM_21; // B0
        cfg.pin_d1  = GPIO_NUM_47; // B1
        cfg.pin_d2  = GPIO_NUM_48; // B2
        cfg.pin_d3  = GPIO_NUM_45; // B3
        cfg.pin_d4  = GPIO_NUM_38; // B4
        
        cfg.pin_d5  = GPIO_NUM_9;  // G0
        cfg.pin_d6  = GPIO_NUM_10; // G1
        cfg.pin_d7  = GPIO_NUM_11; // G2
        cfg.pin_d8  = GPIO_NUM_12; // G3
        cfg.pin_d9  = GPIO_NUM_13; // G4
        cfg.pin_d10 = GPIO_NUM_14; // G5
        
        cfg.pin_d11 = GPIO_NUM_7;  // R0
        cfg.pin_d12 = GPIO_NUM_17; // R1
        cfg.pin_d13 = GPIO_NUM_18; // R2
        cfg.pin_d14 = GPIO_NUM_3;  // R3
        cfg.pin_d15 = GPIO_NUM_46; // R4

        cfg.pin_henable = GPIO_NUM_42;
        cfg.pin_vsync   = GPIO_NUM_41;
        cfg.pin_hsync   = GPIO_NUM_40;
        cfg.pin_pclk    = GPIO_NUM_39;
        cfg.freq_write  = 14000000;  // 14MHz - best stability

        cfg.hsync_polarity    = 1;  // Different from Basic!
        cfg.hsync_front_porch = 8;
        cfg.hsync_pulse_width = 4;
        cfg.hsync_back_porch  = 8;
        
        cfg.vsync_polarity    = 1;  // Different from Basic!
        cfg.vsync_front_porch = 8;
        cfg.vsync_pulse_width = 4;
        cfg.vsync_back_porch  = 8;

        cfg.pclk_active_neg   = 0;  // Not explicitly set in example, using default
        cfg.de_idle_high      = 0;
        cfg.pclk_idle_high    = 1;  // Set to 1 per Elecrow example
#else
        // Basic: TN LCD (800x480)
        cfg.pin_d0  = GPIO_NUM_15; // B0
        cfg.pin_d1  = GPIO_NUM_7;  // B1
        cfg.pin_d2  = GPIO_NUM_6;  // B2
        cfg.pin_d3  = GPIO_NUM_5;  // B3
        cfg.pin_d4  = GPIO_NUM_4;  // B4
        
        cfg.pin_d5  = GPIO_NUM_9;  // G0
        cfg.pin_d6  = GPIO_NUM_46; // G1
        cfg.pin_d7  = GPIO_NUM_3;  // G2
        cfg.pin_d8  = GPIO_NUM_8;  // G3
        cfg.pin_d9  = GPIO_NUM_16; // G4
        cfg.pin_d10 = GPIO_NUM_1;  // G5
        
        cfg.pin_d11 = GPIO_NUM_14; // R0
        cfg.pin_d12 = GPIO_NUM_21; // R1
        cfg.pin_d13 = GPIO_NUM_47; // R2
        cfg.pin_d14 = GPIO_NUM_48; // R3
        cfg.pin_d15 = GPIO_NUM_45; // R4

        cfg.pin_henable = GPIO_NUM_41;
        cfg.pin_vsync   = GPIO_NUM_40;
        cfg.pin_hsync   = GPIO_NUM_39;
        cfg.pin_pclk    = GPIO_NUM_0;
        cfg.freq_write  = 10000000;  // 10MHz

        cfg.hsync_polarity    = 0;
        cfg.hsync_front_porch = 40;
        cfg.hsync_pulse_width = 48;
        cfg.hsync_back_porch  = 40;
        
        cfg.vsync_polarity    = 0;
        cfg.vsync_front_porch = 1;
        cfg.vsync_pulse_width = 31;
        cfg.vsync_back_porch  = 13;

        cfg.pclk_active_neg   = 1;
        cfg.de_idle_high      = 0;
        cfg.pclk_idle_high    = 0;
#endif

        _bus_instance.config(cfg);
    }
    
    {
        auto cfg = _panel_instance.config();
        cfg.memory_width  = SCREEN_WIDTH;
        cfg.memory_height = SCREEN_HEIGHT;
        cfg.panel_width  = SCREEN_WIDTH;
        cfg.panel_height = SCREEN_HEIGHT;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        _panel_instance.config(cfg);
    }
    
#ifdef HARDWARE_ADVANCE
    // Enable PSRAM usage for Advance hardware (per Elecrow example)
    {
        auto cfg = _panel_instance.config_detail();
        cfg.use_psram = 1;
        _panel_instance.config_detail(cfg);
    }
#endif
    
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
    
    // Configure GT911 touch panel
    {
        auto cfg = _touch_instance.config();
        cfg.x_min      = 0;
        cfg.x_max      = 799;
        cfg.y_min      = 0;
        cfg.y_max      = 479;
        cfg.pin_int    = -1;   // Not used
        cfg.bus_shared = true;
        cfg.offset_rotation = 0;

#ifdef HARDWARE_ADVANCE
        // Advance: Touch on different I2C pins
        cfg.i2c_port   = 0;
        cfg.i2c_addr   = 0x5D;
        cfg.pin_sda    = TOUCH_SDA;  // GPIO 15
        cfg.pin_scl    = TOUCH_SCL;  // GPIO 16
        cfg.pin_rst    = -1;  // Reset handled by STC8H1K28 via I2C
        cfg.freq       = 400000;
#else
        // Basic: Touch I2C configuration
        cfg.i2c_port   = 0;
        cfg.i2c_addr   = 0x5D;
        cfg.pin_sda    = TOUCH_SDA;  // GPIO 19
        cfg.pin_scl    = TOUCH_SCL;  // GPIO 20
        cfg.pin_rst    = -1;  // No reset pin defined in Elecrow schematic
        cfg.freq       = 400000;
#endif

        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
    }
}

// DisplayDriver constructor
DisplayDriver::DisplayDriver() : disp(nullptr), disp_draw_buf(nullptr), disp_draw_buf2(nullptr) {
}

// Initialize display
bool DisplayDriver::init() {
    // Initialize I2C bus first (shared by backlight and touch on Advance)
    // Touch driver will call Wire.begin() again but that's safe if already initialized
    
    // Initialize backlight based on hardware variant
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight - configure but keep OFF until screen is cleared
    pinMode(2, OUTPUT);
    ledcSetup(1, 300, 8);
    ledcAttachPin(2, 1);
    ledcWrite(1, 0);  // Start with backlight OFF
    Serial.println("Backlight configured (OFF)");
    
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // No GPIO manipulation - backlight controlled purely via I2C
    Serial.println("Initializing Advance hardware (I2C backlight control)...");
    
    // NOTE: Do NOT call Wire.begin() here - LovyanGFX will initialize the I2C bus
    // for the touch panel first, then we can use it for STC8H1K28 communication
    
    // We'll initialize the backlight AFTER LovyanGFX init
#else
    #error "No backlight type defined! Use -DBACKLIGHT_PWM or -DBACKLIGHT_I2C"
#endif
    
    // Initialize LovyanGFX (this will initialize I2C for touch panel)
    lcd.init();
    lcd.setColorDepth(16);
    lcd.setBrightness(255);
    lcd.fillScreen(0x0000);  // Clear screen to black
    
#ifdef BACKLIGHT_PWM
    // Wait for fillScreen to complete before turning on backlight
    // This prevents the garbled flash of old frame buffer contents
    delay(50);
    
    // Now turn on backlight after screen is cleared
    ledcWrite(1, 255);
    Serial.println("Backlight ON (PWM)");
#endif
    
#ifdef BACKLIGHT_I2C
    // Now initialize STC8H1K28 backlight (I2C already initialized by LovyanGFX)
    Wire.setClock(100000);  // 100kHz for compatibility
    Wire.setTimeOut(100);   // Prevent hangs
    delay(50);  // Give I2C time to stabilize
    
    // Wake STC8H1K28 microcontroller (per Elecrow example)
    Serial.println("Waking STC8H1K28 microcontroller...");
    Wire.beginTransmission(0x30);
    Wire.write(0x19);  // Wake command
    uint8_t wakeError = Wire.endTransmission();
    Serial.printf("  Wake result: %d\n", wakeError);
    delay(10);
    
    // STC8H1K28 handles GT911 reset internally - no GPIO manipulation needed
    // Just send configuration commands
    Serial.println("Configuring STC8H1K28 for GT911 reset...");
    
    // Send reset command sequence to STC8H1K28
    Wire.beginTransmission(0x30);
    Wire.write(0x10);  // Config command 1
    Wire.endTransmission();
    delay(10);
    
    Wire.beginTransmission(0x30);
    Wire.write(0x18);  // Config command 2  
    Wire.endTransmission();
    delay(100);  // Give GT911 time to initialize after STC8H1K28 reset
    
    // Scan I2C to confirm GT911 is present
    Serial.println("Scanning I2C for GT911...");
    int deviceCount = 0;
    bool gt911_found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found device at 0x%02X\n", addr);
            if (addr == 0x5D || addr == 0x14) gt911_found = true;
            deviceCount++;
        }
        delay(1);
    }
    Serial.printf("Found %d I2C device(s), GT911: %s\n", deviceCount, gt911_found ? "YES" : "NO");
    
    // Turn on backlight via STC8H1K28
    // The STC8H1K28 controls LCD backlight via P3.5 and brightness via P1.1
    // All control is via I2C commands to address 0x30
    Serial.println("Enabling backlight via STC8H1K28...");
    
    // Try sending brightness value (0 = brightest, 245 = off)
    Wire.beginTransmission(0x30);
    Wire.write(0x00);  // Maximum brightness
    uint8_t blResult = Wire.endTransmission();
    Serial.printf("  Brightness command result: %d\n", blResult);
    delay(10);
    
    // Also try the "buzzer off" command in case backlight shares control
    Wire.beginTransmission(0x30);
    Wire.write(0xF7);  // 247 = buzzer off (per docs)
    Wire.endTransmission();
    delay(10);
    
    Serial.println("Backlight initialization complete");
#endif
    
    // Initialize LVGL
    lv_init();
    
    // Allocate display buffers in PSRAM (dual buffering for smooth rendering)
    uint32_t buf_size = SCREEN_WIDTH * BUFFER_LINES;
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    if (!disp_draw_buf || !disp_draw_buf2) {
        Serial.println("ERROR: Failed to allocate display buffers in PSRAM!");
        return false;
    }
    
    Serial.printf("Display buffers allocated in PSRAM: 2 x %lu bytes\n", buf_size * sizeof(lv_color_t));
    
    // Create LVGL display
    disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, disp_draw_buf2, buf_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Store lcd instance in display user data for flush callback
    lv_display_set_user_data(disp, &lcd);
    
    return true;
}

// LVGL flush callback
void DisplayDriver::my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    LGFX *lcd = (LGFX *)lv_display_get_user_data(disp);
    
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    
    lv_draw_sw_rgb565_swap(px_map, w * h);
    lcd->pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)px_map);
    
    lv_display_flush_ready(disp);
}

// Backlight control methods
void DisplayDriver::setBacklight(uint8_t brightness_percent) {
    // Clamp to valid percentage range
    if (brightness_percent > 100) brightness_percent = 100;
    
    // Convert percentage (0-100) to hardware value (0-255)
    uint8_t hw_value = (brightness_percent * 255) / 100;
    
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight on GPIO2
    ledcWrite(1, hw_value);
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // Brightness: 0 = brightest, 245 = off
    uint8_t i2c_value = 245 - ((hw_value * 245) / 255);
    Wire.beginTransmission(0x30);
    Wire.write(i2c_value);
    Wire.endTransmission();
#endif
    Serial.printf("Backlight set to: %d%% (hw=%d)\n", brightness_percent, hw_value);
}

void DisplayDriver::setBacklightOn() {
    setBacklight(255);
}

void DisplayDriver::setBacklightOff() {
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight on GPIO2
    ledcWrite(1, 0);
    Serial.println("Backlight OFF (PWM)");
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // Send 0xF5 (245) for off
    Wire.beginTransmission(0x30);
    Wire.write(0xF5);
    Wire.endTransmission();
    Serial.println("Backlight OFF (I2C)");
#endif
}

void DisplayDriver::powerDown() {
    Serial.println("DisplayDriver: Powering down display for deep sleep...");
    
    // Only turn off backlight - do NOT send GT911 sleep command or reconfigure GPIOs
    // The GT911 touch controller on Basic hardware has no reset pin, so if we put it
    // to sleep (register 0x8040 = 0x05), it cannot be woken after ESP32 deep sleep.
    // Leaving GT911 in normal mode allows it to wake naturally when ESP32 wakes.
    // (Advance hardware works either way because STC8H1K28 handles GT911 reset independently)
    setBacklightOff();
    
    Serial.println("  Display powered down (backlight off only)");
}

