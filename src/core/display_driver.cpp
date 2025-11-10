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
        cfg.pin_rst    = 38;  // GPIO 38 for reset
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
    // Basic: PWM backlight on GPIO2
    pinMode(2, OUTPUT);
    ledcSetup(1, 300, 8);
    ledcAttachPin(2, 1);
    ledcWrite(1, 255);
    Serial.println("Backlight ON (PWM)");
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // No GPIO manipulation - backlight controlled purely via I2C
    Serial.println("Initializing Advance hardware (I2C backlight control)...");
    
    // Initialize I2C for STC8H1K28 communication
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
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
#else
    #error "No backlight type defined! Use -DBACKLIGHT_PWM or -DBACKLIGHT_I2C"
#endif
    
    // Initialize LovyanGFX
    lcd.init();
    lcd.setColorDepth(16);
    lcd.setBrightness(255);
    lcd.fillScreen(0x0000);  // Clear screen to black
    
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
void DisplayDriver::setBacklight(uint8_t brightness) {
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight on GPIO2
    ledcWrite(1, brightness);
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // Brightness: 0 = brightest, 245 = off
    uint8_t i2c_value = 245 - ((brightness * 245) / 255);
    Wire.beginTransmission(0x30);
    Wire.write(i2c_value);
    Wire.endTransmission();
#endif
    Serial.printf("Backlight set to: %d\n", brightness);
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
    
    // Turn off backlight - this is the most important for power savings
    setBacklightOff();
    delay(100);  // Give backlight time to fully turn off
    
    // Power down GT911 touch controller to prevent touch-induced current draw
    Serial.println("  Powering down GT911 touch controller...");
    Wire.begin(TOUCH_SDA, TOUCH_SCL);  // Ensure I2C is initialized
    Wire.setClock(400000);
    
    // GT911 sleep command: write 0x05 to register 0x8040
    Wire.beginTransmission(0x5D);
    Wire.write(0x80);  // Register high byte
    Wire.write(0x40);  // Register low byte
    Wire.write(0x05);  // Sleep command
    uint8_t result = Wire.endTransmission();
    Serial.printf("  GT911 sleep command result: %d\n", result);
    delay(10);
    
#ifdef BACKLIGHT_I2C
    // Ensure STC8H1K28 buzzer is off (Advance hardware only)
    Serial.println("  Disabling STC8H1K28 buzzer...");
    Wire.beginTransmission(0x30);
    Wire.write(0xF7);  // Buzzer off command
    uint8_t stc_result = Wire.endTransmission();
    Serial.printf("  STC8H1K28 buzzer off result: %d\n", stc_result);
    delay(10);
#endif
    
    // Shut down I2C peripheral to save power
    Serial.println("  Shutting down I2C bus...");
    Wire.end();
    
    // Set I2C pins to INPUT with pull-down to prevent floating
    pinMode(TOUCH_SDA, INPUT);
    pinMode(TOUCH_SCL, INPUT);
    digitalWrite(TOUCH_SDA, LOW);
    digitalWrite(TOUCH_SCL, LOW);
    
    // Disable RGB parallel interface by setting all data pins low
    Serial.println("  Disabling RGB parallel interface...");
#ifdef HARDWARE_ADVANCE
    // Advance pin mappings - set as INPUT to reduce power
    pinMode(GPIO_NUM_21, INPUT); pinMode(GPIO_NUM_47, INPUT);
    pinMode(GPIO_NUM_48, INPUT); pinMode(GPIO_NUM_45, INPUT);
    pinMode(GPIO_NUM_38, INPUT); pinMode(GPIO_NUM_9, INPUT);
    pinMode(GPIO_NUM_10, INPUT); pinMode(GPIO_NUM_11, INPUT);
    pinMode(GPIO_NUM_12, INPUT); pinMode(GPIO_NUM_13, INPUT);
    pinMode(GPIO_NUM_14, INPUT); pinMode(GPIO_NUM_7, INPUT);
    pinMode(GPIO_NUM_17, INPUT); pinMode(GPIO_NUM_18, INPUT);
    pinMode(GPIO_NUM_3, INPUT);  pinMode(GPIO_NUM_46, INPUT);
    // Sync pins
    pinMode(GPIO_NUM_42, INPUT); pinMode(GPIO_NUM_41, INPUT);
    pinMode(GPIO_NUM_40, INPUT); pinMode(GPIO_NUM_39, INPUT);
    
    // For Advance: also set backlight I2C pins to input
    // (These are different from touch I2C on Advance)
#else
    // Basic pin mappings - set as INPUT to reduce power
    pinMode(GPIO_NUM_15, INPUT); pinMode(GPIO_NUM_7, INPUT);
    pinMode(GPIO_NUM_6, INPUT);  pinMode(GPIO_NUM_5, INPUT);
    pinMode(GPIO_NUM_4, INPUT);  pinMode(GPIO_NUM_9, INPUT);
    pinMode(GPIO_NUM_46, INPUT); pinMode(GPIO_NUM_3, INPUT);
    pinMode(GPIO_NUM_8, INPUT);  pinMode(GPIO_NUM_16, INPUT);
    pinMode(GPIO_NUM_1, INPUT);  pinMode(GPIO_NUM_14, INPUT);
    pinMode(GPIO_NUM_21, INPUT); pinMode(GPIO_NUM_47, INPUT);
    pinMode(GPIO_NUM_48, INPUT); pinMode(GPIO_NUM_45, INPUT);
    // Sync pins
    pinMode(GPIO_NUM_41, INPUT); pinMode(GPIO_NUM_40, INPUT);
    pinMode(GPIO_NUM_39, INPUT); pinMode(GPIO_NUM_0, INPUT);
    
    // For Basic: also disable PWM backlight pin
    pinMode(2, INPUT);
#endif
    
    Serial.println("  Display powered down");
}

