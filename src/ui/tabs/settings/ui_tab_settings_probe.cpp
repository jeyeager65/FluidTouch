#include "ui/tabs/settings/ui_tab_settings_probe.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "config.h"
#include <Preferences.h>

// Static member initialization - default values
int UITabSettingsProbe::default_feed_rate = 100;
int UITabSettingsProbe::default_max_distance = 10;
int UITabSettingsProbe::default_retract = 2;
float UITabSettingsProbe::default_thickness = 0.0f;

// Keyboard
lv_obj_t *UITabSettingsProbe::keyboard = nullptr;
lv_obj_t *UITabSettingsProbe::parent_tab = nullptr;

// UI element references
static lv_obj_t *ta_feed = nullptr;
static lv_obj_t *ta_dist = nullptr;
static lv_obj_t *ta_retract = nullptr;
static lv_obj_t *ta_thickness = nullptr;
static lv_obj_t *status_label = nullptr;

// Forward declarations for event handlers
static void btn_save_probe_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);
static void textarea_focused_event_handler(lv_event_t *e);

void UITabSettingsProbe::create(lv_obj_t *tab) {
    // Store parent tab reference
    parent_tab = tab;
    
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Load preferences
    loadPreferences();
    
    int y_pos = 20;
    int label_x = 20;
    int field_x = 250;  // Shifted 50px right (was 200)
    
    // Title
    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "PROBE CONTROL DEFAULTS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);  // Gray color
    lv_obj_set_pos(title, label_x, y_pos);
    
    y_pos += 40;
    
    // === Feed Rate ===
    lv_obj_t *lbl_feed = lv_label_create(tab);
    lv_label_set_text(lbl_feed, "Feed Rate (mm/min):");
    lv_obj_set_style_text_font(lbl_feed, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_feed, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_feed, label_x, y_pos + 12);  // Align with text area content
    
    ta_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_feed, 100, 40);
    lv_obj_set_pos(ta_feed, field_x, y_pos);
    lv_textarea_set_one_line(ta_feed, true);
    lv_textarea_set_max_length(ta_feed, 6);
    lv_textarea_set_accepted_chars(ta_feed, "0123456789");
    lv_obj_set_style_text_font(ta_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", default_feed_rate);
    lv_textarea_set_text(ta_feed, buf);
    y_pos += 50;
    
    // === Max Distance ===
    lv_obj_t *lbl_dist = lv_label_create(tab);
    lv_label_set_text(lbl_dist, "Max Distance (mm):");
    lv_obj_set_style_text_font(lbl_dist, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_dist, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_dist, label_x, y_pos + 12);  // Align with text area content
    
    ta_dist = lv_textarea_create(tab);
    lv_obj_set_size(ta_dist, 100, 40);
    lv_obj_set_pos(ta_dist, field_x, y_pos);
    lv_textarea_set_one_line(ta_dist, true);
    lv_textarea_set_max_length(ta_dist, 6);
    lv_textarea_set_accepted_chars(ta_dist, "0123456789");
    lv_obj_set_style_text_font(ta_dist, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_dist, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_max_distance);
    lv_textarea_set_text(ta_dist, buf);
    y_pos += 50;
    
    // === Retract Distance ===
    lv_obj_t *lbl_retract = lv_label_create(tab);
    lv_label_set_text(lbl_retract, "Retract (mm):");
    lv_obj_set_style_text_font(lbl_retract, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_retract, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_retract, label_x, y_pos + 12);  // Align with text area content
    
    ta_retract = lv_textarea_create(tab);
    lv_obj_set_size(ta_retract, 100, 40);
    lv_obj_set_pos(ta_retract, field_x, y_pos);
    lv_textarea_set_one_line(ta_retract, true);
    lv_textarea_set_max_length(ta_retract, 6);
    lv_textarea_set_accepted_chars(ta_retract, "0123456789");
    lv_obj_set_style_text_font(ta_retract, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_retract, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_retract);
    lv_textarea_set_text(ta_retract, buf);
    y_pos += 50;
    
    // === Probe Thickness ===
    lv_obj_t *lbl_thickness = lv_label_create(tab);
    lv_label_set_text(lbl_thickness, "Probe Thickness (mm):");
    lv_obj_set_style_text_font(lbl_thickness, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_thickness, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_thickness, label_x, y_pos + 12);  // Align with text area content
    
    ta_thickness = lv_textarea_create(tab);
    lv_obj_set_size(ta_thickness, 100, 40);
    lv_obj_set_pos(ta_thickness, field_x, y_pos);
    lv_textarea_set_one_line(ta_thickness, true);
    lv_textarea_set_max_length(ta_thickness, 8);
    lv_textarea_set_accepted_chars(ta_thickness, "0123456789.");
    lv_obj_set_style_text_font(ta_thickness, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_thickness, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%.1f", default_thickness);
    lv_textarea_set_text(ta_thickness, buf);
    y_pos += 50;
    
    // === Action Buttons (positioned at bottom with 20px margins) ===
    // Save button
    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 180, 50);
    lv_obj_set_pos(btn_save, 20, 280);  // 360px (tab height) - 50px (button) - 30px (margin) = 280px
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save Settings");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, btn_save_probe_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Reset to defaults button
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, 180, 50);
    lv_obj_set_pos(btn_reset, 220, 280);  // Same vertical position, 200px gap from Save button
    lv_obj_set_style_bg_color(btn_reset, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset Defaults");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, btn_reset_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Status label (positioned below buttons with 10px gap)
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 20, 335);  // 280 (button y) + 50 (button height) + 5 (gap)
}

