#include "core/power_manager.h"
#include "network/fluidnc_client.h"
#include "config.h"
#include <Preferences.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

// Static member initialization
DisplayDriver* PowerManager::display_driver = nullptr;
bool PowerManager::enabled = true;
uint32_t PowerManager::dim_timeout_sec = 30;         // Default: dim after 30 seconds
uint32_t PowerManager::sleep_timeout_sec = 300;      // Default: screen off after 5 minutes
uint32_t PowerManager::deep_sleep_timeout_sec = 900; // Default: deep sleep after 15 minutes
uint8_t PowerManager::normal_brightness = 100;       // Default: 100% brightness when active
uint8_t PowerManager::dim_brightness = 25;           // Default: 25% brightness when dimmed
uint32_t PowerManager::last_activity_ms = 0;
PowerManager::PowerState PowerManager::current_state = PowerManager::FULL_BRIGHTNESS;
bool PowerManager::state_changed = false;

void PowerManager::init(DisplayDriver* driver) {
    display_driver = driver;
    last_activity_ms = millis();
    current_state = FULL_BRIGHTNESS;
    loadSettings();
    
    Serial.println("\n=== Power Manager Initialized ===");
    Serial.printf("Enabled: %s\n", enabled ? "YES" : "NO");
    Serial.printf("Dim timeout: %d seconds\n", dim_timeout_sec);
    Serial.printf("Sleep timeout: %d seconds\n", sleep_timeout_sec);
    Serial.printf("Deep sleep timeout: %d seconds\n", deep_sleep_timeout_sec);
    Serial.printf("Normal brightness: %d%%\n", normal_brightness);
    Serial.printf("Dim brightness: %d%%\n", dim_brightness);
    
    // Apply the loaded brightness immediately
    if (display_driver) {
        display_driver->setBacklight(normal_brightness);
        Serial.printf("Applied initial brightness: %d%%\n", normal_brightness);
    }
}

void PowerManager::onUserActivity() {
    last_activity_ms = millis();
    
    // If we were dimmed or screen off, return to full brightness
    if (current_state != FULL_BRIGHTNESS) {
        enterFullBrightness();
    }
}

void PowerManager::update(int machine_state) {
    // Skip if power management disabled
    if (!enabled || display_driver == nullptr) {
        return;
    }
    
    // Power management ONLY applies to IDLE and DISCONNECTED states
    // All other states (RUN, ALARM, HOLD, JOG, etc.) stay at full brightness
    if (machine_state != STATE_IDLE && machine_state != STATE_DISCONNECTED) {
        if (current_state != FULL_BRIGHTNESS) {
            enterFullBrightness();
        }
        // Reset activity timer when not in IDLE/DISCONNECTED to prevent immediate dim when returning
        last_activity_ms = millis();
        return;
    }
    
    // Calculate time since last activity
    uint32_t idle_ms = millis() - last_activity_ms;
    uint32_t idle_sec = idle_ms / 1000;
    
    // Check for deep sleep timeout (if enabled)
    if (deep_sleep_timeout_sec > 0 && idle_sec >= deep_sleep_timeout_sec) {
        enterDeepSleep();
        return;  // Never returns, but good practice
    }
    
    // State machine for screen power management
    switch (current_state) {
        case FULL_BRIGHTNESS:
            // Check if we should dim (only if dimming is enabled)
            if (dim_timeout_sec > 0 && idle_sec >= dim_timeout_sec) {
                enterDimmed();
            }
            break;
            
        case DIMMED:
            // Check if we should turn off screen (only if sleep is enabled)
            if (sleep_timeout_sec > 0 && idle_sec >= sleep_timeout_sec) {
                enterScreenOff();
            }
            break;
            
        case SCREEN_OFF:
            // Stay off until user activity or deep sleep timeout
            break;
    }
}

void PowerManager::loadSettings() {
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    
    enabled = prefs.getBool("pm_enabled", true);
    dim_timeout_sec = prefs.getUInt("pm_dim_to", 30);
    sleep_timeout_sec = prefs.getUInt("pm_sleep_to", 300);
    deep_sleep_timeout_sec = prefs.getUInt("pm_deepsleep", 900);
    normal_brightness = prefs.getUChar("pm_norm_bri", 100);  // 0-100 percentage
    dim_brightness = prefs.getUChar("pm_dim_bri", 25);       // 0-100 percentage
    
    prefs.end();
    
    // Validate ranges (0 = disabled is valid)
    if (dim_timeout_sec > 0 && dim_timeout_sec < 10) dim_timeout_sec = 10;
    if (dim_timeout_sec > 600) dim_timeout_sec = 600;
    if (sleep_timeout_sec > 0 && sleep_timeout_sec < 10) sleep_timeout_sec = 10;
    if (sleep_timeout_sec > 3600) sleep_timeout_sec = 3600;
    // If both dim and sleep are enabled, ensure sleep > dim
    if (dim_timeout_sec > 0 && sleep_timeout_sec > 0 && sleep_timeout_sec < dim_timeout_sec + 10) {
        sleep_timeout_sec = dim_timeout_sec + 10;
    }
    if (deep_sleep_timeout_sec > 0 && deep_sleep_timeout_sec < 300) deep_sleep_timeout_sec = 300;
    if (deep_sleep_timeout_sec > 7200) deep_sleep_timeout_sec = 7200;  // Max 2 hours
    if (normal_brightness > 100) normal_brightness = 100;  // Validate percentage range
    if (dim_brightness > 100) dim_brightness = 25;         // Validate percentage range
}

