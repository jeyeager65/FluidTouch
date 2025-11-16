#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>

// Settings import/export manager for backup/restore functionality
class SettingsManager {
public:
    // Export all settings to JSON file on Display SD card
    // Returns true on success, false on failure
    static bool exportSettings(const char* filepath = "/fluidtouch_settings.json");
    
    // Import settings from JSON file on Display SD card
    // Returns true on success, false on failure
    static bool importSettings(const char* filepath = "/fluidtouch_settings.json");
    
    // Clear all settings from NVS (both namespaces)
    // Does NOT restart - caller should call ESP.restart() after this
    static void clearAllSettings();
    
    // Check if import file exists on Display SD card
    static bool importFileExists(const char* filepath = "/fluidtouch_settings.json");
    
    // Auto-import on boot (only if no machines configured)
    // Returns true if import was performed, false if skipped
    static bool autoImportOnBoot();
};

#endif // SETTINGS_MANAGER_H