// Load preferences from current machine configuration
void UITabSettingsProbe::loadPreferences() {
    int machineIndex = MachineConfigManager::getSelectedMachineIndex();
    MachineConfig config;
    
    if (MachineConfigManager::getMachine(machineIndex, config)) {
        default_feed_rate = config.probe_feed_rate;
        default_max_distance = config.probe_max_distance;
        default_retract = config.probe_retract;
        default_thickness = config.probe_thickness;
        
        Serial.printf("Probe settings loaded for machine %d:\n", machineIndex);
        Serial.printf("  Feed Rate: %d mm/min\n", default_feed_rate);
        Serial.printf("  Max Distance: %d mm\n", default_max_distance);
        Serial.printf("  Retract: %d mm\n", default_retract);
        Serial.printf("  Thickness: %.1f mm\n", default_thickness);
    } else {
        Serial.println("Failed to load machine config, using defaults");
    }
}

// Save preferences to current machine configuration
void UITabSettingsProbe::savePreferences() {
    int machineIndex = MachineConfigManager::getSelectedMachineIndex();
    MachineConfig config;
    
    if (MachineConfigManager::getMachine(machineIndex, config)) {
        config.probe_feed_rate = default_feed_rate;
        config.probe_max_distance = default_max_distance;
        config.probe_retract = default_retract;
        config.probe_thickness = default_thickness;
        
        MachineConfigManager::saveMachine(machineIndex, config);
        Serial.printf("Probe settings saved for machine %d\n", machineIndex);
    } else {
        Serial.println("Failed to save machine config");
    }
}

// Getters
int UITabSettingsProbe::getDefaultFeedRate() { return default_feed_rate; }
int UITabSettingsProbe::getDefaultMaxDistance() { return default_max_distance; }
int UITabSettingsProbe::getDefaultRetract() { return default_retract; }
float UITabSettingsProbe::getDefaultThickness() { return default_thickness; }

// Setters
void UITabSettingsProbe::setDefaultFeedRate(int value) { default_feed_rate = value; }
void UITabSettingsProbe::setDefaultMaxDistance(int value) { default_max_distance = value; }
void UITabSettingsProbe::setDefaultRetract(int value) { default_retract = value; }
void UITabSettingsProbe::setDefaultThickness(float value) { default_thickness = value; }