void PowerManager::saveSettings() {
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, false);  // Read-write
    
    prefs.putBool("pm_enabled", enabled);
    prefs.putUInt("pm_dim_to", dim_timeout_sec);
    prefs.putUInt("pm_sleep_to", sleep_timeout_sec);
    prefs.putUInt("pm_deepsleep", deep_sleep_timeout_sec);
    prefs.putUChar("pm_norm_bri", normal_brightness);
    prefs.putUChar("pm_dim_bri", dim_brightness);
    
    prefs.end();
    
    Serial.println("\n=== Power Manager Settings Saved ===");
    Serial.printf("Enabled: %s\n", enabled ? "YES" : "NO");
    Serial.printf("Dim timeout: %d seconds\n", dim_timeout_sec);
    Serial.printf("Sleep timeout: %d seconds\n", sleep_timeout_sec);
    Serial.printf("Deep sleep timeout: %d seconds\n", deep_sleep_timeout_sec);
    Serial.printf("Normal brightness: %d/255\n", normal_brightness);
    Serial.printf("Dim brightness: %d/255\n", dim_brightness);
}

void PowerManager::setEnabled(bool enable) {
    enabled = enable;
    if (!enabled && current_state != FULL_BRIGHTNESS) {
        // If disabling, restore full brightness
        enterFullBrightness();
    }
}

void PowerManager::setDimTimeout(uint32_t seconds) {
    // 0 = disabled, otherwise must be between 10 and 600 seconds
    if (seconds == 0 || (seconds >= 10 && seconds <= 600)) {
        dim_timeout_sec = seconds;
        // Ensure sleep timeout is always greater (if both are enabled)
        if (dim_timeout_sec > 0 && sleep_timeout_sec > 0 && sleep_timeout_sec < dim_timeout_sec + 10) {
            sleep_timeout_sec = dim_timeout_sec + 10;
        }
    }
}

void PowerManager::setSleepTimeout(uint32_t seconds) {
    // 0 = disabled, otherwise must be greater than dim timeout (if dim is enabled)
    if (seconds == 0 || seconds <= 3600) {
        // If dim is enabled and sleep is enabled, ensure sleep > dim
        if (dim_timeout_sec > 0 && seconds > 0 && seconds < dim_timeout_sec + 10) {
            return;  // Invalid: sleep must be at least 10 seconds after dim
        }
        sleep_timeout_sec = seconds;
        // Ensure deep sleep timeout is always greater (if both are enabled)
        if (sleep_timeout_sec > 0 && deep_sleep_timeout_sec > 0 && deep_sleep_timeout_sec < sleep_timeout_sec + 60) {
            deep_sleep_timeout_sec = sleep_timeout_sec + 60;
        }
    }
}

void PowerManager::setDeepSleepTimeout(uint32_t seconds) {
    // 0 = disabled, otherwise must be greater than sleep timeout
    if (seconds == 0 || (seconds >= sleep_timeout_sec + 60 && seconds <= 7200)) {
        deep_sleep_timeout_sec = seconds;
    }
}

void PowerManager::setNormalBrightness(uint8_t brightness) {
    if (brightness <= 100) {  // Validate percentage range
        normal_brightness = brightness;
        // If currently at full brightness, apply new brightness immediately
        if (current_state == FULL_BRIGHTNESS && display_driver) {
            display_driver->setBacklight(normal_brightness);
        }
    }
}

void PowerManager::setDimBrightness(uint8_t brightness) {
    if (brightness <= 100) {  // Validate percentage range
        dim_brightness = brightness;
        // If currently dimmed, apply new brightness immediately
        if (current_state == DIMMED && display_driver) {
            display_driver->setBacklight(dim_brightness);
        }
    }
}

void PowerManager::applyNormalBrightness() {
    if (display_driver) {
        display_driver->setBacklight(normal_brightness);
        // Also update state to full brightness
        current_state = FULL_BRIGHTNESS;
    }
}

void PowerManager::enterFullBrightness() {
    if (current_state != FULL_BRIGHTNESS) {
        Serial.printf("PowerManager: Entering FULL_BRIGHTNESS (brightness=%d)\n", normal_brightness);
        display_driver->setBacklight(normal_brightness);
        current_state = FULL_BRIGHTNESS;
        state_changed = true;
    }
}

void PowerManager::enterDimmed() {
    if (current_state != DIMMED) {
        Serial.printf("PowerManager: Entering DIMMED (brightness=%d)\n", dim_brightness);
        display_driver->setBacklight(dim_brightness);
        current_state = DIMMED;
        state_changed = true;
    }
}

void PowerManager::enterScreenOff() {
    if (current_state != SCREEN_OFF) {
        Serial.println("PowerManager: Entering SCREEN_OFF");
        display_driver->setBacklightOff();
        current_state = SCREEN_OFF;
        state_changed = true;
    }
}

void PowerManager::enterDeepSleep() {
    Serial.println("PowerManager: Entering DEEP SLEEP due to inactivity");
    
    // Save clean shutdown flag
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, false);
    prefs.putBool("clean_shutdown", true);
    prefs.end();
    
    // Power down display and peripherals (Elecrow-specific)
    display_driver->powerDown();
    delay(100);
    
    // Disconnect WiFi and WebSocket
    FluidNCClient::disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Disable Bluetooth (reduces power even if not used)
    btStop();
    
    // Disable all wakeup sources except reset button
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // Disable RTC peripherals to save power
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    
    // Disable unused GPIO power domains
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    
    // Isolate GPIO pins to prevent current leakage
    // This prevents the display and other peripherals from drawing current
    gpio_deep_sleep_hold_en();
    
    // Enter deep sleep (only reset button can wake)
    Serial.println("Entering deep sleep with maximum power savings...");
    Serial.flush();  // Ensure message is sent
    delay(100);
    
    esp_deep_sleep_start();
    // Never returns
}
