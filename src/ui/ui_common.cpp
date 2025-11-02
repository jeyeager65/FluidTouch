#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "ui/ui_machine_select.h"
#include "network/fluidnc_client.h"
#include "core/display_driver.h"
#include "network/screenshot_server.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

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
lv_obj_t *UICommon::alarm_popup = nullptr;
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

// Machine Position labels
lv_obj_t *UICommon::lbl_mpos_label = nullptr;
lv_obj_t *UICommon::lbl_mpos_x = nullptr;
lv_obj_t *UICommon::lbl_mpos_y = nullptr;
lv_obj_t *UICommon::lbl_mpos_z = nullptr;

// Cached values for delta checking
float UICommon::last_wpos_x = -9999.0f;
float UICommon::last_wpos_y = -9999.0f;
float UICommon::last_wpos_z = -9999.0f;
float UICommon::last_mpos_x = -9999.0f;
float UICommon::last_mpos_y = -9999.0f;
float UICommon::last_mpos_z = -9999.0f;

// Connection timeout tracking
static uint32_t connection_timeout_start = 0;
static bool connection_timeout_active = false;
static bool connection_error_shown = false;

// Event handler for status bar left area click (go to Status tab)
static void status_bar_left_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Switch to Status tab (index 0)
        lv_obj_t *tabview = UITabs::getTabview();
        if (tabview) {
            lv_tabview_set_active(tabview, 0, LV_ANIM_ON);
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
            }
        } else {
            Serial.println("UICommon: Warning - Wireless connection selected but no SSID configured");
            // Popup already shown at start
        }
    } else {
        Serial.println("UICommon: Wired connection selected, skipping WiFi initialization");
        // Popup already shown at start
    }
    
    // Connect to FluidNC using selected machine
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
    lv_obj_set_style_text_color(lbl_wpos_label, UITheme::POS_WORK, 0);  // Orange - primary data
    lv_obj_align(lbl_wpos_label, LV_ALIGN_TOP_MID, -198, 3);  // -210 + 2 + 10 = -198

    lbl_wpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_x, "X:----.---");
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_wpos_x, LV_ALIGN_TOP_MID, -110, 3);  // -120 + 10 = -110

    lbl_wpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_y, "Y:----.---");
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_wpos_y, LV_ALIGN_TOP_MID, 0, 3);  // -10 + 10 = 0

    lbl_wpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_z, "Z:----.---");
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_wpos_z, LV_ALIGN_TOP_MID, 110, 3);  // 100 + 10 = 110

    // Machine Position - Line 2
    lbl_mpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_label, "MPos:");
    lv_obj_set_style_text_font(lbl_mpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_label, UITheme::POS_MACHINE, 0);  // Cyan - secondary data
    lv_obj_align(lbl_mpos_label, LV_ALIGN_BOTTOM_MID, -198, -3);  // -210 + 2 + 10 = -198

    lbl_mpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_x, "X:----.---");
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_mpos_x, LV_ALIGN_BOTTOM_MID, -110, -3);  // -120 + 10 = -110

    lbl_mpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_y, "Y:----.---");
    lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_mpos_y, LV_ALIGN_BOTTOM_MID, 0, -3);  // -10 + 10 = 0

    lbl_mpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_z, "Z:----.---");
    lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_mpos_z, LV_ALIGN_BOTTOM_MID, 110, -3);  // 100 + 10 = 110

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
    String wifi_ssid = WiFi.isConnected() ? WiFi.SSID() : "Not Connected";
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
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_mpos_x && y == last_mpos_y && z == last_mpos_z) {
        return;
    }
    
    char buf[16];
    
    if (status_bar && lbl_mpos_x && x != last_mpos_x) {
        snprintf(buf, sizeof(buf), "X:%04.3f", x);
        lv_label_set_text(lbl_mpos_x, buf);
        last_mpos_x = x;
    }
    if (status_bar && lbl_mpos_y && y != last_mpos_y) {
        snprintf(buf, sizeof(buf), "Y:%04.3f", y);
        lv_label_set_text(lbl_mpos_y, buf);
        last_mpos_y = y;
    }
    if (status_bar && lbl_mpos_z && z != last_mpos_z) {
        snprintf(buf, sizeof(buf), "Z:%04.3f", z);
        lv_label_set_text(lbl_mpos_z, buf);
        last_mpos_z = z;
    }
}

