#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "config.h"
#include <Preferences.h>

// Static member initialization - default values
float UITabSettingsJog::default_xy_step = 10.0f;
float UITabSettingsJog::default_z_step = 1.0f;
int UITabSettingsJog::default_xy_feed = 3000;
int UITabSettingsJog::default_z_feed = 1000;
int UITabSettingsJog::max_xy_feed = 3000;
int UITabSettingsJog::max_z_feed = 1000;

// Keyboard
lv_obj_t *UITabSettingsJog::keyboard = nullptr;
lv_obj_t *UITabSettingsJog::parent_tab = nullptr;

// UI element references
static lv_obj_t *ta_xy_step = nullptr;
static lv_obj_t *ta_z_step = nullptr;
static lv_obj_t *ta_xy_feed = nullptr;
static lv_obj_t *ta_z_feed = nullptr;
static lv_obj_t *ta_max_xy_feed = nullptr;
static lv_obj_t *ta_max_z_feed = nullptr;
static lv_obj_t *status_label = nullptr;

// Forward declarations for event handlers
static void btn_save_jog_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);
static void textarea_focused_event_handler(lv_event_t *e);

void UITabSettingsJog::create(lv_obj_t *tab) {
    // Store parent tab reference
    parent_tab = tab;
    
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Load preferences
    loadPreferences();
    
    int y_pos = 20;
    int col1_label_x = 20;
    int col1_field_x = 215;  // Shifted 50px right (was 165)
    int col2_label_x = 360;  // Moved 20px left (was 380)
    int col2_field_x = 555;  // Moved 20px left (was 575)
    
    // Title - Column 1
    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "JOG CONTROL DEFAULTS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);  // Gray color
    lv_obj_set_pos(title, col1_label_x, y_pos);
    
    // Title - Column 2
    lv_obj_t *title2 = lv_label_create(tab);
    lv_label_set_text(title2, "JOYSTICK CONTROL DEFAULTS");
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title2, UITheme::TEXT_DISABLED, 0);  // Gray color
    lv_obj_set_pos(title2, col2_label_x, y_pos);
    
    y_pos += 40;
    
    // === XY Step Size ===
    lv_obj_t *lbl_xy_step = lv_label_create(tab);
    lv_label_set_text(lbl_xy_step, "XY Step (mm):");
    lv_obj_set_style_text_font(lbl_xy_step, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_xy_step, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_xy_step, col1_label_x, y_pos + 12);  // Align with text area content
    
    ta_xy_step = lv_textarea_create(tab);
    lv_obj_set_size(ta_xy_step, 100, 40);
    lv_obj_set_pos(ta_xy_step, col1_field_x, y_pos);
    lv_textarea_set_one_line(ta_xy_step, true);
    lv_textarea_set_max_length(ta_xy_step, 6);
    lv_textarea_set_accepted_chars(ta_xy_step, "0123456789");
    lv_obj_set_style_text_font(ta_xy_step, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_xy_step, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", default_xy_step);
    lv_textarea_set_text(ta_xy_step, buf);
    
    // === Max XY Feed (Column 2) ===
    lv_obj_t *lbl_max_xy_feed = lv_label_create(tab);
    lv_label_set_text(lbl_max_xy_feed, "Max XY (mm/min):");
    lv_obj_set_style_text_font(lbl_max_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_max_xy_feed, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_max_xy_feed, col2_label_x, y_pos + 12);  // Align with text area content
    
    ta_max_xy_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_max_xy_feed, 100, 40);
    lv_obj_set_pos(ta_max_xy_feed, col2_field_x, y_pos);
    lv_textarea_set_one_line(ta_max_xy_feed, true);
    lv_textarea_set_max_length(ta_max_xy_feed, 6);
    lv_textarea_set_accepted_chars(ta_max_xy_feed, "0123456789");
    lv_obj_set_style_text_font(ta_max_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_max_xy_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", max_xy_feed);
    lv_textarea_set_text(ta_max_xy_feed, buf);
    y_pos += 50;
    
    // === Z Step Size ===
    lv_obj_t *lbl_z_step = lv_label_create(tab);
    lv_label_set_text(lbl_z_step, "Z Step (mm):");
    lv_obj_set_style_text_font(lbl_z_step, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_z_step, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_z_step, col1_label_x, y_pos + 12);  // Align with text area content
    
    ta_z_step = lv_textarea_create(tab);
    lv_obj_set_size(ta_z_step, 100, 40);
    lv_obj_set_pos(ta_z_step, col1_field_x, y_pos);
    lv_textarea_set_one_line(ta_z_step, true);
    lv_textarea_set_max_length(ta_z_step, 6);
    lv_textarea_set_accepted_chars(ta_z_step, "0123456789");
    lv_obj_set_style_text_font(ta_z_step, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_z_step, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%.0f", default_z_step);
    lv_textarea_set_text(ta_z_step, buf);
    
    // === Max Z Feed (Column 2) ===
    lv_obj_t *lbl_max_z_feed = lv_label_create(tab);
    lv_label_set_text(lbl_max_z_feed, "Max Z (mm/min):");
    lv_obj_set_style_text_font(lbl_max_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_max_z_feed, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_max_z_feed, col2_label_x, y_pos + 12);  // Align with text area content
    
    ta_max_z_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_max_z_feed, 100, 40);
    lv_obj_set_pos(ta_max_z_feed, col2_field_x, y_pos);
    lv_textarea_set_one_line(ta_max_z_feed, true);
    lv_textarea_set_max_length(ta_max_z_feed, 6);
    lv_textarea_set_accepted_chars(ta_max_z_feed, "0123456789");
    lv_obj_set_style_text_font(ta_max_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_max_z_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", max_z_feed);
    lv_textarea_set_text(ta_max_z_feed, buf);
    y_pos += 50;
    
    // === XY Feed Rate ===
    lv_obj_t *lbl_xy_feed = lv_label_create(tab);
    lv_label_set_text(lbl_xy_feed, "XY Feed (mm/min):");
    lv_obj_set_style_text_font(lbl_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_xy_feed, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_xy_feed, col1_label_x, y_pos + 12);  // Align with text area content
    
    ta_xy_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_xy_feed, 100, 40);
    lv_obj_set_pos(ta_xy_feed, col1_field_x, y_pos);
    lv_textarea_set_one_line(ta_xy_feed, true);
    lv_textarea_set_max_length(ta_xy_feed, 6);
    lv_textarea_set_accepted_chars(ta_xy_feed, "0123456789");
    lv_obj_set_style_text_font(ta_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_xy_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_xy_feed);
    lv_textarea_set_text(ta_xy_feed, buf);
    y_pos += 50;
    
    // === Z Feed Rate ===
    lv_obj_t *lbl_z_feed = lv_label_create(tab);
    lv_label_set_text(lbl_z_feed, "Z Feed (mm/min):");
    lv_obj_set_style_text_font(lbl_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_z_feed, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_z_feed, col1_label_x, y_pos + 12);  // Align with text area content
    
    ta_z_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_z_feed, 100, 40);
    lv_obj_set_pos(ta_z_feed, col1_field_x, y_pos);
    lv_textarea_set_one_line(ta_z_feed, true);
    lv_textarea_set_max_length(ta_z_feed, 6);
    lv_textarea_set_accepted_chars(ta_z_feed, "0123456789");
    lv_obj_set_style_text_font(ta_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_z_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_z_feed);
    lv_textarea_set_text(ta_z_feed, buf);
    y_pos += 60;
    
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
    lv_obj_add_event_cb(btn_save, btn_save_jog_event_handler, LV_EVENT_CLICKED, NULL);
    
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
    
    // Status label
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 20, 335);
}

