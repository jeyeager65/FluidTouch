#include <Arduino.h>
#include <lvgl.h>
#include "display_driver.h"     // Display driver module
#include "touch_driver.h"       // Touch driver module
#include "screenshot_server.h"  // Screenshot web server
#include "ui/ui_splash.h"       // Splash screen module
#include "ui/ui_common.h"       // UI common components (status bar)
#include "ui/ui_tabs.h"         // UI tabs module

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== FluidTouch - LVGL 9 with LovyanGFX ===");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

    // Initialize Display Driver
    Serial.println("Initializing display driver...");
    static DisplayDriver displayDriver;
    if (!displayDriver.init()) {
        Serial.println("ERROR: Failed to initialize display!");
        while (1) delay(1000);
    }
    Serial.println("Display driver initialized successfully");

    // Initialize Touch Driver
    Serial.println("Initializing touch driver...");
    static TouchDriver touchDriver;
    if (!touchDriver.init()) {
        Serial.println("ERROR: Failed to initialize touch!");
        while (1) delay(1000);
    }
    Serial.println("Touch driver initialized successfully");

    // Initialize Screenshot Server (WiFi)
    Serial.println("Initializing screenshot server...");
    ScreenshotServer::init(&displayDriver);
    if (ScreenshotServer::isConnected()) {
        Serial.println("Screenshot server available at: http://" + ScreenshotServer::getIPAddress());
    }

    // Show splash screen
    Serial.println("Showing splash screen...");
    UISplash::show(displayDriver.getDisplay());

    // Initialize UI Common
    UICommon::init(displayDriver.getDisplay());

    // Create CNC Controller UI
    Serial.println("Creating CNC Controller UI...");
    
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a1a), LV_PART_MAIN);

    // Create status bar using UICommon module
    UICommon::createStatusBar();

    // Create all tabs using UITabs module
    UITabs::createTabs();
}

void loop()
{
    // Handle screenshot server web requests
    ScreenshotServer::handleClient();
    
    // Update LVGL tick (CRITICAL for timers and input device polling!)
    static uint32_t lastTick = 0;
    uint32_t currentMillis = millis();
    lv_tick_inc(currentMillis - lastTick);
    lastTick = currentMillis;
    
    lv_timer_handler();
    delay(5);
    
    // Status update every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        Serial.printf("[%lu] LVGL running, Free heap: %d\n", millis()/1000, ESP.getFreeHeap());
    }
}