void UICommon::updateWorkPosition(float x, float y, float z) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_wpos_x && y == last_wpos_y && z == last_wpos_z) {
        return;
    }
    
    char buf[16];
    
    if (status_bar && lbl_wpos_x && x != last_wpos_x) {
        snprintf(buf, sizeof(buf), "X:%04.3f", x);
        lv_label_set_text(lbl_wpos_x, buf);
        last_wpos_x = x;
    }
    if (status_bar && lbl_wpos_y && y != last_wpos_y) {
        snprintf(buf, sizeof(buf), "Y:%04.3f", y);
        lv_label_set_text(lbl_wpos_y, buf);
        last_wpos_y = y;
    }
    if (status_bar && lbl_wpos_z && z != last_wpos_z) {
        snprintf(buf, sizeof(buf), "Z:%04.3f", z);
        lv_label_set_text(lbl_wpos_z, buf);
        last_wpos_z = z;
    }
}

void UICommon::updateMachineState(const char *state) {
    if (status_bar && lbl_status) {
        lv_label_set_text(lbl_status, state);
        
        // Color code the status (state is already uppercase)
        if (strcmp(state, "IDLE") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0);
        } else if (strcmp(state, "RUN") == 0 || strcmp(state, "JOG") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_RUN, 0);
        } else if (strcmp(state, "ALARM") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_ALARM, 0);
        } else {
            lv_obj_set_style_text_color(lbl_status, UITheme::UI_WARNING, 0);
        }
    }
}

