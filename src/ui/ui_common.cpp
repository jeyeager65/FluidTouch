#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "ui/ui_machine_select.h"
#include "network/fluidnc_client.h"
#include "core/display_driver.h"
#include "core/power_manager.h"
#include "network/screenshot_server.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_sleep.h>

// Static member initialization
lv_display_t *UICommon::display = nullptr;
DisplayDriver *UICommon::display_driver = nullptr;
lv_obj_t *UICommon::status_bar = nullptr;
lv_obj_t *UICommon::status_bar_left_area = nullptr;
lv_obj_t *UICommon::status_bar_right_area = nullptr;
lv_obj_t *UICommon::machine_select_dialog = nullptr;
lv_obj_t *UICommon::connecting_popup = nullptr;
lv_obj_t *UICommon::connection_error_dialog = nullptr;
lv_obj_t *UICommon::hold_popup = nullptr;
lv_obj_t *UICommon::hold_popup_msg_label = nullptr;
lv_obj_t *UICommon::alarm_popup = nullptr;
lv_obj_t *UICommon::alarm_popup_msg_label = nullptr;
int UICommon::last_popup_state = -1;
bool UICommon::hold_popup_dismissed = false;
bool UICommon::alarm_popup_dismissed = false;
lv_obj_t *UICommon::lbl_modal_states = nullptr;
lv_obj_t *UICommon::lbl_status = nullptr;

// Connection status labels
lv_obj_t *UICommon::lbl_machine_symbol = nullptr;
lv_obj_t *UICommon::lbl_machine_name = nullptr;
lv_obj_t *UICommon::lbl_wifi_symbol = nullptr;
lv_obj_t *UICommon::lbl_wifi_name = nullptr;

// Work Position labels
lv_obj_t *UICommon::lbl_wpos_label = nullptr;
lv_obj_t *UICommon::lbl_wpos_x = nullptr;
lv_obj_t *UICommon::lbl_wpos_y = nullptr;
lv_obj_t *UICommon::lbl_wpos_z = nullptr;
lv_obj_t *UICommon::lbl_wpos_a = nullptr;

// Machine Position labels
lv_obj_t *UICommon::lbl_mpos_label = nullptr;
lv_obj_t *UICommon::lbl_mpos_x = nullptr;
lv_obj_t *UICommon::lbl_mpos_y = nullptr;
lv_obj_t *UICommon::lbl_mpos_z = nullptr;

// Job progress display
lv_obj_t *UICommon::lbl_file_progress_container = nullptr;
lv_obj_t *UICommon::lbl_filename = nullptr;
lv_obj_t *UICommon::bar_progress = nullptr;
lv_obj_t *UICommon::lbl_percent = nullptr;
lv_obj_t *UICommon::lbl_elapsed_time = nullptr;
lv_obj_t *UICommon::lbl_elapsed_unit = nullptr;
lv_obj_t *UICommon::lbl_estimated_time = nullptr;
lv_obj_t *UICommon::lbl_estimated_unit = nullptr;

// Cached values for delta checking
float UICommon::last_wpos_x = -9999.0f;
float UICommon::last_wpos_y = -9999.0f;
float UICommon::last_wpos_z = -9999.0f;
float UICommon::last_wpos_a = -9999.0f;
float UICommon::last_mpos_x = -9999.0f;
float UICommon::last_mpos_y = -9999.0f;
float UICommon::last_mpos_z = -9999.0f;

// Cached system preferences (loaded once at startup)
bool UICommon::enable_a_axis = false;

static char last_machine_state[16] = "";  // Cached state to avoid unnecessary updates
static bool last_machine_connected = false;  // Cached connection status
static bool last_wifi_connected = false;     // Cached WiFi status
static bool last_auto_reporting = false;     // Cached auto-reporting status

// File progress cached values for delta checking
static char last_filename[128] = "";
static float last_percent = -1.0f;
static uint32_t last_elapsed_sec = 0xFFFFFFFF;  // Use seconds for comparison
static uint32_t last_estimated_sec = 0xFFFFFFFF;

// Connection timeout tracking
static uint32_t connection_timeout_start = 0;
static bool connection_timeout_active = false;
static bool connection_error_shown = false;
static bool ever_connected_successfully = false;  // Track if we've connected at least once

// Event handler for status bar left area click (go to Status tab)
static void status_bar_left_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Switch to Status tab (index 0) without animation
        lv_obj_t *tabview = UITabs::getTabview();
        if (tabview) {
            lv_tabview_set_active(tabview, 0, LV_ANIM_OFF);
        }
    }
}

// Event handler for status bar right area click (machine selection/connection status)
static void status_bar_right_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Check if both WiFi and WebSocket are connected
        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        bool ws_connected = FluidNCClient::isConnected();
        
        if (wifi_connected && ws_connected) {
            // Both connected - show restart confirmation dialog
            UICommon::showMachineSelectConfirmDialog();
        } else {
            // One or both disconnected - show connection error dialog
            MachineConfig config;
            if (MachineConfigManager::getSelectedMachine(config)) {
                char error_msg[300];
                if (!wifi_connected && !ws_connected) {
                    snprintf(error_msg, sizeof(error_msg), 
                            "WiFi and machine are disconnected.\n\n%s\n\nClick Connect to reconnect.",
                            config.name);
                } else if (!wifi_connected) {
                    snprintf(error_msg, sizeof(error_msg), 
                            "WiFi is disconnected.\n\n%s\n\nClick Connect to reconnect.",
                            config.name);
                } else {
                    snprintf(error_msg, sizeof(error_msg), 
                            "Machine is disconnected.\n\n%s\nURL: %s:%d\n\nClick Connect to reconnect.",
                            config.name, config.fluidnc_url, config.websocket_port);
                }
                UICommon::showConnectionErrorDialog("Connection Lost", error_msg);
            }
        }
    }
}

// Event handler for confirming machine selection change
static void on_machine_select_confirm(lv_event_t *e) {
    Serial.println("UICommon: Restarting to change machine...");
    UICommon::hideMachineSelectConfirmDialog();
    
    // Show restart message
    lv_obj_t *restart_label = lv_label_create(lv_screen_active());
    lv_label_set_text(restart_label, "Restarting...");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(restart_label, UITheme::UI_INFO, 0);
    lv_obj_center(restart_label);
    
    // Force display update
    lv_timer_handler();
    delay(500);
    
    // Restart the ESP32
    ESP.restart();
}