// Load preferences from current machine configuration
void UITabSettingsJog::loadPreferences() {
    int machineIndex = MachineConfigManager::getSelectedMachineIndex();
    MachineConfig config;
    
    if (MachineConfigManager::getMachine(machineIndex, config)) {
        default_xy_step = config.jog_xy_step;
        default_z_step = config.jog_z_step;
        default_xy_feed = config.jog_xy_feed;
        default_z_feed = config.jog_z_feed;
        max_xy_feed = config.jog_max_xy_feed;
        max_z_feed = config.jog_max_z_feed;
        
        Serial.printf("Jog settings loaded for machine %d:\n", machineIndex);
        Serial.printf("  XY Step: %.0f mm\n", default_xy_step);
        Serial.printf("  Z Step: %.0f mm\n", default_z_step);
        Serial.printf("  XY Feed: %d mm/min\n", default_xy_feed);
        Serial.printf("  Z Feed: %d mm/min\n", default_z_feed);
        Serial.printf("  Max XY Feed: %d mm/min\n", max_xy_feed);
        Serial.printf("  Max Z Feed: %d mm/min\n", max_z_feed);
    } else {
        Serial.println("Failed to load machine config, using defaults");
    }
}

// Save preferences to current machine configuration
void UITabSettingsJog::savePreferences() {
    int machineIndex = MachineConfigManager::getSelectedMachineIndex();
    MachineConfig config;
    
    if (MachineConfigManager::getMachine(machineIndex, config)) {
        config.jog_xy_step = default_xy_step;
        config.jog_z_step = default_z_step;
        config.jog_xy_feed = default_xy_feed;
        config.jog_z_feed = default_z_feed;
        config.jog_max_xy_feed = max_xy_feed;
        config.jog_max_z_feed = max_z_feed;
        
        MachineConfigManager::saveMachine(machineIndex, config);
        Serial.printf("Jog settings saved for machine %d\n", machineIndex);
    } else {
        Serial.println("Failed to save machine config");
    }
}

