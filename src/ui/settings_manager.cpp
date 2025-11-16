#include "ui/settings_manager.h"
#include "ui/machine_config.h"
#include "ui/upload_manager.h"
#include "ui/tabs/ui_tab_macros.h"
#include "config.h"
#include "core/power_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <SD.h>

// Export all settings to JSON file on Display SD card
bool SettingsManager::exportSettings(const char* filepath) {
    Serial.printf("[SettingsManager] Exporting settings to: %s\n", filepath);
    
    // Initialize SD card if needed
    if (!UploadManager::init()) {
        Serial.println("[SettingsManager] ERROR - Failed to initialize SD card");
        return false;
    }
    
    // Check if SD card is available
    if (SD.cardType() == CARD_NONE) {
        Serial.println("[SettingsManager] ERROR - No SD card detected");
        return false;
    }
    
    // Create JSON document (allocate enough space)
    JsonDocument doc;
    
    // Add metadata
    doc["version"] = "1.0";
    doc["fluidtouch_version"] = FLUIDTOUCH_VERSION;
    
    // Get current timestamp (use millis since we don't have RTC)
    char timestamp[32];
    unsigned long ms = millis();
    snprintf(timestamp, sizeof(timestamp), "uptime_%lu_ms", ms);
    doc["exported"] = timestamp;
    
    // === Export Machines ===
    JsonArray machines = doc["machines"].to<JsonArray>();
    MachineConfig machine_configs[MAX_MACHINES];
    MachineConfigManager::loadMachines(machine_configs);
    
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (!machine_configs[i].is_configured) continue;
        
        JsonObject machine = machines.add<JsonObject>();
        machine["name"] = machine_configs[i].name;
        machine["connection_type"] = (uint8_t)machine_configs[i].connection_type;
        machine["ssid"] = machine_configs[i].ssid;
        machine["password"] = "";  // Password not exported for security
        machine["fluidnc_url"] = machine_configs[i].fluidnc_url;
        machine["websocket_port"] = machine_configs[i].websocket_port;
        
        // Jog settings
        JsonObject jog = machine["jog"].to<JsonObject>();
        jog["xy_feed_rate"] = machine_configs[i].jog_xy_feed;
        jog["z_feed_rate"] = machine_configs[i].jog_z_feed;
        jog["xy_step"] = machine_configs[i].jog_xy_step;
        jog["z_step"] = machine_configs[i].jog_z_step;
        
        // Probe settings
        JsonObject probe = machine["probe"].to<JsonObject>();
        probe["feed_rate"] = machine_configs[i].probe_feed_rate;
        probe["max_distance"] = machine_configs[i].probe_max_distance;
        probe["retract"] = machine_configs[i].probe_retract;
        probe["thickness"] = machine_configs[i].probe_thickness;
        
        // Macros (read from preferences for this machine)
        Preferences prefs;
        prefs.begin(PREFS_NAMESPACE, true);  // Read-only
        
        char key[32];
        snprintf(key, sizeof(key), "m%d_macros", i);
        
        MacroConfig macros[MAX_MACROS];
        size_t size = prefs.getBytesLength(key);
        if (size == sizeof(macros)) {
            prefs.getBytes(key, macros, size);
            
            JsonArray macrosArray = machine["macros"].to<JsonArray>();
            for (int m = 0; m < MAX_MACROS; m++) {
                if (!macros[m].is_configured) continue;
                
                JsonObject macro = macrosArray.add<JsonObject>();
                macro["name"] = macros[m].name;
                macro["file_path"] = macros[m].file_path;
                macro["color_index"] = macros[m].color_index;
            }
        }
        
        prefs.end();
    }
    
    // === Export System Settings ===
    JsonObject system = doc["system"].to<JsonObject>();
    
    // Power management settings
    JsonObject power = system["power"].to<JsonObject>();
    power["enabled"] = PowerManager::isEnabled();
    power["dim_timeout"] = PowerManager::getDimTimeout();
    power["sleep_timeout"] = PowerManager::getSleepTimeout();
    power["deep_sleep_timeout"] = PowerManager::getDeepSleepTimeout();
    power["normal_brightness"] = PowerManager::getNormalBrightness();
    power["dim_brightness"] = PowerManager::getDimBrightness();
    
    // UI preferences
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    JsonObject ui = system["ui"].to<JsonObject>();
    ui["show_machine_select"] = prefs.getBool("show_mach_sel", true);
    ui["folders_on_top"] = prefs.getBool("folders_on_top", false);
    prefs.end();
    
    // Selected machine index
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    system["selected_machine"] = prefs.getInt("sel_machine", 0);
    prefs.end();
    
    // Write to SD card
    File file = SD.open(filepath, FILE_WRITE);
    if (!file) {
        Serial.printf("[SettingsManager] ERROR - Failed to open file for writing: %s\n", filepath);
        return false;
    }
    
    if (serializeJsonPretty(doc, file) == 0) {
        Serial.println("[SettingsManager] ERROR - Failed to write JSON to file");
        file.close();
        return false;
    }
    
    file.close();
    Serial.printf("[SettingsManager] Successfully exported settings to: %s\n", filepath);
    Serial.printf("[SettingsManager] File size: %d bytes\n", SD.open(filepath).size());
    
    return true;
}