// Event handler for power off
static void on_power_off_confirm(lv_event_t *e) {
    Serial.println("UICommon: Powering off device...");
    UICommon::hideMachineSelectConfirmDialog();
    
    // Save clean shutdown flag in separate namespace to avoid corrupting machine configs
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, false);
    prefs.putBool("clean_shutdown", true);
    prefs.end();
    
    // Show power off message
    lv_obj_t *poweroff_label = lv_label_create(lv_screen_active());
    lv_label_set_text(poweroff_label, "Powering off...");
    lv_obj_set_style_text_font(poweroff_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(poweroff_label, UITheme::UI_WARNING, 0);
    lv_obj_center(poweroff_label);
    
    // Force display update
    lv_timer_handler();
    delay(1000);
    
    // Disconnect from FluidNC gracefully
    FluidNCClient::disconnect();
    delay(100);
    
    // Power down display and peripherals (Elecrow-specific)
    DisplayDriver *display_driver = UICommon::getDisplayDriver();
    if (display_driver) {
        display_driver->powerDown();
    }
    delay(100);
    
    // Disconnect WiFi cleanly
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Disable Bluetooth (reduces power even if not used)
    btStop();
    
    // Disable all wakeup sources except reset button
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // Disable RTC peripherals to save power
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    // Note: ESP_PD_DOMAIN_RTC_SLOW_MEM and RTC_FAST_MEM removed in IDF 5.3+
    // RTC memory domains are now controlled automatically
    
    // Disable unused GPIO power domains
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    
    // Isolate GPIO pins to prevent current leakage
    gpio_deep_sleep_hold_en();
    
    // Enter deep sleep (only reset button can wake)
    Serial.println("Entering deep sleep with maximum power savings...");
    Serial.flush();
    delay(100);
    
    esp_deep_sleep_start();
}

// Event handler for canceling machine selection change
static void on_machine_select_cancel(lv_event_t *e) {
    Serial.println("UICommon: Machine selection cancelled");
    UICommon::hideMachineSelectConfirmDialog();
}

void UICommon::init(lv_display_t *disp) {
    display = disp;
}

void UICommon::setDisplayDriver(DisplayDriver* driver) {
    display_driver = driver;
}

void UICommon::createMainUI() {
    Serial.println("UICommon: Creating main UI");

    // Load machine-specific preferences now that the machine has been selected
    loadSystemPreferences();
    
    // Get selected machine configuration
    MachineConfig config;
    if (!MachineConfigManager::getSelectedMachine(config)) {
        Serial.println("UICommon: ERROR - No machine selected!");
        return;
    }
    
    // Create main screen first
    lv_obj_t *main_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(main_screen, UITheme::BG_DARKER, LV_PART_MAIN);
    
    // Load the new screen immediately to show user something is happening
    lv_scr_load(main_screen);
    
    // Show connecting popup BEFORE creating UI (faster response)
    // This gives immediate visual feedback while UI is being built
    if (config.connection_type == CONN_WIRELESS && strlen(config.ssid) > 0) {
        showConnectingPopup(config.name, config.ssid);
        lv_refr_now(nullptr);  // Force immediate display update
    } else if (config.connection_type == CONN_WIRED) {
        showConnectingPopup(config.name, nullptr);
        lv_refr_now(nullptr);  // Force immediate display update
    }
    
    // Create status bar
    createStatusBar();
    
    // Create all tabs
    UITabs::createTabs();
    
    // Initialize WiFi connection using machine-specific credentials
    if (config.connection_type == CONN_WIRELESS) {
        if (strlen(config.ssid) > 0) {
            Serial.println("\n=== WiFi Connection ===");
            Serial.printf("Connecting to WiFi: %s\n", config.ssid);
            
            WiFi.mode(WIFI_STA);
            WiFi.setAutoReconnect(false);  // Disable auto-reconnect - user must manually reconnect
            WiFi.begin(config.ssid, config.password);
            
            // Wait for connection with timeout (10 seconds)
            int timeout = 20;
            while (WiFi.status() != WL_CONNECTED && timeout > 0) {
                delay(500);
                Serial.print(".");
                timeout--;
                lv_timer_handler();  // Keep UI responsive
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\nWiFi connected!");
                Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
                
                // Initialize mDNS client stack to enable resolving .local hostnames (like fluidnc.local)
                // Note: MDNS.begin() is required on ESP32 to enable mDNS client queries, not just advertising
                if (MDNS.begin("fluidtouch")) {
                    Serial.println("mDNS client initialized - can now resolve .local hostnames");
                    delay(1000);  // Give mDNS time to fully initialize before attempting queries
                } else {
                    Serial.println("Warning: mDNS client failed to start (.local hostname resolution will not work)");
                }
                
                // Initialize screenshot server now that WiFi is connected
                if (display_driver) {
                    Serial.println("Initializing screenshot server with WiFi connection...");
                    ScreenshotServer::init(display_driver);
                    if (ScreenshotServer::isConnected()) {
                        Serial.println("Screenshot server available at: http://" + ScreenshotServer::getIPAddress());
                    }
                }
                
                // Update WiFi status in status bar
                if (lbl_wifi_name) {
                    lv_label_set_text(lbl_wifi_name, config.ssid);
                }
                if (lbl_wifi_symbol) {
                    lv_obj_set_style_text_color(lbl_wifi_symbol, UITheme::STATE_IDLE, 0);
                }
                
                // Update popup to show machine connection (WiFi is now connected)
                hideConnectingPopup();
                showConnectingPopup(config.name, nullptr);  // nullptr = no WiFi name (already connected)
            } else {
                Serial.println("\nWiFi connection failed!");
                
                // Build error message
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), 
                        "Could not connect to network:\n%s\n\nCheck WiFi settings and try again.",
                        config.ssid);
                
                // Show error dialog
                showConnectionErrorDialog("WiFi Connection Failed", error_msg);
                
                // Disable connection timeout monitoring since we're not attempting machine connection
                connection_timeout_active = false;
                
                // Do NOT attempt machine connection if WiFi failed
                return;
            }
        } else {
            Serial.println("UICommon: Warning - Wireless connection selected but no SSID configured");
            // Popup already shown at start
            // Disable connection timeout monitoring since we're not attempting machine connection
            connection_timeout_active = false;
            return;  // Don't attempt machine connection without WiFi config
        }
    } else {
        Serial.println("UICommon: Wired connection selected, skipping WiFi initialization");
        // Popup already shown at start
    }
    
    // Connect to FluidNC using selected machine
    // Only reached if WiFi connected successfully (or wired connection)
    Serial.printf("UICommon: Connecting to FluidNC at %s:%d\n", 
                 config.fluidnc_url, config.websocket_port);
    FluidNCClient::connect(config);
    
    // Start connection timeout monitoring (10 seconds)
    connection_timeout_start = millis();
    connection_timeout_active = true;
    connection_error_shown = false;
    
    Serial.println("UICommon: Main UI created");
}

