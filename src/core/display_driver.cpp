#include "core/display_driver.h"
#include <esp_heap_caps.h>

// LovyanGFX constructor
LGFX::LGFX(void) {
    {
        auto cfg = _bus_instance.config();
        cfg.panel = &_panel_instance;
        
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
        cfg.freq_write  = 10000000;  // 10MHz - optimal balance (no glitching, good performance)

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
    
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
}

// DisplayDriver constructor
DisplayDriver::DisplayDriver() : disp(nullptr), disp_draw_buf(nullptr), disp_draw_buf2(nullptr) {
}

// Initialize display
bool DisplayDriver::init() {
    // Initialize backlight first
    pinMode(2, OUTPUT);
    ledcSetup(1, 300, 8);
    ledcAttachPin(2, 1);
    ledcWrite(1, 255);
    Serial.println("Backlight ON");
    
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