void UICommon::updateConnectionStatus(bool machine_connected, bool wifi_connected) {
    // Update machine symbol color with three states:
    // - Red (STATE_ALARM): Disconnected
    // - Orange (UI_WARNING): Connected with fallback polling (degraded)
    // - Green (STATE_IDLE): Connected with auto-reporting (optimal)
    if (lbl_machine_symbol) {
        lv_color_t color;
        if (!machine_connected) {
            color = UITheme::STATE_ALARM;  // Red: Disconnected
        } else if (!FluidNCClient::isAutoReporting()) {
            color = UITheme::UI_WARNING;   // Orange: Fallback polling (1s updates)
        } else {
            color = UITheme::STATE_IDLE;   // Green: Auto-reporting (250ms updates)
        }
        lv_obj_set_style_text_color(lbl_machine_symbol, color, 0);
    }
    
    // Update WiFi symbol color
    if (lbl_wifi_symbol) {
        lv_obj_set_style_text_color(lbl_wifi_symbol, 
            wifi_connected ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
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
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(machine_select_dialog);
    lv_obj_set_size(content, 500, 220);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Icon and title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Restart Controller?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    
    // Message line 1
    lv_obj_t *msg1_label = lv_label_create(content);
    lv_label_set_text(msg1_label, "This will disconnect and restart the");
    lv_obj_set_style_text_font(msg1_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg1_label, UITheme::TEXT_LIGHT, 0);
    
    // Message line 2
    lv_obj_t *msg2_label = lv_label_create(content);
    lv_label_set_text(msg2_label, "controller. Continue?");
    lv_obj_set_style_text_font(msg2_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg2_label, UITheme::TEXT_LIGHT, 0);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 180, 50);
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, on_machine_select_cancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Confirm button
    lv_obj_t *confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(confirm_btn, 180, 50);
    lv_obj_set_style_bg_color(confirm_btn, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(confirm_btn, on_machine_select_confirm, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, LV_SYMBOL_POWER " Restart");
    lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_18, 0);
    lv_obj_center(confirm_label);
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
    
    // Create modal background
    connection_error_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(connection_error_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(connection_error_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(connection_error_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(connection_error_dialog, 0, 0);
    lv_obj_clear_flag(connection_error_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(connection_error_dialog);
    lv_obj_set_size(content, 600, 280);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Error icon and title
    lv_obj_t *title_label = lv_label_create(content);
    char title_text[128];
    snprintf(title_text, sizeof(title_text), LV_SYMBOL_WARNING " %s", title);
    lv_label_set_text(title_label, title_text);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title_label, UITheme::STATE_ALARM, 0);
    
    // Error message
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, message);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, 550);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Close button
    lv_obj_t *close_btn = lv_btn_create(btn_container);
    lv_obj_set_size(close_btn, 150, 50);
    lv_obj_set_style_bg_color(close_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(close_btn, on_connection_error_close, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_18, 0);
    lv_obj_center(close_label);
    
    // Connect button
    lv_obj_t *connect_btn = lv_btn_create(btn_container);
    lv_obj_set_size(connect_btn, 150, 50);
    lv_obj_set_style_bg_color(connect_btn, UITheme::BTN_CONNECT, 0);
    lv_obj_add_event_cb(connect_btn, on_connection_error_connect, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, LV_SYMBOL_REFRESH " Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_16, 0);
    lv_obj_center(connect_label);
    
    // Restart button
    lv_obj_t *restart_btn = lv_btn_create(btn_container);
    lv_obj_set_size(restart_btn, 150, 50);
    lv_obj_set_style_bg_color(restart_btn, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(restart_btn, on_connection_error_restart, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *restart_label = lv_label_create(restart_btn);
    lv_label_set_text(restart_label, LV_SYMBOL_POWER " Restart");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_16, 0);
    lv_obj_center(restart_label);
    
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
        hideConnectingPopup();  // Hide connecting popup when connected
        hideConnectionErrorDialog();
        return;
    }
    
    // Check if timeout exceeded (10 seconds)
    uint32_t elapsed = millis() - connection_timeout_start;
    if (elapsed >= 10000 && !connection_error_shown) {
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
    lv_obj_t *msg_label = lv_label_create(dialog);
    lv_label_set_text(msg_label, message && strlen(message) > 0 ? message : "Machine paused");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_width(msg_label, 520);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    
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
    
    // Resume button
    lv_obj_t *resume_btn = lv_btn_create(btn_container);
    lv_obj_set_size(resume_btn, 250, 50);
    lv_obj_set_style_bg_color(resume_btn, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(resume_btn, [](lv_event_t *e) {
        FluidNCClient::sendCommand("~"); // Send cycle start (resume)
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *resume_label = lv_label_create(resume_btn);
    lv_label_set_text(resume_label, LV_SYMBOL_PLAY " Resume");
    lv_obj_set_style_text_font(resume_label, &lv_font_montserrat_18, 0);
    lv_obj_center(resume_label);
    
    Serial.println("UICommon: HOLD popup shown");
}

void UICommon::hideHoldPopup() {
    if (hold_popup) {
        lv_obj_del(hold_popup);
        hold_popup = nullptr;
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
    lv_obj_t *msg_label = lv_label_create(dialog);
    lv_label_set_text(msg_label, message && strlen(message) > 0 ? message : "Alarm condition detected");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_width(msg_label, 520);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(msg_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    
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
    
    // Clear Alarm button
    lv_obj_t *clear_btn = lv_btn_create(btn_container);
    lv_obj_set_size(clear_btn, 250, 50);
    lv_obj_set_style_bg_color(clear_btn, UITheme::UI_WARNING, 0);
    lv_obj_add_event_cb(clear_btn, [](lv_event_t *e) {
        // Send soft reset, then unlock
        FluidNCClient::sendCommand("\x18"); // Ctrl-X (soft reset)
        delay(100);
        FluidNCClient::sendCommand("$X\n");   // Unlock
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, LV_SYMBOL_REFRESH " Clear Alarm");
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_18, 0);
    lv_obj_center(clear_label);
    
    Serial.println("UICommon: ALARM popup shown");
}

void UICommon::hideAlarmPopup() {
    if (alarm_popup) {
        lv_obj_del(alarm_popup);
        alarm_popup = nullptr;
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
    if (current_state == STATE_HOLD && !hold_popup && !hold_popup_dismissed) {
        showHoldPopup(last_message);
    } else if (current_state == STATE_ALARM && !alarm_popup && !alarm_popup_dismissed) {
        showAlarmPopup(last_message);
    }
    
    // Update last state
    last_popup_state = current_state;
}