void UICommon::createStatusBar() {
    // Debug: Check A-axis state at status bar creation
    bool a_axis_enabled = isAAxisEnabled();
    Serial.printf("[StatusBar] Creating status bar - A-axis enabled: %d\n", a_axis_enabled);

    // Create status bar at bottom (always visible) - 2 lines with CNC info
    status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, SCREEN_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, UITheme::BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_bar, UITheme::BORDER_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 0, LV_PART_MAIN); // No rounded corners
    lv_obj_set_style_pad_all(status_bar, 5, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create left clickable area (goes to Status tab)
    status_bar_left_area = lv_obj_create(status_bar);
    lv_obj_set_size(status_bar_left_area, 550, STATUS_BAR_HEIGHT - 10);  // Left 550px
    lv_obj_align(status_bar_left_area, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_left_area, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_width(status_bar_left_area, 0, 0);
    lv_obj_clear_flag(status_bar_left_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(status_bar_left_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar_left_area, status_bar_left_click_handler, LV_EVENT_CLICKED, NULL);
    
    // Create right clickable area (goes to machine selection with confirmation)
    status_bar_right_area = lv_obj_create(status_bar);
    lv_obj_set_size(status_bar_right_area, 240, STATUS_BAR_HEIGHT - 10);  // Right 240px
    lv_obj_align(status_bar_right_area, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_right_area, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_width(status_bar_right_area, 0, 0);
    lv_obj_clear_flag(status_bar_right_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(status_bar_right_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar_right_area, status_bar_right_click_handler, LV_EVENT_CLICKED, NULL);

    // Left side: Large Status (centered vertically)
    lbl_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_status, "OFFLINE");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_status, UITheme::STATE_ALARM, 0);
    lv_obj_align(lbl_status, LV_ALIGN_LEFT_MID, 5, 0);

    // Middle: Work Position (line 1) and Machine Position (line 2)
    // Work Position - Line 1
    lv_obj_t *lbl_wpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_label, "WPos:");
    lv_obj_set_style_text_font(lbl_wpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_label, UITheme::ACCENT_SECONDARY, 0);  // Teal
    lv_obj_set_style_text_align(lbl_wpos_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(lbl_wpos_label, 60);  // Fixed width for right alignment
    lv_obj_set_pos(lbl_wpos_label, 200, 3);  // Top line

    lbl_wpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_x, "X ----.---");
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_pos(lbl_wpos_x, 270, 3);

    lbl_wpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_y, "Y ----.---");
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(lbl_wpos_y, 380, 3);

    lbl_wpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_z, "Z ----.---");
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(lbl_wpos_z, 490, 3);

    // A-axis position label (conditionally shown when A-axis is enabled)
    // Position at MPos X location (line 2) when A-axis is enabled
    if (isAAxisEnabled()) {
        Serial.println("[StatusBar] Creating A-axis label");
        lbl_wpos_a = lv_label_create(status_bar);
        lv_label_set_text(lbl_wpos_a, "A ----.---");
        lv_obj_set_style_text_font(lbl_wpos_a, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_wpos_a, UITheme::AXIS_A, 0);
        lv_obj_set_pos(lbl_wpos_a, 270, 27);  // Line 2, where MPos X would be
    }

    // Machine Position - Line 2
    // Only create when A-axis is disabled (no room for both)
    if (!isAAxisEnabled()) {
        lbl_mpos_label = lv_label_create(status_bar);
        lv_label_set_text(lbl_mpos_label, "MPos:");
        lv_obj_set_style_text_font(lbl_mpos_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_mpos_label, UITheme::POS_MACHINE, 0);  // Cyan - secondary data
        lv_obj_set_style_text_align(lbl_mpos_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_width(lbl_mpos_label, 60);  // Fixed width for right alignment
        lv_obj_set_pos(lbl_mpos_label, 200, 27);  // Bottom line, aligned with WiFi name

        lbl_mpos_x = lv_label_create(status_bar);
        lv_label_set_text(lbl_mpos_x, "X ----.---");
        lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
        lv_obj_set_pos(lbl_mpos_x, 270, 27);

        lbl_mpos_y = lv_label_create(status_bar);
        lv_label_set_text(lbl_mpos_y, "Y ----.---");
        lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
        lv_obj_set_pos(lbl_mpos_y, 380, 27);

        lbl_mpos_z = lv_label_create(status_bar);
        lv_label_set_text(lbl_mpos_z, "Z ----.---");
        lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
        lv_obj_set_pos(lbl_mpos_z, 490, 27);

        Serial.println("[StatusBar] Created MPos labels (A-axis disabled)");
    } else {
        Serial.println("[StatusBar] Skipped MPos labels creation (A-axis enabled)");
    }

    // Job Progress Container (hidden by default, shown when printing)
    lbl_file_progress_container = lv_obj_create(status_bar);
    lv_obj_set_size(lbl_file_progress_container, 550, 50);  // Span from col 2 to col 4
    lv_obj_set_pos(lbl_file_progress_container, 5, 0);
    lv_obj_set_style_bg_color(lbl_file_progress_container, UITheme::BG_DARK, 0);  // Match status bar background
    lv_obj_set_style_border_width(lbl_file_progress_container, 0, 0);  // No border
    lv_obj_set_style_pad_all(lbl_file_progress_container, 5, 0);
    lv_obj_clear_flag(lbl_file_progress_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    lv_obj_add_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Filename label
    lbl_filename = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_filename, "filename.gcode");
    lv_obj_set_style_text_font(lbl_filename, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_filename, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_filename, 0, 0);
    lv_label_set_long_mode(lbl_filename, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_filename, 350);  // Truncate long filenames
    
    // Progress bar (expanded 80px wider: 250→330, 2px taller: 13→15, black background)
    bar_progress = lv_bar_create(lbl_file_progress_container);
    lv_obj_set_size(bar_progress, 300, 15);  // 2px taller: 13→15
    lv_obj_set_pos(bar_progress, 0, 20);
    lv_obj_set_style_bg_color(bar_progress, UITheme::BG_PROGRESS, LV_PART_MAIN);  // Black for incomplete
    lv_obj_set_style_bg_color(bar_progress, UITheme::UI_SUCCESS, LV_PART_INDICATOR);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    
    // Percentage label (next to progress bar)
    lbl_percent = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_percent, "0.0%");
    lv_obj_set_style_text_font(lbl_percent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_percent, UITheme::UI_SUCCESS, 0);
    lv_obj_set_pos(lbl_percent, 305, 20);
    
    // Elapsed time - split into label, value, unit (moved 85px right for 5px more space)
    lv_obj_t *elapsed_label = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(elapsed_label, "Elapsed");
    lv_obj_set_style_text_font(elapsed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(elapsed_label, UITheme::UI_INFO, 0);  // Colored label
    lv_obj_set_style_text_align(elapsed_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(elapsed_label, 355, 2);  // 5px more space (390→385)
    lv_obj_set_width(elapsed_label, 75);  // 5px wider (60→65)
    
    lbl_elapsed_time = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_elapsed_time, "00:00");
    lv_obj_set_style_text_font(lbl_elapsed_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_elapsed_time, UITheme::TEXT_LIGHT, 0);  // White value
    lv_obj_set_pos(lbl_elapsed_time, 435, 2);
    
    lbl_elapsed_unit = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_elapsed_unit, "min:sec");  // Changed from mm:ss
    lv_obj_set_style_text_font(lbl_elapsed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_elapsed_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_elapsed_unit, 480, 2);
    
    // Estimated time - split into label, value, unit
    lv_obj_t *estimated_label = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(estimated_label, "Estimated");
    lv_obj_set_style_text_font(estimated_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(estimated_label, UITheme::UI_WARNING, 0);  // Colored label
    lv_obj_set_style_text_align(estimated_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(estimated_label, 355, 21);  // 5px more space (390→385)
    lv_obj_set_width(estimated_label, 75);  // 5px wider (60→65)
    
    lbl_estimated_time = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_estimated_time, "00:00");
    lv_obj_set_style_text_font(lbl_estimated_time, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_estimated_time, UITheme::TEXT_LIGHT, 0);  // White value
    lv_obj_set_pos(lbl_estimated_time, 435, 21);
    
    lbl_estimated_unit = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_estimated_unit, "min:sec");  // Changed from mm:ss
    lv_obj_set_style_text_font(lbl_estimated_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_estimated_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_estimated_unit, 480, 22);

    // Right side Line 1: Machine name with symbol
    // Get selected machine from config manager
    MachineConfig selected_machine;
    
    if (MachineConfigManager::getSelectedMachine(selected_machine)) {
        const char *symbol = (selected_machine.connection_type == CONN_WIRELESS) ? LV_SYMBOL_WIFI : LV_SYMBOL_USB;
        
        // Symbol (will be colored based on connection status)
        lbl_machine_symbol = lv_label_create(status_bar);
        lv_label_set_text(lbl_machine_symbol, symbol);
        lv_obj_set_style_text_font(lbl_machine_symbol, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_machine_symbol, UITheme::STATE_ALARM, 0);  // Start red
        lv_obj_align(lbl_machine_symbol, LV_ALIGN_TOP_RIGHT, -5, 3);
        
        // Machine name
        lbl_machine_name = lv_label_create(status_bar);
        lv_label_set_text(lbl_machine_name, selected_machine.name);
        lv_obj_set_style_text_font(lbl_machine_name, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_machine_name, UITheme::ACCENT_PRIMARY, 0);
        lv_obj_align_to(lbl_machine_name, lbl_machine_symbol, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    } else {
        lbl_modal_states = lv_label_create(status_bar);
        lv_label_set_text(lbl_modal_states, "No Machine");
        lv_obj_set_style_text_font(lbl_modal_states, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_modal_states, UITheme::ACCENT_PRIMARY, 0);
        lv_obj_align(lbl_modal_states, LV_ALIGN_TOP_RIGHT, -5, 3);
    }

    // Right side Line 2: WiFi network
    // WiFi network name (right-aligned, positioned first)
    // If WiFi connected, show actual SSID. Otherwise, show configured SSID from machine settings
    String wifi_ssid;
    if (WiFi.isConnected()) {
        wifi_ssid = WiFi.SSID();
    } else {
        // Get configured SSID from machine settings
        MachineConfig config;
        if (MachineConfigManager::getSelectedMachine(config) && config.ssid[0] != '\0') {
            wifi_ssid = String(config.ssid);
        } else {
            wifi_ssid = "Not Connected";
        }
    }
    
    lbl_wifi_name = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi_name, wifi_ssid.c_str());
    lv_obj_set_style_text_font(lbl_wifi_name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wifi_name, UITheme::UI_INFO, 0);
    lv_obj_set_style_text_align(lbl_wifi_name, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(lbl_wifi_name, 180);  // Set fixed width for right alignment
    lv_obj_align(lbl_wifi_name, LV_ALIGN_BOTTOM_RIGHT, -32, -3);  // 32px from right for symbol (2px spacing)
    
    // WiFi symbol (will be colored based on connection status)
    lbl_wifi_symbol = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi_symbol, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi_symbol, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wifi_symbol, WiFi.isConnected() ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
    lv_obj_align(lbl_wifi_symbol, LV_ALIGN_BOTTOM_RIGHT, -5, -3);
}