// Textarea focused event handler - show keyboard
static void textarea_focused_event_handler(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    UITabSettingsProbe::showKeyboard(ta);
}

// Show keyboard
void UITabSettingsProbe::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, 220);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);  // All probe settings are numeric
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsProbe::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsProbe::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabSettingsProbe::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
        }
    }
    
    // Enable scrolling on parent tab and add extra padding at bottom for keyboard (every time keyboard is shown)
    if (parent_tab) {
        lv_obj_add_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_bottom(parent_tab, 240, 0); // Extra space for scrolling (keyboard height + margin)
    }
    
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll the parent tab to position the focused textarea just above keyboard
    if (parent_tab && ta) {
        // Get textarea position within parent_tab
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_obj_t *parent = lv_obj_get_parent(ta);
        
        // Walk up parent hierarchy to get cumulative Y position
        while (parent && parent != parent_tab) {
            ta_y += lv_obj_get_y(parent);
            parent = lv_obj_get_parent(parent);
        }
        
        // Calculate scroll position to place textarea just above keyboard
        // Status bar is 60px, keyboard is 220px, so visible area is 200px (480 - 60 - 220)
        lv_coord_t visible_height = 200; // Height above keyboard and below status bar
        lv_coord_t ta_height = lv_obj_get_height(ta);
        lv_coord_t target_position = visible_height - ta_height - 10; // 10px margin above keyboard
        
        // Scroll amount = (textarea Y position) - (where we want it)
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) scroll_y = 0; // Don't scroll past top
        
        lv_obj_scroll_to_y(parent_tab, scroll_y, LV_ANIM_ON);
    }
}

// Hide keyboard
void UITabSettingsProbe::hideKeyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // Restore parent tab to non-scrollable and remove extra padding
        if (parent_tab) {
            lv_obj_clear_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_bottom(parent_tab, 10, 0); // Back to original padding
            lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_ON); // Reset scroll position
        }
    }
}

// Save button event handler
static void btn_save_probe_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Read values from text areas
        const char *feed_text = lv_textarea_get_text(ta_feed);
        const char *dist_text = lv_textarea_get_text(ta_dist);
        const char *retract_text = lv_textarea_get_text(ta_retract);
        const char *thickness_text = lv_textarea_get_text(ta_thickness);
        
        // Validate and update
        int feed_val = atoi(feed_text);
        int dist_val = atoi(dist_text);
        int retract_val = atoi(retract_text);
        float thickness_val = atof(thickness_text);
        
        // Clamp to reasonable ranges
        if (feed_val < 10) feed_val = 10;
        if (feed_val > 5000) feed_val = 5000;
        if (dist_val < 1) dist_val = 1;
        if (dist_val > 500) dist_val = 500;
        if (retract_val < 0) retract_val = 0;
        if (retract_val > 100) retract_val = 100;
        if (thickness_val < 0.0f) thickness_val = 0.0f;
        if (thickness_val > 100.0f) thickness_val = 100.0f;
        
        UITabSettingsProbe::setDefaultFeedRate(feed_val);
        UITabSettingsProbe::setDefaultMaxDistance(dist_val);
        UITabSettingsProbe::setDefaultRetract(retract_val);
        UITabSettingsProbe::setDefaultThickness(thickness_val);
        
        UITabSettingsProbe::savePreferences();
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Settings saved!");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
        Serial.println("Probe settings saved");
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to hardcoded defaults
        lv_textarea_set_text(ta_feed, "100");
        lv_textarea_set_text(ta_dist, "10");
        lv_textarea_set_text(ta_retract, "2");
        lv_textarea_set_text(ta_thickness, "0.0");
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Reset to defaults (not saved)");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("Probe settings reset to defaults");
    }
}
