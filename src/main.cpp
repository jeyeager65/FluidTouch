#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Preferences.h>
#include "core/display_driver.h"     // Display driver module
#include "core/touch_driver.h"       // Touch driver module
#include "network/screenshot_server.h"  // Screenshot web server
#include "network/fluidnc_client.h"     // FluidNC WebSocket client
#include "ui/ui_theme.h"        // UI theme colors
#include "ui/ui_splash.h"       // Splash screen module
#include "ui/ui_machine_select.h" // Machine selection screen
#include "ui/ui_common.h"       // UI common components (status bar)
#include "ui/ui_tabs.h"         // UI tabs module
#include "ui/tabs/ui_tab_status.h" // Status tab for updates
#include "ui/tabs/ui_tab_macros.h" // Macros tab for progress updates
#include "ui/tabs/ui_tab_terminal.h" // Terminal tab for updates
#include "ui/tabs/settings/ui_tab_settings_about.h" // About tab for screenshot URL updates
#include "ui/tabs/control/ui_tab_control_actions.h" // Actions tab for pause button updates
#include "ui/tabs/control/ui_tab_control_override.h" // Override tab for updates
#include "ui/machine_config.h"  // Machine configuration manager

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

    // Store display driver reference for later use (screenshot server after WiFi connects)
    UICommon::setDisplayDriver(&displayDriver);

    // Initialize Screenshot Server (WiFi not connected yet, will initialize after machine selection)
    Serial.println("Screenshot server will initialize after WiFi connection...");

    // Initialize FluidNC Client
    Serial.println("Initializing FluidNC client...");
    FluidNCClient::init();

    // Show splash screen
    Serial.println("Showing splash screen...");
    UISplash::show(displayDriver.getDisplay());

    // Check if machine selection should be shown
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    prefs.end();
    
    Serial.printf("Main: show_mach_sel preference = %d\n", show_machine_select);
    
    if (show_machine_select) {
        // Show machine selection screen
        Serial.println("Showing machine selection screen...");
        UIMachineSelect::show(displayDriver.getDisplay());
    } else {
        // Auto-load first configured machine
        Serial.println("Auto-loading first machine...");
        MachineConfig machines[MAX_MACHINES];
        MachineConfigManager::loadMachines(machines);
        
        // Find first configured machine
        int first_machine_index = -1;
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (machines[i].is_configured) {
                first_machine_index = i;
                break;
            }
        }
        
        if (first_machine_index >= 0) {
            // Set as selected machine and initialize UI
            MachineConfigManager::setSelectedMachineIndex(first_machine_index);
            Serial.printf("Auto-selected machine: %s\n", machines[first_machine_index].name);
            
            // Initialize main UI directly
            UICommon::createMainUI();
        } else {
            // No machines configured, show selection screen anyway
            Serial.println("No machines configured, showing selection screen...");
            UIMachineSelect::show(displayDriver.getDisplay());
        }
    }
}

void loop()
{
    // Handle screenshot server web requests
    ScreenshotServer::handleClient();
    
    // Handle FluidNC client WebSocket events
    FluidNCClient::loop();
    
    // Check for connection timeout (non-blocking)
    UICommon::checkConnectionTimeout();
    
    // Update UI from FluidNC status (every 250ms)
    static uint32_t lastUIUpdate = 0;
    uint32_t currentMillis = millis();
    if (currentMillis - lastUIUpdate >= 250) {
        lastUIUpdate = currentMillis;
        
        bool machine_connected = FluidNCClient::isConnected();
        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        
        // Update connection status symbols (always update, even if not connected)
        UICommon::updateConnectionStatus(machine_connected, wifi_connected);
        
        // Update About tab screenshot server URL (in case WiFi status changed)
        UITabSettingsAbout::update();
        
        // Only update other status info if machine is connected
        if (machine_connected) {
            const FluidNCStatus& status = FluidNCClient::getStatus();
        
        // Update status bar
        const char* state_str = "IDLE";
        switch (status.state) {
            case STATE_IDLE: state_str = "IDLE"; break;
            case STATE_RUN: state_str = "RUN"; break;
            case STATE_HOLD: state_str = "HOLD"; break;
            case STATE_JOG: state_str = "JOG"; break;
            case STATE_ALARM: state_str = "ALARM"; break;
            case STATE_DOOR: state_str = "DOOR"; break;
            case STATE_CHECK: state_str = "CHECK"; break;
            case STATE_HOME: state_str = "HOME"; break;
            case STATE_SLEEP: state_str = "SLEEP"; break;
            default: state_str = "DISCONNECTED"; break;
        }
        
        UICommon::updateMachineState(state_str);
        UICommon::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
        UICommon::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
        
        // Check for HOLD/ALARM state and show popups if needed
        UICommon::checkStatePopups(status.state, status.last_message);
        
        // Update Control Actions pause/resume button based on machine state
        UITabControlActions::updatePauseButton(status.state);
        
        // Update Status tab
        UITabStatus::updateState(state_str);
        UITabStatus::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
        UITabStatus::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
        UITabStatus::updateFeedRate(status.feed_rate, status.feed_override);
        UITabStatus::updateRapidOverride(status.rapid_override);
        UITabStatus::updateSpindle(status.spindle_speed, status.spindle_override);
        UITabStatus::updateModalStates(status.modal_wcs, status.modal_plane, status.modal_distance,
                                      status.modal_units, status.modal_motion, status.modal_feedrate,
                                      status.modal_spindle, status.modal_coolant, status.modal_tool);
        UITabStatus::updateFileProgress(status.is_sd_printing, status.sd_percent, 
                                       status.sd_filename, status.sd_elapsed_ms);
        UITabStatus::updateMessage(status.last_message);
        
        // Update Macros tab progress (when SD file is running)
        if (status.is_sd_printing && status.sd_percent > 0) {
            UITabMacros::updateProgress((int)status.sd_percent, status.sd_filename, status.last_message);
            UITabMacros::showProgress();
        } else {
            UITabMacros::hideProgress();
        }
        
        // Update Override tab
        UITabControlOverride::updateValues(status.feed_override, status.rapid_override, status.spindle_override);
        }
    }
    
    // Check machine connection timeout
    UICommon::checkConnectionTimeout();
    
    // Update Terminal tab (batched UI updates every 100ms)
    UITabTerminal::update();
    
    // Update LVGL tick (CRITICAL for timers and input device polling!)
    static uint32_t lastTick = 0;
    lv_tick_inc(currentMillis - lastTick);
    lastTick = currentMillis;
    
    lv_timer_handler();
    delay(5);
    
    // Status update every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        Serial.printf("[%lu] LVGL running, Free heap: %d, FluidNC: %s\n", 
                      millis()/1000, ESP.getFreeHeap(),
                      FluidNCClient::isConnected() ? "Connected" : "Disconnected");
    }
}