void UICommon::updateModalStates(const char *text) {
    // Check if status bar exists and label is valid
    if (status_bar && lbl_modal_states) {
        lv_label_set_text(lbl_modal_states, text);
    }
}

void UICommon::updateMachinePosition(float x, float y, float z) {
    // Don't update machine position when A-axis is enabled (MPos is hidden)
    if (isAAxisEnabled()) {
        return;
    }

    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_mpos_x && y == last_mpos_y && z == last_mpos_z) {
        return;
    }

    char buf[16];
    
    if (status_bar && lbl_mpos_x && x != last_mpos_x) {
        if (x <= -9999.0f) {
            lv_label_set_text(lbl_mpos_x, "X ----.---");
        } else {
            snprintf(buf, sizeof(buf), "X %04.3f", x);
            lv_label_set_text(lbl_mpos_x, buf);
        }
        last_mpos_x = x;
    }
    if (status_bar && lbl_mpos_y && y != last_mpos_y) {
        if (y <= -9999.0f) {
            lv_label_set_text(lbl_mpos_y, "Y ----.---");
        } else {
            snprintf(buf, sizeof(buf), "Y %04.3f", y);
            lv_label_set_text(lbl_mpos_y, buf);
        }
        last_mpos_y = y;
    }
    if (status_bar && lbl_mpos_z && z != last_mpos_z) {
        if (z <= -9999.0f) {
            lv_label_set_text(lbl_mpos_z, "Z ----.---");
        } else {
            snprintf(buf, sizeof(buf), "Z %04.3f", z);
            lv_label_set_text(lbl_mpos_z, buf);
        }
        last_mpos_z = z;
    }
}

void UICommon::updateWorkPosition(float x, float y, float z, float a) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_wpos_x && y == last_wpos_y && z == last_wpos_z && a == last_wpos_a) {
        return;
    }

    char buf[16];

    if (status_bar && lbl_wpos_x && x != last_wpos_x) {
        if (x <= -9999.0f) {
            lv_label_set_text(lbl_wpos_x, "X ----.---");
        } else {
            snprintf(buf, sizeof(buf), "X %04.3f", x);
            lv_label_set_text(lbl_wpos_x, buf);
        }
        last_wpos_x = x;
    }
    if (status_bar && lbl_wpos_y && y != last_wpos_y) {
        if (y <= -9999.0f) {
            lv_label_set_text(lbl_wpos_y, "Y ----.---");
        } else {
            snprintf(buf, sizeof(buf), "Y %04.3f", y);
            lv_label_set_text(lbl_wpos_y, buf);
        }
        last_wpos_y = y;
    }
    if (status_bar && lbl_wpos_z && z != last_wpos_z) {
        if (z <= -9999.0f) {
            lv_label_set_text(lbl_wpos_z, "Z ----.---");
        } else {
            snprintf(buf, sizeof(buf), "Z %04.3f", z);
            lv_label_set_text(lbl_wpos_z, buf);
        }
        last_wpos_z = z;
    }
    // Update A-axis if enabled and value changed
    if (status_bar && lbl_wpos_a && a != last_wpos_a && a > -9999.0f) {
        snprintf(buf, sizeof(buf), "A %04.3f", a);
        lv_label_set_text(lbl_wpos_a, buf);
        last_wpos_a = a;
    }
}

void UICommon::updateMachineState(const char *state) {
    if (status_bar && lbl_status) {
        // Only update if state actually changed
        if (strcmp(state, last_machine_state) == 0) {
            return;  // No change, skip update
        }
        
        lv_label_set_text(lbl_status, state);
        
        // Update cached state
        strncpy(last_machine_state, state, sizeof(last_machine_state) - 1);
        last_machine_state[sizeof(last_machine_state) - 1] = '\0';
        
        // Color code the status (state is already uppercase)
        if (strcmp(state, "IDLE") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0);
        } else if (strcmp(state, "RUN") == 0 || strcmp(state, "JOG") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_RUN, 0);
        } else if (strcmp(state, "ALARM") == 0 || strcmp(state, "OFFLINE") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_ALARM, 0);
        } else {
            lv_obj_set_style_text_color(lbl_status, UITheme::UI_WARNING, 0);
        }
    }
}

