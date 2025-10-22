#include "ui/tabs/settings/ui_tab_settings_connection.h"
#include "ui/ui_theme.h"
#include <Preferences.h>

// Global references for Connection settings UI elements
static lv_obj_t *ssid_textarea_global = NULL;
static lv_obj_t *pass_textarea_global = NULL;
static lv_obj_t *ip_textarea_global = NULL;
static lv_obj_t *port_textarea_global = NULL;
static lv_obj_t *status_info_global = NULL;
static lv_obj_t *keyboard = NULL;

// Preferences object for flash storage
static Preferences preferences;

// Forward declarations for event handlers
static void scroll_to_field_cb(lv_timer_t *timer);
static void settings_textarea_event_handler(lv_event_t *e);
static void btn_save_connection_event_handler(lv_event_t *e);

void UITabSettingsConnection::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // WiFi SSID
    lv_obj_t *ssid_label = lv_label_create(tab);
    lv_label_set_text(ssid_label, "WiFi SSID:");
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ssid_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(ssid_label, 20, 20);

    lv_obj_t *ssid_textarea = lv_textarea_create(tab);
    lv_obj_set_size(ssid_textarea, 240, 45);
    lv_obj_set_pos(ssid_textarea, 140, 15);
    lv_textarea_set_placeholder_text(ssid_textarea, "WiFi network name");
    lv_textarea_set_one_line(ssid_textarea, true);
    lv_obj_add_event_cb(ssid_textarea, settings_textarea_event_handler, LV_EVENT_ALL, NULL);
    ssid_textarea_global = ssid_textarea;  // Store global reference

    // WiFi Password
    lv_obj_t *pass_label = lv_label_create(tab);
    lv_label_set_text(pass_label, "WiFi Password:");
    lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pass_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(pass_label, 20, 80);

    lv_obj_t *pass_textarea = lv_textarea_create(tab);
    lv_obj_set_size(pass_textarea, 240, 45);
    lv_obj_set_pos(pass_textarea, 140, 75);
    lv_textarea_set_placeholder_text(pass_textarea, "WiFi password");
    lv_textarea_set_one_line(pass_textarea, true);
    lv_textarea_set_password_mode(pass_textarea, true);
    lv_textarea_set_text(pass_textarea, "");
    lv_obj_add_event_cb(pass_textarea, settings_textarea_event_handler, LV_EVENT_ALL, NULL);
    pass_textarea_global = pass_textarea;  // Store global reference

    // FluidNC IP Address
    lv_obj_t *ip_label = lv_label_create(tab);
    lv_label_set_text(ip_label, "FluidNC IP:");
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ip_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(ip_label, 20, 140);

    lv_obj_t *ip_textarea = lv_textarea_create(tab);
    lv_obj_set_size(ip_textarea, 240, 45);
    lv_obj_set_pos(ip_textarea, 140, 135);
    lv_textarea_set_placeholder_text(ip_textarea, "e.g., 192.168.0.1");
    lv_textarea_set_one_line(ip_textarea, true);
    lv_obj_add_event_cb(ip_textarea, settings_textarea_event_handler, LV_EVENT_ALL, NULL);
    ip_textarea_global = ip_textarea;  // Store global reference

    // WebSocket Port
    lv_obj_t *port_label = lv_label_create(tab);
    lv_label_set_text(port_label, "WS Port:");
    lv_obj_set_style_text_font(port_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(port_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(port_label, 20, 200);

    lv_obj_t *port_textarea = lv_textarea_create(tab);
    lv_obj_set_size(port_textarea, 240, 45);
    lv_obj_set_pos(port_textarea, 140, 195);
    lv_textarea_set_placeholder_text(port_textarea, "e.g., 81");
    lv_textarea_set_one_line(port_textarea, true);
    lv_textarea_set_text(port_textarea, "81");
    lv_obj_add_event_cb(port_textarea, settings_textarea_event_handler, LV_EVENT_ALL, NULL);
    port_textarea_global = port_textarea;  // Store global reference

    // Save button
    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 160, 50);
    lv_obj_set_pos(btn_save, 20, 260);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save Settings");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, btn_save_connection_event_handler, LV_EVENT_CLICKED, NULL);

    // Connect button
    lv_obj_t *btn_connect = lv_button_create(tab);
    lv_obj_set_size(btn_connect, 160, 50);
    lv_obj_set_pos(btn_connect, 200, 260);
    lv_obj_set_style_bg_color(btn_connect, UITheme::BTN_CONNECT, LV_PART_MAIN);
    lv_obj_t *lbl_connect = lv_label_create(btn_connect);
    lv_label_set_text(lbl_connect, "Connect");
    lv_obj_set_style_text_font(lbl_connect, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_connect);

    // Disconnect button
    lv_obj_t *btn_disconnect = lv_button_create(tab);
    lv_obj_set_size(btn_disconnect, 160, 50);
    lv_obj_set_pos(btn_disconnect, 380, 260);
    lv_obj_set_style_bg_color(btn_disconnect, UITheme::BTN_DISCONNECT, LV_PART_MAIN);
    lv_obj_t *lbl_disconnect = lv_label_create(btn_disconnect);
    lv_label_set_text(lbl_disconnect, "Disconnect");
    lv_obj_set_style_text_font(lbl_disconnect, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_disconnect);

    // Status info
    lv_obj_t *status_info = lv_label_create(tab);
    lv_label_set_text(status_info, "Connection Status: Disconnected");
    lv_obj_set_style_text_font(status_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_info, UITheme::UI_WARNING, 0);
    lv_obj_set_pos(status_info, 20, 325);
    status_info_global = status_info;  // Store global reference
    
    // Add spacer at bottom to ensure content extends past visible area
    lv_obj_t *spacer = lv_obj_create(tab);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_pos(spacer, 0, 500);  // Position beyond visible area
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    
    // Load saved settings from flash
    UITabSettingsConnection::loadSettings();
}