// Import settings from JSON file on Display SD card
bool SettingsManager::importSettings(const char* filepath) {
    Serial.printf("[SettingsManager] Importing settings from: %s\n", filepath);
    
    // Initialize SD card if needed
    if (!UploadManager::init()) {
        Serial.println("[SettingsManager] ERROR - Failed to initialize SD card");
        return false;
    }
    
    // Check if SD card is available
    if (SD.cardType() == CARD_NONE) {
        Serial.println("[SettingsManager] ERROR - No SD card detected");
        return false;
    }
    
    // Check if file exists
    if (!SD.exists(filepath)) {
        Serial.printf("[SettingsManager] ERROR - File not found: %s\n", filepath);
        return false;
    }
    
    // Open file
    File file = SD.open(filepath, FILE_READ);
    if (!file) {
        Serial.printf("[SettingsManager] ERROR - Failed to open file: %s\n", filepath);
        return false;
    }
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("[SettingsManager] ERROR - Failed to parse JSON: %s\n", error.c_str());
        return false;
    }
    
    // Validate version
    const char* version = doc["version"] | "unknown";
    Serial.printf("[SettingsManager] Import file version: %s\n", version);
    
    if (strcmp(version, "1.0") != 0) {
        Serial.println("[SettingsManager] WARNING - Unknown version, attempting import anyway");
    }
    
    // === Import Machines ===
    JsonArray machines = doc["machines"];
    if (machines.size() > 0) {
        MachineConfig machine_configs[MAX_MACHINES];
        
        // Initialize all machines as unconfigured
        for (int i = 0; i < MAX_MACHINES; i++) {
            machine_configs[i].is_configured = false;
        }
        
        // Import each machine
        int machine_index = 0;
        for (JsonObject machine : machines) {
            if (machine_index >= MAX_MACHINES) break;
            
            machine_configs[machine_index].is_configured = true;
            strncpy(machine_configs[machine_index].name, machine["name"] | "", sizeof(machine_configs[machine_index].name) - 1);
            machine_configs[machine_index].connection_type = (ConnectionType)(machine["connection_type"] | 0);
            strncpy(machine_configs[machine_index].ssid, machine["ssid"] | "", sizeof(machine_configs[machine_index].ssid) - 1);
            strncpy(machine_configs[machine_index].password, machine["password"] | "", sizeof(machine_configs[machine_index].password) - 1);
            strncpy(machine_configs[machine_index].fluidnc_url, machine["fluidnc_url"] | "", sizeof(machine_configs[machine_index].fluidnc_url) - 1);
            machine_configs[machine_index].websocket_port = machine["websocket_port"] | 81;
            
            // Jog settings
            JsonObject jog = machine["jog"];
            machine_configs[machine_index].jog_xy_feed = jog["xy_feed_rate"] | 1000;
            machine_configs[machine_index].jog_z_feed = jog["z_feed_rate"] | 500;
            machine_configs[machine_index].jog_xy_step = jog["xy_step"] | 10.0f;
            machine_configs[machine_index].jog_z_step = jog["z_step"] | 1.0f;
            
            // Probe settings
            JsonObject probe = machine["probe"];
            machine_configs[machine_index].probe_feed_rate = probe["feed_rate"] | 50;
            machine_configs[machine_index].probe_max_distance = probe["max_distance"] | 50.0f;
            machine_configs[machine_index].probe_retract = probe["retract"] | 2.0f;
            machine_configs[machine_index].probe_thickness = probe["thickness"] | 0.0f;
            
            // Macros
            JsonArray macrosArray = machine["macros"];
            if (macrosArray.size() > 0) {
                MacroConfig macros[MAX_MACROS];
                
                // Initialize all macros as unconfigured
                for (int m = 0; m < MAX_MACROS; m++) {
                    macros[m].is_configured = false;
                }
                
                // Import each macro
                int macro_index = 0;
                for (JsonObject macro : macrosArray) {
                    if (macro_index >= MAX_MACROS) break;
                    
                    macros[macro_index].is_configured = true;
                    strncpy(macros[macro_index].name, macro["name"] | "", sizeof(macros[macro_index].name) - 1);
                    strncpy(macros[macro_index].file_path, macro["file_path"] | "", sizeof(macros[macro_index].file_path) - 1);
                    macros[macro_index].color_index = macro["color_index"] | 0;
                    
                    macro_index++;
                }
                
                // Save macros to preferences
                Preferences prefs;
                prefs.begin(PREFS_NAMESPACE, false);  // Read-write
                
                char key[32];
                snprintf(key, sizeof(key), "m%d_macros", machine_index);
                prefs.putBytes(key, macros, sizeof(macros));
                
                prefs.end();
            }
            
            machine_index++;
        }
        
        // Save all machines
        MachineConfigManager::saveMachines(machine_configs);
        Serial.printf("[SettingsManager] Imported %d machines\n", machine_index);
    }
    
    // === Import System Settings ===
    JsonObject system = doc["system"];
    
    if (!system.isNull()) {
        Preferences prefs;
        
        // Power management
        JsonObject power = system["power"];
        if (!power.isNull()) {
            PowerManager::setEnabled(power["enabled"] | true);
            PowerManager::setDimTimeout(power["dim_timeout"] | 30);
            PowerManager::setSleepTimeout(power["sleep_timeout"] | 300);
            PowerManager::setDeepSleepTimeout(power["deep_sleep_timeout"] | 900);
            PowerManager::setNormalBrightness(power["normal_brightness"] | 100);
            PowerManager::setDimBrightness(power["dim_brightness"] | 25);
            PowerManager::saveSettings();
        }
        
        // UI preferences
        JsonObject ui = system["ui"];
        if (!ui.isNull()) {
            prefs.begin(PREFS_SYSTEM_NAMESPACE, false);  // Read-write
            prefs.putBool("show_mach_sel", ui["show_machine_select"] | true);
            prefs.putBool("folders_on_top", ui["folders_on_top"] | false);
            prefs.end();
        }
        
        // Selected machine
        int sel_machine = system["selected_machine"] | 0;
        prefs.begin(PREFS_NAMESPACE, false);  // Read-write
        prefs.putInt("sel_machine", sel_machine);
        prefs.end();
        
        Serial.println("[SettingsManager] Imported system settings");
    }
    
    Serial.println("[SettingsManager] Import completed successfully");
    Serial.println("[SettingsManager] WARNING: WiFi passwords were NOT imported for security.");
    Serial.println("[SettingsManager] You will need to set WiFi passwords manually for each machine.");
    return true;
}