void UICommon::updateConnectionStatus(bool machine_connected, bool wifi_connected) {
    bool auto_reporting = FluidNCClient::isAutoReporting();
    
    // Update machine symbol color with three states:
    // - Red (STATE_ALARM): Disconnected
    // - Orange (UI_WARNING): Connected with fallback polling (degraded)
    // - Green (STATE_IDLE): Connected with auto-reporting (optimal)
    if (lbl_machine_symbol && (machine_connected != last_machine_connected || auto_reporting != last_auto_reporting)) {
        lv_color_t color;
        if (!machine_connected) {
            color = UITheme::STATE_ALARM;  // Red: Disconnected
        } else if (!auto_reporting) {
            color = UITheme::UI_WARNING;   // Orange: Fallback polling (1s updates)
        } else {
            color = UITheme::STATE_IDLE;   // Green: Auto-reporting (250ms updates)
        }
        lv_obj_set_style_text_color(lbl_machine_symbol, color, 0);
        last_machine_connected = machine_connected;
        last_auto_reporting = auto_reporting;
    }
    
    // Update WiFi symbol color
    if (lbl_wifi_symbol && wifi_connected != last_wifi_connected) {
        lv_obj_set_style_text_color(lbl_wifi_symbol, 
            wifi_connected ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
        last_wifi_connected = wifi_connected;
    }
}

void UICommon::showMachineSelectConfirmDialog() {
    // Create modal background
    machine_select_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(machine_select_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(machine_select_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(machine_select_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(machine_select_dialog, 0, 0);
    lv_obj_clear_flag(machine_select_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box (same size as HOLD/ALARM popups)
    lv_obj_t *content = lv_obj_create(machine_select_dialog);
    lv_obj_set_size(content, 600, 300);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title (positioned near top)
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " System Options");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    // Check if power management is enabled
    bool power_mgmt_enabled = PowerManager::isEnabled();
    
    // Message (centered vertically in available space)
    lv_obj_t *msg_label = lv_label_create(content);
    if (power_mgmt_enabled) {
        lv_label_set_text(msg_label, "Restart to change machines,\nor power off for battery operation.\n\nNote: Press reset button to power on\nafter using power off.");
    } else {
        lv_label_set_text(msg_label, "Restart to change machines.");
    }
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, 560);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 50);
    
    // Button container (positioned at bottom)
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_pad_gap(btn_container, 10, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Restart button (adjust size based on number of buttons)
    lv_obj_t *restart_btn = lv_btn_create(btn_container);
    lv_obj_set_size(restart_btn, power_mgmt_enabled ? 165 : 250, 50);
    lv_obj_set_style_bg_color(restart_btn, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(restart_btn, on_machine_select_confirm, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, LV_SYMBOL_REFRESH " Restart");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_18, 0);
    lv_obj_center(restart_label);
    
    // Power off button (only show if power management is enabled)
    if (power_mgmt_enabled) {
        lv_obj_t *poweroff_btn = lv_btn_create(btn_container);
        lv_obj_set_size(poweroff_btn, 165, 50);
        lv_obj_set_style_bg_color(poweroff_btn, lv_color_make(180, 60, 0), 0);  // Orange/red for power off
        lv_obj_add_event_cb(poweroff_btn, on_power_off_confirm, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t *poweroff_label = lv_label_create(poweroff_btn);
        lv_label_set_text(poweroff_label, LV_SYMBOL_POWER " Power Off");
        lv_obj_set_style_text_font(poweroff_label, &lv_font_montserrat_18, 0);
        lv_obj_center(poweroff_label);
    }
    
    // Cancel button (adjust size based on number of buttons)
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, power_mgmt_enabled ? 165 : 250, 50);
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, on_machine_select_cancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
}

void UICommon::showPowerOffConfirmDialog() {
    // Create modal background
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(dialog);
    lv_obj_set_size(content, 520, 220);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, lv_color_make(180, 60, 0), 0);  // Orange
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Icon and title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_POWER " Power Off");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_make(180, 60, 0), 0);  // Orange
    
    // Message
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, "Power off the controller?\nPress reset button to wake.");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Button container (horizontal)
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_pad_gap(btn_container, 15, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Power Off button
    lv_obj_t *poweroff_btn = lv_btn_create(btn_container);
    lv_obj_set_size(poweroff_btn, 200, 50);
    lv_obj_set_style_bg_color(poweroff_btn, lv_color_make(180, 60, 0), 0);  // Orange
    lv_obj_add_event_cb(poweroff_btn, [](lv_event_t *e) {
        lv_obj_t *dialog = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_del(dialog);
        on_power_off_confirm(e);
    }, LV_EVENT_CLICKED, dialog);
    
    lv_obj_t *poweroff_label = lv_label_create(poweroff_btn);
    lv_label_set_text(poweroff_label, LV_SYMBOL_POWER " Power Off");
    lv_obj_set_style_text_font(poweroff_label, &lv_font_montserrat_18, 0);
    lv_obj_center(poweroff_label);
    
    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 200, 50);
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *e) {
        lv_obj_t *dialog = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_del(dialog);
    }, LV_EVENT_CLICKED, dialog);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
}

void UICommon::hideMachineSelectConfirmDialog() {
    if (machine_select_dialog) {
        lv_obj_del(machine_select_dialog);
        machine_select_dialog = nullptr;
    }
}