// Scroll to input field callback (one-shot timer)
static void scroll_to_field_cb(lv_timer_t *timer) {
    lv_obj_t *textarea = (lv_obj_t *)lv_timer_get_user_data(timer);
    lv_obj_t *tab = lv_obj_get_parent(textarea);
    
    // Get textarea position and height
    int32_t textarea_y = lv_obj_get_y(textarea);
    int32_t textarea_h = lv_obj_get_height(textarea);
    
    // Position field near bottom of visible area (closer to keyboard)
    int32_t scroll_y = textarea_y + textarea_h - 170;
    
    // Make sure we don't scroll negative
    if (scroll_y < 0) scroll_y = 0;
    
    // Scroll the tab
    lv_obj_scroll_to_y(tab, scroll_y, LV_ANIM_ON);
    
    lv_timer_del(timer);  // Delete one-shot timer
}

// Connection input field event handler for keyboard
static void settings_textarea_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *textarea = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Get the parent tab
        lv_obj_t *tab = lv_obj_get_parent(textarea);
        
        // Enable scrolling on the Connection tab
        lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_AUTO);
        
        // Create keyboard if it doesn't exist
        if (keyboard == NULL) {
            keyboard = lv_keyboard_create(lv_screen_active());
            lv_obj_set_size(keyboard, 800, 240);
            lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        }
        lv_keyboard_set_textarea(keyboard, textarea);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // Scroll after a short delay to ensure keyboard is rendered
        lv_timer_t *timer = lv_timer_create(scroll_to_field_cb, 100, textarea);
        lv_timer_set_repeat_count(timer, 1);
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        // Hide keyboard when textarea loses focus
        if (keyboard != NULL) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, NULL);
            
            // Scroll back to top
            lv_obj_t *tab = lv_obj_get_parent(textarea);
            lv_obj_scroll_to_y(tab, 0, LV_ANIM_ON);
            
            // Disable scrolling on the Connection tab
            lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
    else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Hide keyboard on OK or Cancel
        if (keyboard != NULL) {
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, NULL);
            
            // Scroll back to top
            lv_obj_t *tab = lv_obj_get_parent(textarea);
            lv_obj_scroll_to_y(tab, 0, LV_ANIM_ON);
            
            // Disable scrolling on the Connection tab
            lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
        }
    }
}

// Save button event handler
static void btn_save_connection_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        UITabSettingsConnection::saveSettings();
    }
}

// Load settings from flash
void UITabSettingsConnection::loadSettings() {
    preferences.begin("fluidtouch", true);  // Read-only mode
    
    String ssid = preferences.getString("wifi_ssid", "");
    String pass = preferences.getString("wifi_pass", "");
    String ip = preferences.getString("fluidnc_ip", "");
    String port = preferences.getString("ws_port", "81");
    
    preferences.end();
    
    Serial.println("=== Loading Connection Settings from Flash ===");
    Serial.printf("WiFi SSID: %s\n", ssid.c_str());
    Serial.printf("WiFi Pass: %s\n", pass.length() > 0 ? "****" : "(empty)");
    Serial.printf("FluidNC IP: %s\n", ip.c_str());
    Serial.printf("WS Port: %s\n", port.c_str());
    
    // Update UI elements if they exist
    if (ssid_textarea_global != NULL && ssid.length() > 0) {
        lv_textarea_set_text(ssid_textarea_global, ssid.c_str());
    }
    if (pass_textarea_global != NULL && pass.length() > 0) {
        lv_textarea_set_text(pass_textarea_global, pass.c_str());
    }
    if (ip_textarea_global != NULL && ip.length() > 0) {
        lv_textarea_set_text(ip_textarea_global, ip.c_str());
    }
    if (port_textarea_global != NULL && port.length() > 0) {
        lv_textarea_set_text(port_textarea_global, port.c_str());
    }
}

// Save settings to flash
void UITabSettingsConnection::saveSettings() {
    preferences.begin("fluidtouch", false);  // Read-write mode
    
    // Get text from textareas
    String ssid = lv_textarea_get_text(ssid_textarea_global);
    String pass = lv_textarea_get_text(pass_textarea_global);
    String ip = lv_textarea_get_text(ip_textarea_global);
    String port = lv_textarea_get_text(port_textarea_global);
    
    // Save to flash
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);
    preferences.putString("fluidnc_ip", ip);
    preferences.putString("ws_port", port);
    
    preferences.end();
    
    Serial.println("=== Connection Settings Saved to Flash ===");
    Serial.printf("WiFi SSID: %s\n", ssid.c_str());
    Serial.printf("WiFi Pass: %s\n", pass.length() > 0 ? "****" : "(empty)");
    Serial.printf("FluidNC IP: %s\n", ip.c_str());
    Serial.printf("WS Port: %s\n", port.c_str());
    
    // Update status label
    if (status_info_global != NULL) {
        lv_label_set_text(status_info_global, "Settings saved successfully!");
        lv_obj_set_style_text_color(status_info_global, UITheme::UI_SUCCESS, 0);
    }
}
