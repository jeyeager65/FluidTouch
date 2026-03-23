#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/machine_config.h"
#include "config.h"
#include <Preferences.h>

// Static member initialization - default values
float UITabSettingsJog::default_xy_step = 10.0f;
float UITabSettingsJog::default_z_step = 1.0f;
float UITabSettingsJog::default_a_step = 1.0f;
int UITabSettingsJog::default_xy_feed = 3000;
int UITabSettingsJog::default_z_feed = 1000;
int UITabSettingsJog::default_a_feed = 1000;
int UITabSettingsJog::max_xy_feed = 3000;
int UITabSettingsJog::max_z_feed = 1000;
int UITabSettingsJog::max_a_feed = 1000;
char UITabSettingsJog::xy_steps[64] = "100,50,10,1,0.1";
char UITabSettingsJog::z_steps[64] = "50,25,10,1,0.1";
char UITabSettingsJog::a_steps[64] = "50,25,10,1,0.1";

// Keyboard
lv_obj_t *UITabSettingsJog::keyboard = nullptr;
lv_obj_t *UITabSettingsJog::parent_tab = nullptr;

// UI element references
static lv_obj_t *ta_xy_step = nullptr;
static lv_obj_t *ta_z_step = nullptr;
static lv_obj_t *ta_a_step = nullptr;
static lv_obj_t *ta_xy_feed = nullptr;
static lv_obj_t *ta_z_feed = nullptr;
static lv_obj_t *ta_a_feed = nullptr;
static lv_obj_t *ta_max_xy_feed = nullptr;
static lv_obj_t *ta_max_z_feed = nullptr;
static lv_obj_t *ta_max_a_feed = nullptr;
static lv_obj_t *ta_xy_steps = nullptr;
static lv_obj_t *ta_z_steps = nullptr;
static lv_obj_t *ta_a_steps = nullptr;
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

    // 5-column layout: Axis | Step | Feed | Max | Steps
    int col1_x = 20;      // Axis label
    int col2_x = 70;      // Step field (85px wide)
    int col3_x = 165;     // Feed field (85px wide)
    int col4_x = 260;     // Max field (85px wide)
    int col5_x = 355;     // Steps field (220px wide)

    // Title
    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "JOG & JOYSTICK CONTROL DEFAULTS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(title, col1_x, y_pos);

    y_pos += 30;

    // Column headers - two lines for headers with units
    lv_obj_t *hdr_step = lv_label_create(tab);
    lv_label_set_text(hdr_step, "Step");
    lv_obj_set_style_text_font(hdr_step, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_step, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_step, col2_x, y_pos);

    lv_obj_t *hdr_step_unit = lv_label_create(tab);
    lv_label_set_text(hdr_step_unit, "(mm)");
    lv_obj_set_style_text_font(hdr_step_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr_step_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_step_unit, col2_x, y_pos + 18);

    lv_obj_t *hdr_feed = lv_label_create(tab);
    lv_label_set_text(hdr_feed, "Feed");
    lv_obj_set_style_text_font(hdr_feed, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_feed, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_feed, col3_x, y_pos);

    lv_obj_t *hdr_feed_unit = lv_label_create(tab);
    lv_label_set_text(hdr_feed_unit, "(mm/min)");
    lv_obj_set_style_text_font(hdr_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr_feed_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_feed_unit, col3_x, y_pos + 18);

    lv_obj_t *hdr_max = lv_label_create(tab);
    lv_label_set_text(hdr_max, "Max");
    lv_obj_set_style_text_font(hdr_max, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_max, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_max, col4_x, y_pos);

    lv_obj_t *hdr_max_unit = lv_label_create(tab);
    lv_label_set_text(hdr_max_unit, "(mm/min)");
    lv_obj_set_style_text_font(hdr_max_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr_max_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_max_unit, col4_x, y_pos + 18);

    // Headers without units - positioned at second line Y value
    lv_obj_t *hdr_axis = lv_label_create(tab);
    lv_label_set_text(hdr_axis, "Axis");
    lv_obj_set_style_text_font(hdr_axis, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_axis, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_axis, col1_x, y_pos + 18);

    lv_obj_t *hdr_steps = lv_label_create(tab);
    lv_label_set_text(hdr_steps, "Step List");
    lv_obj_set_style_text_font(hdr_steps, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hdr_steps, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_steps, col5_x, y_pos);

    lv_obj_t *hdr_steps_unit = lv_label_create(tab);
    lv_label_set_text(hdr_steps_unit, "(comma separated, max 5)");
    lv_obj_set_style_text_font(hdr_steps_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr_steps_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(hdr_steps_unit, col5_x, y_pos + 18);

    y_pos += 40;

    char buf[64];

    // ========== XY ROW ==========
    lv_obj_t *lbl_xy = lv_label_create(tab);
    lv_label_set_text(lbl_xy, "XY");
    lv_obj_set_style_text_font(lbl_xy, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_xy, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(lbl_xy, col1_x, y_pos + 8);

    ta_xy_step = lv_textarea_create(tab);
    lv_obj_set_size(ta_xy_step, 85, 40);
    lv_obj_set_pos(ta_xy_step, col2_x, y_pos);
    lv_textarea_set_one_line(ta_xy_step, true);
    lv_textarea_set_max_length(ta_xy_step, 6);
    lv_textarea_set_accepted_chars(ta_xy_step, "0123456789");
    lv_obj_set_style_text_font(ta_xy_step, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_xy_step, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%.0f", default_xy_step);
    lv_textarea_set_text(ta_xy_step, buf);

    ta_xy_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_xy_feed, 85, 40);
    lv_obj_set_pos(ta_xy_feed, col3_x, y_pos);
    lv_textarea_set_one_line(ta_xy_feed, true);
    lv_textarea_set_max_length(ta_xy_feed, 6);
    lv_textarea_set_accepted_chars(ta_xy_feed, "0123456789");
    lv_obj_set_style_text_font(ta_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_xy_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_xy_feed);
    lv_textarea_set_text(ta_xy_feed, buf);

    ta_max_xy_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_max_xy_feed, 85, 40);
    lv_obj_set_pos(ta_max_xy_feed, col4_x, y_pos);
    lv_textarea_set_one_line(ta_max_xy_feed, true);
    lv_textarea_set_max_length(ta_max_xy_feed, 6);
    lv_textarea_set_accepted_chars(ta_max_xy_feed, "0123456789");
    lv_obj_set_style_text_font(ta_max_xy_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_max_xy_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", max_xy_feed);
    lv_textarea_set_text(ta_max_xy_feed, buf);

    ta_xy_steps = lv_textarea_create(tab);
    lv_obj_set_size(ta_xy_steps, 220, 40);
    lv_obj_set_pos(ta_xy_steps, col5_x, y_pos);
    lv_textarea_set_one_line(ta_xy_steps, true);
    lv_textarea_set_max_length(ta_xy_steps, 60);
    lv_textarea_set_accepted_chars(ta_xy_steps, "0123456789,.");
    lv_obj_set_style_text_font(ta_xy_steps, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_xy_steps, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    lv_textarea_set_text(ta_xy_steps, xy_steps);

    y_pos += 50;

    // ========== Z ROW ==========
    lv_obj_t *lbl_z = lv_label_create(tab);
    lv_label_set_text(lbl_z, "Z");
    lv_obj_set_style_text_font(lbl_z, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_z, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(lbl_z, col1_x, y_pos + 8);

    ta_z_step = lv_textarea_create(tab);
    lv_obj_set_size(ta_z_step, 85, 40);
    lv_obj_set_pos(ta_z_step, col2_x, y_pos);
    lv_textarea_set_one_line(ta_z_step, true);
    lv_textarea_set_max_length(ta_z_step, 6);
    lv_textarea_set_accepted_chars(ta_z_step, "0123456789");
    lv_obj_set_style_text_font(ta_z_step, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_z_step, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%.0f", default_z_step);
    lv_textarea_set_text(ta_z_step, buf);

    ta_z_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_z_feed, 85, 40);
    lv_obj_set_pos(ta_z_feed, col3_x, y_pos);
    lv_textarea_set_one_line(ta_z_feed, true);
    lv_textarea_set_max_length(ta_z_feed, 6);
    lv_textarea_set_accepted_chars(ta_z_feed, "0123456789");
    lv_obj_set_style_text_font(ta_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_z_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", default_z_feed);
    lv_textarea_set_text(ta_z_feed, buf);

    ta_max_z_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_max_z_feed, 85, 40);
    lv_obj_set_pos(ta_max_z_feed, col4_x, y_pos);
    lv_textarea_set_one_line(ta_max_z_feed, true);
    lv_textarea_set_max_length(ta_max_z_feed, 6);
    lv_textarea_set_accepted_chars(ta_max_z_feed, "0123456789");
    lv_obj_set_style_text_font(ta_max_z_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_max_z_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    snprintf(buf, sizeof(buf), "%d", max_z_feed);
    lv_textarea_set_text(ta_max_z_feed, buf);

    ta_z_steps = lv_textarea_create(tab);
    lv_obj_set_size(ta_z_steps, 220, 40);
    lv_obj_set_pos(ta_z_steps, col5_x, y_pos);
    lv_textarea_set_one_line(ta_z_steps, true);
    lv_textarea_set_max_length(ta_z_steps, 60);
    lv_textarea_set_accepted_chars(ta_z_steps, "0123456789,.");
    lv_obj_set_style_text_font(ta_z_steps, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_z_steps, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    lv_textarea_set_text(ta_z_steps, z_steps);

    y_pos += 50;

    // ========== A ROW (conditional) ==========
    if (UICommon::isAAxisEnabled()) {
        lv_obj_t *lbl_a = lv_label_create(tab);
        lv_label_set_text(lbl_a, "A");
        lv_obj_set_style_text_font(lbl_a, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_a, UITheme::AXIS_A, 0);
        lv_obj_set_pos(lbl_a, col1_x, y_pos + 8);

        ta_a_step = lv_textarea_create(tab);
        lv_obj_set_size(ta_a_step, 85, 40);
        lv_obj_set_pos(ta_a_step, col2_x, y_pos);
        lv_textarea_set_one_line(ta_a_step, true);
        lv_textarea_set_max_length(ta_a_step, 6);
        lv_textarea_set_accepted_chars(ta_a_step, "0123456789");
        lv_obj_set_style_text_font(ta_a_step, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_a_step, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
        snprintf(buf, sizeof(buf), "%.0f", default_a_step);
        lv_textarea_set_text(ta_a_step, buf);

        ta_a_feed = lv_textarea_create(tab);
        lv_obj_set_size(ta_a_feed, 85, 40);
        lv_obj_set_pos(ta_a_feed, col3_x, y_pos);
        lv_textarea_set_one_line(ta_a_feed, true);
        lv_textarea_set_max_length(ta_a_feed, 6);
        lv_textarea_set_accepted_chars(ta_a_feed, "0123456789");
        lv_obj_set_style_text_font(ta_a_feed, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_a_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
        snprintf(buf, sizeof(buf), "%d", default_a_feed);
        lv_textarea_set_text(ta_a_feed, buf);

        ta_max_a_feed = lv_textarea_create(tab);
        lv_obj_set_size(ta_max_a_feed, 85, 40);
        lv_obj_set_pos(ta_max_a_feed, col4_x, y_pos);
        lv_textarea_set_one_line(ta_max_a_feed, true);
        lv_textarea_set_max_length(ta_max_a_feed, 6);
        lv_textarea_set_accepted_chars(ta_max_a_feed, "0123456789");
        lv_obj_set_style_text_font(ta_max_a_feed, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_max_a_feed, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
        snprintf(buf, sizeof(buf), "%d", max_a_feed);
        lv_textarea_set_text(ta_max_a_feed, buf);

        ta_a_steps = lv_textarea_create(tab);
        lv_obj_set_size(ta_a_steps, 220, 40);
        lv_obj_set_pos(ta_a_steps, col5_x, y_pos);
        lv_textarea_set_one_line(ta_a_steps, true);
        lv_textarea_set_max_length(ta_a_steps, 60);
        lv_textarea_set_accepted_chars(ta_a_steps, "0123456789,.");
        lv_obj_set_style_text_font(ta_a_steps, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_a_steps, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
        lv_textarea_set_text(ta_a_steps, a_steps);

        y_pos += 50;
    }

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
        default_a_step = config.jog_a_step;
        default_xy_feed = config.jog_xy_feed;
        default_z_feed = config.jog_z_feed;
        default_a_feed = config.jog_a_feed;
        max_xy_feed = config.jog_max_xy_feed;
        max_z_feed = config.jog_max_z_feed;
        max_a_feed = config.jog_max_a_feed;
        strncpy(xy_steps, config.jog_xy_steps, sizeof(xy_steps) - 1);
        strncpy(z_steps, config.jog_z_steps, sizeof(z_steps) - 1);
        strncpy(a_steps, config.jog_a_steps, sizeof(a_steps) - 1);

        Serial.printf("Jog settings loaded for machine %d:\n", machineIndex);
        Serial.printf("  XY Step: %.0f mm\n", default_xy_step);
        Serial.printf("  Z Step: %.0f mm\n", default_z_step);
        Serial.printf("  A Step: %.0f\n", default_a_step);
        Serial.printf("  XY Feed: %d mm/min\n", default_xy_feed);
        Serial.printf("  Z Feed: %d mm/min\n", default_z_feed);
        Serial.printf("  A Feed: %d\n", default_a_feed);
        Serial.printf("  Max XY Feed: %d mm/min\n", max_xy_feed);
        Serial.printf("  Max Z Feed: %d mm/min\n", max_z_feed);
        Serial.printf("  Max A Feed: %d\n", max_a_feed);
        Serial.printf("  XY Steps: %s\n", xy_steps);
        Serial.printf("  Z Steps: %s\n", z_steps);
        Serial.printf("  A Steps: %s\n", a_steps);
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
        config.jog_a_step = default_a_step;
        config.jog_xy_feed = default_xy_feed;
        config.jog_z_feed = default_z_feed;
        config.jog_a_feed = default_a_feed;
        config.jog_max_xy_feed = max_xy_feed;
        config.jog_max_z_feed = max_z_feed;
        config.jog_max_a_feed = max_a_feed;
        strncpy(config.jog_xy_steps, xy_steps, sizeof(config.jog_xy_steps) - 1);
        strncpy(config.jog_z_steps, z_steps, sizeof(config.jog_z_steps) - 1);
        strncpy(config.jog_a_steps, a_steps, sizeof(config.jog_a_steps) - 1);
        
        if (MachineConfigManager::saveMachine(machineIndex, config)) {
            Serial.println("Jog settings saved successfully");
        } else {
            Serial.println("Failed to save jog settings");
        }
    } else {
        Serial.println("Failed to get machine config for saving");
    }
}

// Getters
float UITabSettingsJog::getDefaultXYStep() { return default_xy_step; }
float UITabSettingsJog::getDefaultZStep() { return default_z_step; }
float UITabSettingsJog::getDefaultAStep() { return default_a_step; }
int UITabSettingsJog::getDefaultXYFeed() { return default_xy_feed; }
int UITabSettingsJog::getDefaultZFeed() { return default_z_feed; }
int UITabSettingsJog::getDefaultAFeed() { return default_a_feed; }
int UITabSettingsJog::getMaxXYFeed() { return max_xy_feed; }
int UITabSettingsJog::getMaxZFeed() { return max_z_feed; }
int UITabSettingsJog::getMaxAFeed() { return max_a_feed; }
const char* UITabSettingsJog::getXYSteps() { return xy_steps; }
const char* UITabSettingsJog::getZSteps() { return z_steps; }
const char* UITabSettingsJog::getASteps() { return a_steps; }

// Setters
void UITabSettingsJog::setDefaultXYStep(float value) { default_xy_step = value; }
void UITabSettingsJog::setDefaultZStep(float value) { default_z_step = value; }
void UITabSettingsJog::setDefaultAStep(float value) { default_a_step = value; }
void UITabSettingsJog::setDefaultXYFeed(int value) { default_xy_feed = value; }
void UITabSettingsJog::setDefaultZFeed(int value) { default_z_feed = value; }
void UITabSettingsJog::setDefaultAFeed(int value) { default_a_feed = value; }
void UITabSettingsJog::setMaxXYFeed(int value) { max_xy_feed = value; }
void UITabSettingsJog::setMaxZFeed(int value) { max_z_feed = value; }
void UITabSettingsJog::setMaxAFeed(int value) { max_a_feed = value; }
void UITabSettingsJog::setXYSteps(const char* value) { strncpy(xy_steps, value, sizeof(xy_steps) - 1); xy_steps[sizeof(xy_steps) - 1] = '\0'; }
void UITabSettingsJog::setZSteps(const char* value) { strncpy(z_steps, value, sizeof(z_steps) - 1); z_steps[sizeof(z_steps) - 1] = '\0'; }
void UITabSettingsJog::setASteps(const char* value) { strncpy(a_steps, value, sizeof(a_steps) - 1); a_steps[sizeof(a_steps) - 1] = '\0'; }

// Validate step list string - returns true if valid, false otherwise
static bool validateStepList(const char* step_list, char* error_msg, size_t error_msg_size) {
    if (!step_list || strlen(step_list) == 0) {
        snprintf(error_msg, error_msg_size, "Step list cannot be empty");
        return false;
    }
    
    // Make a copy for parsing
    char buffer[64];
    strncpy(buffer, step_list, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    int count = 0;
    bool valid_number_found = false;
    char* token = strtok(buffer, ",");
    
    while (token != nullptr) {
        count++;
        
        // Check if count exceeds max
        if (count > 5) {
            snprintf(error_msg, error_msg_size, "Maximum 5 values allowed");
            return false;
        }
        
        // Trim whitespace from token
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';
        
        // Check if token is a valid number
        if (strlen(token) == 0) {
            snprintf(error_msg, error_msg_size, "Empty value in list");
            return false;
        }
        
        // Validate that token contains only digits and optional decimal point
        bool has_decimal = false;
        for (size_t i = 0; i < strlen(token); i++) {
            char c = token[i];
            if (c == '.') {
                if (has_decimal) {
                    snprintf(error_msg, error_msg_size, "Invalid number format");
                    return false;
                }
                has_decimal = true;
            } else if (c < '0' || c > '9') {
                snprintf(error_msg, error_msg_size, "Invalid character in number");
                return false;
            }
        }
        
        // Check if value is positive and reasonable
        float value = atof(token);
        if (value <= 0.0f || value > 10000.0f) {
            snprintf(error_msg, error_msg_size, "Values must be 0.01-10000");
            return false;
        }
        
        valid_number_found = true;
        token = strtok(nullptr, ",");
    }
    
    if (!valid_number_found) {
        snprintf(error_msg, error_msg_size, "No valid values found");
        return false;
    }
    
    return true;
}

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
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabSettingsJog::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
        }
    }
    
    // Set keyboard mode based on which field is focused
    bool is_step_list = (ta == ta_xy_steps || ta == ta_z_steps || ta == ta_a_steps);
    if (is_step_list) {
        // For step list fields, use SPECIAL mode which includes comma and period
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_SPECIAL);
    } else {
        // For numeric fields (step, feed, max), use NUMBER mode
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
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
        const char *a_step_text = ta_a_step ? lv_textarea_get_text(ta_a_step) : "1";
        const char *xy_feed_text = lv_textarea_get_text(ta_xy_feed);
        const char *z_feed_text = lv_textarea_get_text(ta_z_feed);
        const char *a_feed_text = ta_a_feed ? lv_textarea_get_text(ta_a_feed) : "1000";
        const char *max_xy_feed_text = lv_textarea_get_text(ta_max_xy_feed);
        const char *max_z_feed_text = lv_textarea_get_text(ta_max_z_feed);
        const char *max_a_feed_text = ta_max_a_feed ? lv_textarea_get_text(ta_max_a_feed) : "3000";
        const char *xy_steps_text = lv_textarea_get_text(ta_xy_steps);
        const char *z_steps_text = lv_textarea_get_text(ta_z_steps);
        const char *a_steps_text = ta_a_steps ? lv_textarea_get_text(ta_a_steps) : "1,10,50";

        // Validate and update
        float xy_step_val = atof(xy_step_text);
        float z_step_val = atof(z_step_text);
        float a_step_val = atof(a_step_text);
        int xy_feed_val = atoi(xy_feed_text);
        int z_feed_val = atoi(z_feed_text);
        int a_feed_val = atoi(a_feed_text);
        int max_xy_feed_val = atoi(max_xy_feed_text);
        int max_z_feed_val = atoi(max_z_feed_text);
        int max_a_feed_val = atoi(max_a_feed_text);

        // Clamp to reasonable ranges
        if (xy_step_val < 0.1f) xy_step_val = 0.1f;
        if (xy_step_val > 500.0f) xy_step_val = 500.0f;
        if (z_step_val < 0.1f) z_step_val = 0.1f;
        if (z_step_val > 100.0f) z_step_val = 100.0f;
        if (a_step_val < 0.1f) a_step_val = 0.1f;
        if (a_step_val > 100.0f) a_step_val = 100.0f;
        if (xy_feed_val < 100) xy_feed_val = 100;
        if (xy_feed_val > 10000) xy_feed_val = 10000;
        if (z_feed_val < 50) z_feed_val = 50;
        if (z_feed_val > 5000) z_feed_val = 5000;
        if (a_feed_val < 50) a_feed_val = 50;
        if (a_feed_val > 5000) a_feed_val = 5000;
        if (max_xy_feed_val < 100) max_xy_feed_val = 100;
        if (max_xy_feed_val > 15000) max_xy_feed_val = 15000;
        if (max_z_feed_val < 50) max_z_feed_val = 50;
        if (max_z_feed_val > 10000) max_z_feed_val = 10000;
        if (max_a_feed_val < 50) max_a_feed_val = 50;
        if (max_a_feed_val > 10000) max_a_feed_val = 10000;

        // Validate step lists
        char error_msg[64];
        if (!validateStepList(xy_steps_text, error_msg, sizeof(error_msg))) {
            if (status_label != nullptr) {
                char full_msg[80];
                snprintf(full_msg, sizeof(full_msg), "XY Steps: %s", error_msg);
                lv_label_set_text(status_label, full_msg);
                lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
            }
            Serial.printf("XY step list validation failed: %s\n", error_msg);
            return;
        }
        
        if (!validateStepList(z_steps_text, error_msg, sizeof(error_msg))) {
            if (status_label != nullptr) {
                char full_msg[80];
                snprintf(full_msg, sizeof(full_msg), "Z Steps: %s", error_msg);
                lv_label_set_text(status_label, full_msg);
                lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
            }
            Serial.printf("Z step list validation failed: %s\n", error_msg);
            return;
        }
        
        if (UICommon::isAAxisEnabled() && !validateStepList(a_steps_text, error_msg, sizeof(error_msg))) {
            if (status_label != nullptr) {
                char full_msg[80];
                snprintf(full_msg, sizeof(full_msg), "A Steps: %s", error_msg);
                lv_label_set_text(status_label, full_msg);
                lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
            }
            Serial.printf("A step list validation failed: %s\n", error_msg);
            return;
        }

        UITabSettingsJog::setDefaultXYStep(xy_step_val);
        UITabSettingsJog::setDefaultZStep(z_step_val);
        UITabSettingsJog::setDefaultAStep(a_step_val);
        UITabSettingsJog::setDefaultXYFeed(xy_feed_val);
        UITabSettingsJog::setDefaultZFeed(z_feed_val);
        UITabSettingsJog::setDefaultAFeed(a_feed_val);
        UITabSettingsJog::setMaxXYFeed(max_xy_feed_val);
        UITabSettingsJog::setMaxZFeed(max_z_feed_val);
        UITabSettingsJog::setMaxAFeed(max_a_feed_val);
        UITabSettingsJog::setXYSteps(xy_steps_text);
        UITabSettingsJog::setZSteps(z_steps_text);
        UITabSettingsJog::setASteps(a_steps_text);
        
        UITabSettingsJog::savePreferences();
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Settings saved! Restart to apply.");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
        Serial.println("Jog settings saved");
    }
}

// Reset to defaults button event handler
// TODO - Also reset step list
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to hardcoded defaults
        lv_textarea_set_text(ta_xy_step, "10.0");
        lv_textarea_set_text(ta_z_step, "1.0");
        if (ta_a_step) lv_textarea_set_text(ta_a_step, "1.0");
        lv_textarea_set_text(ta_xy_feed, "3000");
        lv_textarea_set_text(ta_z_feed, "1000");
        if (ta_a_feed) lv_textarea_set_text(ta_a_feed, "1000");
        lv_textarea_set_text(ta_max_xy_feed, "8000");
        lv_textarea_set_text(ta_max_z_feed, "3000");
        if (ta_max_a_feed) lv_textarea_set_text(ta_max_a_feed, "3000");
        
        if (status_label != nullptr) {
            lv_label_set_text(status_label, "Reset to defaults (not saved)");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("Jog settings reset to defaults");
    }
}
