#include "ui/tabs/settings/ui_tab_settings_power.h"
#include "ui/ui_theme.h"
#include "config.h"
#include "core/power_manager.h"

// Global references for UI elements
static lv_obj_t *power_mgmt_switch = NULL;
static lv_obj_t *dim_timeout_dropdown = NULL;
static lv_obj_t *sleep_timeout_dropdown = NULL;
static lv_obj_t *normal_brightness_dropdown = NULL;
static lv_obj_t *dim_brightness_dropdown = NULL;
static lv_obj_t *deep_sleep_timeout_dropdown = NULL;
static lv_obj_t *status_label = NULL;

// Forward declarations for event handlers
static void btn_save_power_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);
static void power_mgmt_switch_event_handler(lv_event_t *e);

void UITabSettingsPower::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // === Power Management Section ===
    lv_obj_t *section_title = lv_label_create(tab);
    lv_label_set_text(section_title, "POWER MANAGEMENT");
    lv_obj_set_style_text_font(section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(section_title, 20, 20);
    
    // Column 1: Left side (x=20)
    // Enable power management switch
    lv_obj_t *pm_label = lv_label_create(tab);
    lv_label_set_text(pm_label, "Enabled:");
    lv_obj_set_style_text_font(pm_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(pm_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(pm_label, 20, 70);
    
    power_mgmt_switch = lv_switch_create(tab);
    lv_obj_set_pos(power_mgmt_switch, 140, 65);
    if (PowerManager::isEnabled()) {
        lv_obj_add_state(power_mgmt_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(power_mgmt_switch, power_mgmt_switch_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Dim timeout dropdown
    lv_obj_t *dim_label = lv_label_create(tab);
    lv_label_set_text(dim_label, "Dim After:");
    lv_obj_set_style_text_font(dim_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(dim_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(dim_label, 20, 120);
    
    dim_timeout_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(dim_timeout_dropdown, "Disabled\n15 sec\n30 sec\n45 sec\n60 sec\n90 sec\n2 min\n5 min");
    lv_obj_set_size(dim_timeout_dropdown, 130, 48);
    lv_obj_set_style_pad_top(dim_timeout_dropdown, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_obj_set_pos(dim_timeout_dropdown, 140, 107);
    lv_obj_set_style_text_font(dim_timeout_dropdown, &lv_font_montserrat_18, 0);
    
    // Set current dim timeout selection
    uint32_t dim_sec = PowerManager::getDimTimeout();
    uint16_t dim_idx = 2;  // Default to 30 sec
    if (dim_sec == 0) dim_idx = 0;          // Disabled
    else if (dim_sec == 15) dim_idx = 1;
    else if (dim_sec == 30) dim_idx = 2;
    else if (dim_sec == 45) dim_idx = 3;
    else if (dim_sec == 60) dim_idx = 4;
    else if (dim_sec == 90) dim_idx = 5;
    else if (dim_sec == 120) dim_idx = 6;
    else if (dim_sec == 300) dim_idx = 7;
    lv_dropdown_set_selected(dim_timeout_dropdown, dim_idx);
    
    // Sleep timeout dropdown
    lv_obj_t *sleep_label = lv_label_create(tab);
    lv_label_set_text(sleep_label, "Sleep After:");
    lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sleep_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(sleep_label, 20, 172);
    
    sleep_timeout_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(sleep_timeout_dropdown, "Disabled\n1 min\n2 min\n5 min\n10 min\n15 min\n30 min\n60 min");
    lv_obj_set_size(sleep_timeout_dropdown, 130, 48);
    lv_obj_set_style_pad_top(sleep_timeout_dropdown, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_obj_set_pos(sleep_timeout_dropdown, 140, 158);
    lv_obj_set_style_text_font(sleep_timeout_dropdown, &lv_font_montserrat_18, 0);
    
    // Set current sleep timeout selection
    uint32_t sleep_sec = PowerManager::getSleepTimeout();
    uint16_t sleep_idx = 3;  // Default to 5 min
    if (sleep_sec == 0) sleep_idx = 0;          // Disabled
    else if (sleep_sec == 60) sleep_idx = 1;
    else if (sleep_sec == 120) sleep_idx = 2;
    else if (sleep_sec == 300) sleep_idx = 3;
    else if (sleep_sec == 600) sleep_idx = 4;
    else if (sleep_sec == 900) sleep_idx = 5;
    else if (sleep_sec == 1800) sleep_idx = 6;
    else if (sleep_sec == 3600) sleep_idx = 7;
    lv_dropdown_set_selected(sleep_timeout_dropdown, sleep_idx);
    
    // Column 2: Right side (x=300)
    // Normal brightness dropdown
    lv_obj_t *normal_brightness_label = lv_label_create(tab);
    lv_label_set_text(normal_brightness_label, "Brightness:");
    lv_obj_set_style_text_font(normal_brightness_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(normal_brightness_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(normal_brightness_label, 300, 70);
    
    normal_brightness_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(normal_brightness_dropdown, "25%\n50%\n75%\n100%");
    lv_obj_set_size(normal_brightness_dropdown, 130, 48);
    lv_obj_set_style_pad_top(normal_brightness_dropdown, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_obj_set_pos(normal_brightness_dropdown, 420, 56);
    lv_obj_set_style_text_font(normal_brightness_dropdown, &lv_font_montserrat_18, 0);
    
    // Set current normal brightness selection (now uses percentages directly)
    uint8_t normal_bright = PowerManager::getNormalBrightness();  // 0-100
    uint16_t normal_idx = 3;  // Default to 100%
    if (normal_bright <= 25) normal_idx = 0;       // 25%
    else if (normal_bright <= 50) normal_idx = 1;  // 50%
    else if (normal_bright <= 75) normal_idx = 2;  // 75%
    else normal_idx = 3;                           // 100%
    lv_dropdown_set_selected(normal_brightness_dropdown, normal_idx);
    
    // Dim brightness dropdown
    lv_obj_t *brightness_label = lv_label_create(tab);
    lv_label_set_text(brightness_label, "Dim Level:");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(brightness_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(brightness_label, 300, 120);
    
    dim_brightness_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(dim_brightness_dropdown, "5%\n10%\n25%\n50%");
    lv_obj_set_size(dim_brightness_dropdown, 130, 48);
    lv_obj_set_style_pad_top(dim_brightness_dropdown, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_obj_set_pos(dim_brightness_dropdown, 420, 107);
    lv_obj_set_style_text_font(dim_brightness_dropdown, &lv_font_montserrat_18, 0);
    
    // Set current dim brightness selection (now uses percentages directly)
    uint8_t dim_bright = PowerManager::getDimBrightness();  // 0-100
    uint16_t bright_idx = 2;  // Default to 25%
    if (dim_bright <= 5) bright_idx = 0;       // 5%
    else if (dim_bright <= 10) bright_idx = 1;  // 10%
    else if (dim_bright <= 25) bright_idx = 2;  // 25%
    else bright_idx = 3;                        // 50%
    lv_dropdown_set_selected(dim_brightness_dropdown, bright_idx);
    
    // Deep Sleep label
    lv_obj_t *deep_sleep_label = lv_label_create(tab);
    lv_label_set_text(deep_sleep_label, "Deep Sleep:");
    lv_obj_set_style_text_font(deep_sleep_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(deep_sleep_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(deep_sleep_label, 300, 172);
    
    // Deep Sleep timeout dropdown
    deep_sleep_timeout_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(deep_sleep_timeout_dropdown, "Disabled\n5 min\n10 min\n15 min\n30 min\n60 min\n90 min");
    lv_obj_set_size(deep_sleep_timeout_dropdown, 130, 48);
    lv_obj_set_style_pad_top(deep_sleep_timeout_dropdown, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_obj_set_pos(deep_sleep_timeout_dropdown, 420, 158);
    lv_obj_set_style_text_font(deep_sleep_timeout_dropdown, &lv_font_montserrat_18, 0);
    
    // Set current deep sleep timeout selection
    uint32_t deep_sleep_sec = PowerManager::getDeepSleepTimeout();
    uint16_t deep_idx = 3;  // Default to 15 min
    if (deep_sleep_sec == 0) deep_idx = 0;           // Disabled
    else if (deep_sleep_sec <= 300) deep_idx = 1;    // 5 min
    else if (deep_sleep_sec <= 600) deep_idx = 2;    // 10 min
    else if (deep_sleep_sec <= 900) deep_idx = 3;    // 15 min
    else if (deep_sleep_sec <= 1800) deep_idx = 4;   // 30 min
    else if (deep_sleep_sec <= 3600) deep_idx = 5;   // 60 min
    else deep_idx = 6;                                // 90 min
    lv_dropdown_set_selected(deep_sleep_timeout_dropdown, deep_idx);
    
    // Explanatory note about IDLE/DISCONNECTED only (moved to bottom)
    lv_obj_t *note_label = lv_label_create(tab);
    lv_label_set_text(note_label, "Note: Power management is only active during IDLE or OFFLINE states.");
    lv_obj_set_pos(note_label, 20, 215);
    lv_obj_set_style_text_font(note_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(note_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_width(note_label, 760);  // Full width
    
    // Enable/disable power management controls based on switch state
    if (!PowerManager::isEnabled()) {
        lv_obj_add_state(dim_timeout_dropdown, LV_STATE_DISABLED);
        lv_obj_add_state(sleep_timeout_dropdown, LV_STATE_DISABLED);
        lv_obj_add_state(normal_brightness_dropdown, LV_STATE_DISABLED);
        lv_obj_add_state(dim_brightness_dropdown, LV_STATE_DISABLED);
        lv_obj_add_state(deep_sleep_timeout_dropdown, LV_STATE_DISABLED);
    }
    
    // === Action Buttons (positioned at bottom with 20px margins) ===
    // Save button
    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 180, 50);
    lv_obj_set_pos(btn_save, 20, 280);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save Settings");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, btn_save_power_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Reset to defaults button
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, 180, 50);
    lv_obj_set_pos(btn_reset, 220, 280);
    lv_obj_set_style_bg_color(btn_reset, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset Defaults");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, btn_reset_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Status label (positioned below buttons)
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(status_label, 20, 335);
}

// Save button event handler
static void btn_save_power_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Save power management settings
        bool pm_enabled = lv_obj_has_state(power_mgmt_switch, LV_STATE_CHECKED);
        PowerManager::setEnabled(pm_enabled);
        
        // Get dim timeout from dropdown
        uint16_t dim_idx = lv_dropdown_get_selected(dim_timeout_dropdown);
        uint32_t dim_seconds[] = {0, 15, 30, 45, 60, 90, 120, 300};  // 0=Disabled
        if (dim_idx < 8) {
            PowerManager::setDimTimeout(dim_seconds[dim_idx]);
        }
        
        // Get sleep timeout from dropdown
        uint16_t sleep_idx = lv_dropdown_get_selected(sleep_timeout_dropdown);
        uint32_t sleep_seconds[] = {0, 60, 120, 300, 600, 900, 1800, 3600};  // 0=Disabled
        if (sleep_idx < 8) {
            PowerManager::setSleepTimeout(sleep_seconds[sleep_idx]);
        }
        
        // Get normal brightness from dropdown (now just percentages)
        uint16_t normal_idx = lv_dropdown_get_selected(normal_brightness_dropdown);
        uint8_t normal_brightness_values[] = {25, 50, 75, 100};  // 25%, 50%, 75%, 100%
        if (normal_idx < 4) {
            PowerManager::setNormalBrightness(normal_brightness_values[normal_idx]);
        }
        
        // Get dim brightness from dropdown (now just percentages) (now just percentages)
        uint16_t bright_idx = lv_dropdown_get_selected(dim_brightness_dropdown);
        uint8_t brightness_values[] = {5, 10, 25, 50};  // 5%, 10%, 25%, 50%
        if (bright_idx < 4) {
            PowerManager::setDimBrightness(brightness_values[bright_idx]);
        }
        
        // Get deep sleep timeout from dropdown
        uint16_t deep_idx = lv_dropdown_get_selected(deep_sleep_timeout_dropdown);
        uint32_t deep_sleep_seconds[] = {0, 300, 600, 900, 1800, 3600, 5400};  // 0=disabled, 5/10/15/30/60/90 min
        if (deep_idx < 7) {
            PowerManager::setDeepSleepTimeout(deep_sleep_seconds[deep_idx]);
        }
        
        // Save power manager settings to NVS
        PowerManager::saveSettings();
        
        // Apply the new normal brightness immediately
        PowerManager::applyNormalBrightness();
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Settings saved!");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset power management to defaults
        lv_obj_add_state(power_mgmt_switch, LV_STATE_CHECKED);
        lv_dropdown_set_selected(dim_timeout_dropdown, 2);          // 30 sec (index shifted by 1 due to Disabled)
        lv_dropdown_set_selected(sleep_timeout_dropdown, 3);        // 5 min (index shifted by 1 due to Disabled)
        lv_dropdown_set_selected(normal_brightness_dropdown, 3);    // 100%
        lv_dropdown_set_selected(dim_brightness_dropdown, 2);       // 25%
        lv_dropdown_set_selected(deep_sleep_timeout_dropdown, 3);   // 15 min
        
        // Enable power management dropdowns
        lv_obj_clear_state(dim_timeout_dropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(sleep_timeout_dropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(normal_brightness_dropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(dim_brightness_dropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(deep_sleep_timeout_dropdown, LV_STATE_DISABLED);
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("Power Reset button clicked");
    }
}

// Power management switch event handler
static void power_mgmt_switch_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        bool enabled = lv_obj_has_state(power_mgmt_switch, LV_STATE_CHECKED);
        
        // Enable or disable the power management dropdowns
        if (enabled) {
            lv_obj_clear_state(dim_timeout_dropdown, LV_STATE_DISABLED);
            lv_obj_clear_state(sleep_timeout_dropdown, LV_STATE_DISABLED);
            lv_obj_clear_state(normal_brightness_dropdown, LV_STATE_DISABLED);
            lv_obj_clear_state(dim_brightness_dropdown, LV_STATE_DISABLED);
            lv_obj_clear_state(deep_sleep_timeout_dropdown, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(dim_timeout_dropdown, LV_STATE_DISABLED);
            lv_obj_add_state(sleep_timeout_dropdown, LV_STATE_DISABLED);
            lv_obj_add_state(normal_brightness_dropdown, LV_STATE_DISABLED);
            lv_obj_add_state(dim_brightness_dropdown, LV_STATE_DISABLED);
            lv_obj_add_state(deep_sleep_timeout_dropdown, LV_STATE_DISABLED);
        }
    }
}