// Getters
float UITabSettingsJog::getDefaultXYStep() { return default_xy_step; }
float UITabSettingsJog::getDefaultZStep() { return default_z_step; }
int UITabSettingsJog::getDefaultXYFeed() { return default_xy_feed; }
int UITabSettingsJog::getDefaultZFeed() { return default_z_feed; }
int UITabSettingsJog::getMaxXYFeed() { return max_xy_feed; }
int UITabSettingsJog::getMaxZFeed() { return max_z_feed; }

// Setters
void UITabSettingsJog::setDefaultXYStep(float value) { default_xy_step = value; }
void UITabSettingsJog::setDefaultZStep(float value) { default_z_step = value; }
void UITabSettingsJog::setDefaultXYFeed(int value) { default_xy_feed = value; }
void UITabSettingsJog::setDefaultZFeed(int value) { default_z_feed = value; }
void UITabSettingsJog::setMaxXYFeed(int value) { max_xy_feed = value; }
void UITabSettingsJog::setMaxZFeed(int value) { max_z_feed = value; }

// Textarea focused event handler - show keyboard
static void textarea_focused_event_handler(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    UITabSettingsJog::showKeyboard(ta);
}

// Show keyboard
void UITabSettingsJog::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, 220);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);  // All jog settings are numeric
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
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
void UITabSettingsJog::hideKeyboard() {
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
static void btn_save_jog_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Read values from text areas
        const char *xy_step_text = lv_textarea_get_text(ta_xy_step);
        const char *z_step_text = lv_textarea_get_text(ta_z_step);
        const char *xy_feed_text = lv_textarea_get_text(ta_xy_feed);
        const char *z_feed_text = lv_textarea_get_text(ta_z_feed);
        const char *max_xy_feed_text = lv_textarea_get_text(ta_max_xy_feed);
        const char *max_z_feed_text = lv_textarea_get_text(ta_max_z_feed);
        
        // Validate and update
        float xy_step_val = atof(xy_step_text);
        float z_step_val = atof(z_step_text);
        int xy_feed_val = atoi(xy_feed_text);
        int z_feed_val = atoi(z_feed_text);
        int max_xy_feed_val = atoi(max_xy_feed_text);
        int max_z_feed_val = atoi(max_z_feed_text);
        
        // Clamp to reasonable ranges
        if (xy_step_val < 0.1f) xy_step_val = 0.1f;
        if (xy_step_val > 500.0f) xy_step_val = 500.0f;
        if (z_step_val < 0.1f) z_step_val = 0.1f;
        if (z_step_val > 100.0f) z_step_val = 100.0f;
        if (xy_feed_val < 100) xy_feed_val = 100;
        if (xy_feed_val > 10000) xy_feed_val = 10000;
        if (z_feed_val < 50) z_feed_val = 50;
        if (z_feed_val > 5000) z_feed_val = 5000;
        if (max_xy_feed_val < 100) max_xy_feed_val = 100;
        if (max_xy_feed_val > 15000) max_xy_feed_val = 15000;
        if (max_z_feed_val < 50) max_z_feed_val = 50;
        if (max_z_feed_val > 10000) max_z_feed_val = 10000;
        
        UITabSettingsJog::setDefaultXYStep(xy_step_val);
        UITabSettingsJog::setDefaultZStep(z_step_val);
        UITabSettingsJog::setDefaultXYFeed(xy_feed_val);
        UITabSettingsJog::setDefaultZFeed(z_feed_val);
        UITabSettingsJog::setMaxXYFeed(max_xy_feed_val);
        UITabSettingsJog::setMaxZFeed(max_z_feed_val);
        
        UITabSettingsJog::savePreferences();
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Settings saved! Restart to apply.");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
        Serial.println("Jog settings saved");
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to hardcoded defaults
        lv_textarea_set_text(ta_xy_step, "10.0");
        lv_textarea_set_text(ta_z_step, "1.0");
        lv_textarea_set_text(ta_xy_feed, "3000");
        lv_textarea_set_text(ta_z_feed, "1000");
        lv_textarea_set_text(ta_max_xy_feed, "8000");
        lv_textarea_set_text(ta_max_z_feed, "3000");
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Reset to defaults (not saved)");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("Jog settings reset to defaults");
    }
}
