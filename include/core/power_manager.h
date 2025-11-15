#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <cstdint>
#include "core/display_driver.h"

// Power management for battery-powered operation
class PowerManager {
public:
    // Initialize power manager with display driver reference
    static void init(DisplayDriver* driver);
    
    // Call this on every touch event to reset inactivity timer
    static void onUserActivity();
    
    // Call this periodically from main loop to handle power management
    static void update(int machine_state);
    
    // Load settings from preferences
    static void loadSettings();
    
    // Save settings to preferences
    static void saveSettings();
    
    // Getters for current settings
    static bool isEnabled() { return enabled; }
    static uint32_t getDimTimeout() { return dim_timeout_sec; }
    static uint32_t getSleepTimeout() { return sleep_timeout_sec; }
    static uint32_t getDeepSleepTimeout() { return deep_sleep_timeout_sec; }
    static uint8_t getNormalBrightness() { return normal_brightness; }  // Returns percentage (0-100)
    static uint8_t getDimBrightness() { return dim_brightness; }        // Returns percentage (0-100)
    
    // Setters for settings
    static void setEnabled(bool enable);
    static void setDimTimeout(uint32_t seconds);
    static void setSleepTimeout(uint32_t seconds);
    static void setDeepSleepTimeout(uint32_t seconds);
    static void setNormalBrightness(uint8_t brightness);  // brightness: 0-100 percentage
    static void setDimBrightness(uint8_t brightness);     // brightness: 0-100 percentage
    
    // Apply current normal brightness immediately (useful after changing settings)
    static void applyNormalBrightness();
    
    // Get current power state for UI feedback
    enum PowerState {
        FULL_BRIGHTNESS,
        DIMMED,
        SCREEN_OFF
    };
    static PowerState getCurrentState() { return current_state; }
    
private:
    static DisplayDriver* display_driver;
    static bool enabled;
    static uint32_t dim_timeout_sec;          // Time until dimming (seconds)
    static uint32_t sleep_timeout_sec;        // Time until screen off (seconds)
    static uint32_t deep_sleep_timeout_sec;   // Time until deep sleep (seconds, 0=disabled)
    static uint8_t normal_brightness;         // Normal brightness percentage (0-100)
    static uint8_t dim_brightness;            // Dim brightness percentage (0-100)
    static uint32_t last_activity_ms;         // Last touch/activity timestamp
    static PowerState current_state;
    static bool state_changed;                // Track if we just changed state
    
    // Internal state management
    static void enterFullBrightness();
    static void enterDimmed();
    static void enterScreenOff();
    static void enterDeepSleep();
};

#endif // POWER_MANAGER_H