// Clear all settings from NVS (both namespaces)
void SettingsManager::clearAllSettings() {
    Serial.println("[SettingsManager] Clearing all settings from NVS...");
    
    Preferences prefs;
    
    // Clear main namespace (machines, macros, etc.)
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    Serial.println("[SettingsManager] Cleared PREFS_NAMESPACE");
    
    // Clear system namespace (power, UI settings, etc.)
    prefs.begin(PREFS_SYSTEM_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    Serial.println("[SettingsManager] Cleared PREFS_SYSTEM_NAMESPACE");
    
    Serial.println("[SettingsManager] All settings cleared - restart required");
}

// Check if import file exists on Display SD card
bool SettingsManager::importFileExists(const char* filepath) {
    // Initialize SD card if needed
    if (!UploadManager::init()) {
        return false;
    }
    
    // Check if SD card is available
    if (SD.cardType() == CARD_NONE) {
        return false;
    }
    
    return SD.exists(filepath);
}

// Auto-import on boot (only if no machines configured)
bool SettingsManager::autoImportOnBoot() {
    Serial.println("\n[SettingsManager] Checking for auto-import...");
    
    // Check if any machines are configured
    MachineConfig machines[MAX_MACHINES];
    MachineConfigManager::loadMachines(machines);
    
    bool has_machines = false;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].is_configured) {
            has_machines = true;
            break;
        }
    }
    
    if (has_machines) {
        Serial.println("[SettingsManager] Machines already configured, skipping auto-import");
        return false;
    }
    
    Serial.println("[SettingsManager] No machines configured, checking for import file...");
    
    // Check if import file exists
    if (!importFileExists()) {
        Serial.println("[SettingsManager] No import file found, skipping auto-import");
        return false;
    }
    
    Serial.println("[SettingsManager] Import file found! Attempting auto-import...");
    
    // Attempt import
    if (importSettings()) {
        Serial.println("[SettingsManager] Auto-import successful!");
        return true;
    } else {
        Serial.println("[SettingsManager] Auto-import failed!");
        return false;
    }
}