void UICommon::showConnectingPopup(const char *machine_name, const char *ssid) {
    // Create modal background
    connecting_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(connecting_popup, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(connecting_popup, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(connecting_popup, LV_OPA_70, 0);
    lv_obj_set_style_border_width(connecting_popup, 0, 0);
    lv_obj_clear_flag(connecting_popup, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(connecting_popup);
    lv_obj_set_size(content, 600, 200);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 25, 0);
    lv_obj_set_style_pad_gap(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Connection text - use ssid if provided, otherwise machine name
    // Add "..." to indicate activity without spinner animation
    char conn_text[128];
    if (ssid && strlen(ssid) > 0) {
        snprintf(conn_text, sizeof(conn_text), "Connecting to %s...", ssid);
    } else {
        snprintf(conn_text, sizeof(conn_text), "Connecting to %s...", machine_name);
    }
    
    lv_obj_t *conn_label = lv_label_create(content);
    lv_label_set_text(conn_label, conn_text);
    lv_obj_set_style_text_font(conn_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(conn_label, UITheme::TEXT_LIGHT, 0);
    
    Serial.printf("UICommon: %s\n", conn_text);
}

void UICommon::hideConnectingPopup() {
    if (connecting_popup) {
        lv_obj_del(connecting_popup);
        connecting_popup = nullptr;
        Serial.println("UICommon: Connecting popup hidden");
    }
}

// Event handlers for connection error dialog
static void on_connection_error_close(lv_event_t *e) {
    Serial.println("UICommon: Connection error dialog closed");
    UICommon::hideConnectionErrorDialog();
}

static void on_connection_error_connect(lv_event_t *e) {
    Serial.println("UICommon: Reconnecting WiFi and WebSocket...");
    UICommon::hideConnectionErrorDialog();
    
    // Get machine config
    MachineConfig config;
    if (!MachineConfigManager::getSelectedMachine(config)) {
        Serial.println("UICommon: Error - No machine selected!");
        return;
    }
    
    // Show connecting popup
    UICommon::showConnectingPopup(config.name, config.ssid);
    lv_refr_now(nullptr);  // Force immediate display update
    
    // Disconnect existing connections
    WiFi.disconnect();
    FluidNCClient::disconnect();
    
    // Small delay to ensure clean disconnect
    delay(100);
    lv_timer_handler();
    
    // Reconnect WiFi
    if (config.connection_type == CONN_WIRELESS && strlen(config.ssid) > 0) {
        Serial.printf("UICommon: Reconnecting to WiFi: %s\n", config.ssid);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.begin(config.ssid, config.password);
        
        // Wait for WiFi connection with timeout (10 seconds)
        int timeout = 20;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
            delay(500);
            Serial.print(".");
            timeout--;
            lv_timer_handler();
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi reconnected!");
            Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
            
            // Update status bar WiFi indicator
            UICommon::updateConnectionStatus(false, true);
            
            // Update connecting popup to show machine connection
            UICommon::hideConnectingPopup();
            UICommon::showConnectingPopup(config.name, nullptr);
            
            // Reconnect to FluidNC
            Serial.printf("UICommon: Reconnecting to FluidNC at %s:%d\n", 
                         config.fluidnc_url, config.websocket_port);
            FluidNCClient::connect(config);
            
            // Restart connection timeout monitoring (10 seconds)
            connection_timeout_start = millis();
            connection_timeout_active = true;
            connection_error_shown = false;
        } else {
            Serial.println("\nWiFi reconnection failed!");
            UICommon::hideConnectingPopup();
            UICommon::showConnectionErrorDialog("WiFi Connection Failed", 
                "Could not reconnect to WiFi.\n\nCheck network settings and try again.");
        }
    }
}

static void on_connection_error_restart(lv_event_t *e) {
    Serial.println("UICommon: Restarting to change machine...");
    UICommon::hideConnectionErrorDialog();
    
    // Show restart message
    lv_obj_t *restart_label = lv_label_create(lv_screen_active());
    lv_label_set_text(restart_label, "Restarting...");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(restart_label, UITheme::UI_INFO, 0);
    lv_obj_center(restart_label);
    
    // Force display update
    lv_timer_handler();
    delay(500);
    
    // Restart the ESP32
    ESP.restart();
}

void UICommon::showConnectionErrorDialog(const char *title, const char *message) {
    // Hide connecting popup if it's showing
    hideConnectingPopup();
    
    // Stop FluidNC reconnection attempts
    FluidNCClient::stopReconnectionAttempts();
    
    // Create modal background
    connection_error_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(connection_error_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(connection_error_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(connection_error_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(connection_error_dialog, 0, 0);
    lv_obj_clear_flag(connection_error_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box (consistent with System Options)
    lv_obj_t *content = lv_obj_create(connection_error_dialog);
    lv_obj_set_size(content, 600, 300);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title (positioned near top, consistent with System Options)
    lv_obj_t *title_label = lv_label_create(content);
    char title_text[128];
    snprintf(title_text, sizeof(title_text), LV_SYMBOL_WARNING " %s", title);
    lv_label_set_text(title_label, title_text);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title_label, UITheme::STATE_ALARM, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // Error message (centered vertically in available space)
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, 560);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 50);
    
    // Button container (positioned at bottom, consistent with System Options)
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_pad_gap(btn_container, 10, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Connect button (left)
    lv_obj_t *connect_btn = lv_btn_create(btn_container);
    lv_obj_set_size(connect_btn, 165, 50);
    lv_obj_set_style_bg_color(connect_btn, UITheme::BTN_CONNECT, 0);
    lv_obj_add_event_cb(connect_btn, on_connection_error_connect, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, LV_SYMBOL_REFRESH " Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_16, 0);
    lv_obj_center(connect_label);
    
    // Restart button (center)
    lv_obj_t *restart_btn = lv_btn_create(btn_container);
    lv_obj_set_size(restart_btn, 165, 50);
    lv_obj_set_style_bg_color(restart_btn, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(restart_btn, on_connection_error_restart, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, LV_SYMBOL_POWER " Restart");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_16, 0);
    lv_obj_center(restart_label);
    
    // Close button (right)
    lv_obj_t *close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 165, 50);
    lv_obj_set_style_bg_color(close_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(close_btn, on_connection_error_close, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_18, 0);
    lv_obj_center(close_label);
    
    Serial.printf("UICommon: Connection error dialog shown - %s: %s\n", title, message);
}

void UICommon::hideConnectionErrorDialog() {
    if (connection_error_dialog) {
        lv_obj_del(connection_error_dialog);
        connection_error_dialog = nullptr;
        Serial.println("UICommon: Connection error dialog hidden");
    }
}

void UICommon::checkConnectionTimeout() {
    // If not monitoring timeout, nothing to do
    if (!connection_timeout_active) {
        return;
    }
    
    // If already connected, deactivate timeout and hide popups
    if (FluidNCClient::isConnected()) {
        connection_timeout_active = false;
        connection_error_shown = false;
        ever_connected_successfully = true;  // Mark that we've connected successfully
        hideConnectingPopup();  // Hide connecting popup when connected
        hideConnectionErrorDialog();
        return;
    }
    
    // Check if timeout exceeded (10 seconds)
    uint32_t elapsed = millis() - connection_timeout_start;
    if (elapsed >= 10000 && !connection_error_shown && !ever_connected_successfully) {
        // Only show error if we've never connected successfully (prevents popup on brief disconnects after initial connection)
        connection_error_shown = true;
        
        // Get machine config for error message
        MachineConfig config;
        if (MachineConfigManager::getSelectedMachine(config)) {
            // Build error message
            char error_msg[300];
            snprintf(error_msg, sizeof(error_msg), 
                    "Could not connect to machine:\n%s\n\nURL: %s:%d\n\nCheck that the machine is powered on\nand network connection is available.",
                    config.name, config.fluidnc_url, config.websocket_port);
            
            // Show error dialog
            showConnectionErrorDialog("Machine Connection Failed", error_msg);
            
            Serial.println("UICommon: Machine connection timeout - showing error dialog");
        }
    }
}

// HOLD popup - shown when machine enters HOLD state
void UICommon::showHoldPopup(const char *message) {
    if (hold_popup) return; // Already showing
    
    // Create modal backdrop
    hold_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(hold_popup, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(hold_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hold_popup, LV_OPA_50, 0);
    lv_obj_set_style_border_width(hold_popup, 0, 0);
    lv_obj_clear_flag(hold_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(hold_popup);
    
    // Create dialog box
    lv_obj_t *dialog = lv_obj_create(hold_popup);
    lv_obj_set_size(dialog, 600, 300);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::STATE_HOLD, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dialog);
    
    // State label (large, colored)
    lv_obj_t *state_label = lv_label_create(dialog);
    lv_label_set_text(state_label, "HOLD");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(state_label, UITheme::STATE_HOLD, 0);
    lv_obj_align(state_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // Message label
    hold_popup_msg_label = lv_label_create(dialog);
    lv_label_set_text(hold_popup_msg_label, message && strlen(message) > 0 ? message : "Machine paused");
    lv_obj_set_style_text_font(hold_popup_msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(hold_popup_msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_width(hold_popup_msg_label, 520);
    lv_label_set_long_mode(hold_popup_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(hold_popup_msg_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Resume button
    lv_obj_t *resume_btn = lv_btn_create(btn_container);
    lv_obj_set_size(resume_btn, 250, 50);
    lv_obj_set_style_bg_color(resume_btn, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(resume_btn, [](lv_event_t *e) {
        FluidNCClient::sendCommand("~"); // Send cycle start (resume)
        // Clear the last message so it doesn't persist after resuming
        FluidNCClient::clearLastMessage();
        if (hold_popup_msg_label) lv_label_set_text(hold_popup_msg_label, "");
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *resume_label = lv_label_create(resume_btn);
    lv_label_set_text(resume_label, LV_SYMBOL_PLAY " Resume");
    lv_obj_set_style_text_font(resume_label, &lv_font_montserrat_18, 0);
    lv_obj_center(resume_label);
    
    // Close button
    lv_obj_t *close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 250, 50);
    lv_obj_set_style_bg_color(close_btn, UITheme::BG_MEDIUM, 0);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) {
        UICommon::hold_popup_dismissed = true;  // Mark as dismissed
        UICommon::hideHoldPopup();
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE " Close");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_18, 0);
    lv_obj_center(close_label);
    
    Serial.println("UICommon: HOLD popup shown");
}

void UICommon::hideHoldPopup() {
    if (hold_popup) {
        lv_obj_del(hold_popup);
        hold_popup = nullptr;
        hold_popup_msg_label = nullptr;
        Serial.println("UICommon: HOLD popup hidden");
    }
}

// ALARM popup - shown when machine enters ALARM state
void UICommon::showAlarmPopup(const char *message) {
    if (alarm_popup) return; // Already showing
    
    // Create modal backdrop
    alarm_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(alarm_popup, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(alarm_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(alarm_popup, LV_OPA_50, 0);
    lv_obj_set_style_border_width(alarm_popup, 0, 0);
    lv_obj_clear_flag(alarm_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(alarm_popup);
    
    // Create dialog box
    lv_obj_t *dialog = lv_obj_create(alarm_popup);
    lv_obj_set_size(dialog, 600, 300);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dialog);
    
    // State label (large, colored)
    lv_obj_t *state_label = lv_label_create(dialog);
    lv_label_set_text(state_label, "ALARM");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(state_label, UITheme::STATE_ALARM, 0);
    lv_obj_align(state_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // Message label
    alarm_popup_msg_label = lv_label_create(dialog);
    lv_label_set_text(alarm_popup_msg_label, message && strlen(message) > 0 ? message : "Alarm condition detected");
    lv_obj_set_style_text_font(alarm_popup_msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(alarm_popup_msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_width(alarm_popup_msg_label, 520);
    lv_label_set_long_mode(alarm_popup_msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(alarm_popup_msg_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // Clear Alarm button
    lv_obj_t *clear_btn = lv_btn_create(btn_container);
    lv_obj_set_size(clear_btn, 250, 50);
    lv_obj_set_style_bg_color(clear_btn, UITheme::UI_WARNING, 0);
    lv_obj_add_event_cb(clear_btn, [](lv_event_t *e) {
        // Send soft reset, then unlock
        FluidNCClient::sendCommand("\x18"); // Ctrl-X (soft reset)
        delay(100);
        FluidNCClient::sendCommand("$X\n");   // Unlock
        // Clear the last message so it doesn't persist after clearing the alarm
        FluidNCClient::clearLastMessage();
        if (alarm_popup_msg_label) lv_label_set_text(alarm_popup_msg_label, "");
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, LV_SYMBOL_REFRESH " Clear Alarm");
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_18, 0);
    lv_obj_center(clear_label);
    
    // Close button
    lv_obj_t *close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 250, 50);
    lv_obj_set_style_bg_color(close_btn, UITheme::BG_MEDIUM, 0);
    lv_obj_add_event_cb(close_btn, [](lv_event_t *e) {
        UICommon::alarm_popup_dismissed = true;  // Mark as dismissed
        UICommon::hideAlarmPopup();
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE " Close");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_18, 0);
    lv_obj_center(close_label);
    
    Serial.println("UICommon: ALARM popup shown");
}

void UICommon::hideAlarmPopup() {
    if (alarm_popup) {
        lv_obj_del(alarm_popup);
        alarm_popup = nullptr;
        alarm_popup_msg_label = nullptr;
        Serial.println("UICommon: ALARM popup hidden");
    }
}

// Check current state and manage popups accordingly
void UICommon::checkStatePopups(int current_state, const char *last_message) {
    // If state changed from previous, reset dismissal flags and hide any existing popups
    if (current_state != last_popup_state && last_popup_state != -1) {
        // State changed - reset dismissal flags
        if (current_state != STATE_HOLD) {
            hold_popup_dismissed = false;
        }
        if (current_state != STATE_ALARM) {
            alarm_popup_dismissed = false;
        }
        
        // Hide any existing popups
        hideHoldPopup();
        hideAlarmPopup();
    }
    
    // Show appropriate popup for current state (only if not dismissed)
    if (current_state == STATE_HOLD && !hold_popup_dismissed) {
        if (!hold_popup) {
            showHoldPopup(last_message);
        } else if (hold_popup_msg_label && last_message) {
            lv_label_set_text(hold_popup_msg_label, last_message);
        }
    } else if (current_state == STATE_ALARM && !alarm_popup_dismissed) {
        if (!alarm_popup) {
            showAlarmPopup(last_message);
        } else if (alarm_popup_msg_label && last_message) {
            lv_label_set_text(alarm_popup_msg_label, last_message);
        }
    }
    
    // Update last state
    last_popup_state = current_state;
}

void UICommon::updateFileProgress(bool is_printing, float percent, const char *filename, uint32_t elapsed_ms) {
    if (!lbl_file_progress_container) return;
    
    if (is_printing) {
        // Show job progress, hide normal status/position display
        lv_obj_clear_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);
        if (lbl_status) lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_label) lv_obj_add_flag(lbl_wpos_label, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_x) lv_obj_add_flag(lbl_wpos_x, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_y) lv_obj_add_flag(lbl_wpos_y, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_z) lv_obj_add_flag(lbl_wpos_z, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_label) lv_obj_add_flag(lbl_mpos_label, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_x) lv_obj_add_flag(lbl_mpos_x, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_y) lv_obj_add_flag(lbl_mpos_y, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_z) lv_obj_add_flag(lbl_mpos_z, LV_OBJ_FLAG_HIDDEN);
        
        // Update filename (delta check)
        if (lbl_filename && filename && strcmp(filename, last_filename) != 0) {
            lv_label_set_text(lbl_filename, filename);
            strncpy(last_filename, filename, sizeof(last_filename) - 1);
            last_filename[sizeof(last_filename) - 1] = '\0';
        }
        
        // Update progress bar and percentage (delta check with 0.1% tolerance)
        if (fabsf(percent - last_percent) >= 0.1f) {
            if (bar_progress) {
                lv_bar_set_value(bar_progress, (int)percent, LV_ANIM_OFF);
            }
            if (lbl_percent) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%.1f%%", percent);
                lv_label_set_text(lbl_percent, percent_text);
            }
            last_percent = percent;
        }
        
        // Update elapsed time (delta check - only update if changed by 1+ second)
        uint32_t elapsed_sec = elapsed_ms / 1000;
        if (elapsed_sec != last_elapsed_sec) {
            if (lbl_elapsed_time) {
                uint32_t hours = elapsed_sec / 3600;
                uint32_t minutes = (elapsed_sec % 3600) / 60;
                
                char time_text[16];
                if (hours > 0) {
                    snprintf(time_text, sizeof(time_text), "%d:%02d", hours, minutes);
                    if (lbl_elapsed_unit) lv_label_set_text(lbl_elapsed_unit, "hr:min");
                } else {
                    snprintf(time_text, sizeof(time_text), "%d:%02d", minutes, (int)(elapsed_sec % 60));
                    if (lbl_elapsed_unit) lv_label_set_text(lbl_elapsed_unit, "min:sec");
                }
                lv_label_set_text(lbl_elapsed_time, time_text);
            }
            last_elapsed_sec = elapsed_sec;
        }
        
        // Calculate and update estimated time (delta check - only update if changed by 1+ second)
        if (lbl_estimated_time && percent > 0.1f) {
            uint32_t estimated_total_sec = (uint32_t)((elapsed_sec / percent) * 100.0f);
            
            if (estimated_total_sec != last_estimated_sec) {
                uint32_t est_hours = estimated_total_sec / 3600;
                uint32_t est_minutes = (estimated_total_sec % 3600) / 60;
                
                char est_text[20];
                if (est_hours > 0) {
                    snprintf(est_text, sizeof(est_text), "%d:%02d", est_hours, est_minutes);
                    if (lbl_estimated_unit) lv_label_set_text(lbl_estimated_unit, "hr:min");
                } else {
                    snprintf(est_text, sizeof(est_text), "%d:%02d", est_minutes, (int)(estimated_total_sec % 60));
                    if (lbl_estimated_unit) lv_label_set_text(lbl_estimated_unit, "min:sec");
                }
                lv_label_set_text(lbl_estimated_time, est_text);
                last_estimated_sec = estimated_total_sec;
            }
        }
    } else {
        // Hide job progress, show normal status/position display
        lv_obj_add_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);
        if (lbl_status) lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_label) lv_obj_clear_flag(lbl_wpos_label, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_x) lv_obj_clear_flag(lbl_wpos_x, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_y) lv_obj_clear_flag(lbl_wpos_y, LV_OBJ_FLAG_HIDDEN);
        if (lbl_wpos_z) lv_obj_clear_flag(lbl_wpos_z, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_label) lv_obj_clear_flag(lbl_mpos_label, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_x) lv_obj_clear_flag(lbl_mpos_x, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_y) lv_obj_clear_flag(lbl_mpos_y, LV_OBJ_FLAG_HIDDEN);
        if (lbl_mpos_z) lv_obj_clear_flag(lbl_mpos_z, LV_OBJ_FLAG_HIDDEN);
        
        // Reset cached values when not printing
        last_filename[0] = '\0';
        last_percent = -1.0f;
        last_elapsed_sec = 0xFFFFFFFF;
        last_estimated_sec = 0xFFFFFFFF;
    }
}

void UICommon::showWCSLockDialog(const char *wcs_code, const char *wcs_name, void (*continue_callback)(lv_event_t*)) {
    // Create modal backdrop
    lv_obj_t *backdrop = lv_obj_create(lv_screen_active());
    lv_obj_set_size(backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create dialog container
    lv_obj_t *dialog = lv_obj_create(backdrop);
    lv_obj_set_size(dialog, 600, 300);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARKER, 0);
    lv_obj_set_style_border_color(dialog, UITheme::UI_WARNING, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title with warning icon and WCS code
    lv_obj_t *title = lv_label_create(dialog);
    char title_text[80];
    if (wcs_name && strlen(wcs_name) > 0) {
        snprintf(title_text, sizeof(title_text), LV_SYMBOL_WARNING " %s (%s) is Locked", wcs_code, wcs_name);
    } else {
        snprintf(title_text, sizeof(title_text), LV_SYMBOL_WARNING " %s is Locked", wcs_code);
    }
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::UI_WARNING, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, 560);
    lv_obj_set_pos(title, 0, 0);
    
    // Warning message with centered text and more spacing from title
    lv_obj_t *message = lv_label_create(dialog);
    lv_label_set_text(message, "Modifying this position may affect fixtures or setups.\n\nAre you sure you want to continue?");
    lv_obj_set_style_text_font(message, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(message, 560);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(message, 0, 75);
    
    // Cancel button (left)
    lv_obj_t *btn_cancel = lv_button_create(dialog);
    lv_obj_set_size(btn_cancel, 220, 50);
    lv_obj_set_pos(btn_cancel, 40, 200);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_MEDIUM, 0);
    
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
    
    // Close on Cancel
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t *e) {
        lv_obj_t *backdrop = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(backdrop);
    }, LV_EVENT_CLICKED, backdrop);
    
    // Continue button (right) with warning icon
    lv_obj_t *btn_continue = lv_button_create(dialog);
    lv_obj_set_size(btn_continue, 220, 50);
    lv_obj_set_pos(btn_continue, 280, 200);
    lv_obj_set_style_bg_color(btn_continue, UITheme::UI_WARNING, 0);
    
    lv_obj_t *lbl_continue = lv_label_create(btn_continue);
    lv_label_set_text(lbl_continue, LV_SYMBOL_WARNING " Continue");
    lv_obj_set_style_text_font(lbl_continue, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_continue);
    
    // Store callback and backdrop in user data
    struct CallbackData {
        void (*callback)(lv_event_t*);
        lv_obj_t *backdrop;
    };
    
    CallbackData *data = (CallbackData*)malloc(sizeof(CallbackData));
    data->callback = continue_callback;
    data->backdrop = backdrop;
    
    lv_obj_add_event_cb(btn_continue, [](lv_event_t *e) {
        CallbackData *data = (CallbackData*)lv_event_get_user_data(e);
        
        // Call the continue callback
        if (data->callback) {
            data->callback(e);
        }
        
        // Close dialog
        lv_obj_delete(data->backdrop);
        free(data);
    }, LV_EVENT_CLICKED, data);
}

// Load system preferences once at startup
void UICommon::loadSystemPreferences() {
    // Load enable_a_axis from the selected machine's config (it's machine-specific)
    enable_a_axis = false;
    MachineConfig config;
    if (MachineConfigManager::getSelectedMachine(config)) {
        enable_a_axis = config.enable_a_axis;
    }

    Serial.printf("UICommon: Loaded system preferences - enable_a_axis=%d\n", enable_a_axis);
}

// Get cached A-axis enabled preference
bool UICommon::isAAxisEnabled() {
    return enable_a_axis;
}

// Set cached A-axis enabled preference (call after saving to preferences)
void UICommon::setAAxisEnabled(bool enabled) {
    enable_a_axis = enabled;
    Serial.printf("UICommon: Updated enable_a_axis to %d\n", enabled);
}

