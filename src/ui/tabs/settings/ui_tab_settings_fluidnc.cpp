#include "ui/tabs/settings/ui_tab_settings_fluidnc.h"
#include "ui/ui_theme.h"
#include "config.h"

// Global references for UI elements
static lv_obj_t *status_label = NULL;

// Forward declarations for event handlers
static void btn_save_fluidnc_event_handler(lv_event_t *e);
static void btn_defaults_event_handler(lv_event_t *e);

void UITabSettingsFluidNC::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Placeholder text
    lv_obj_t *placeholder = lv_label_create(tab);
    lv_label_set_text(placeholder, "FluidNC Settings\n\nConfiguration options for FluidNC communication\nand machine-specific settings will appear here.");
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(placeholder, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(placeholder, 600);
    lv_obj_set_pos(placeholder, 40, 60);
    
    // === Action Buttons ===
    // Save button
    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 180, 50);
    lv_obj_set_pos(btn_save, 100, 240);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save Settings");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, btn_save_fluidnc_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Reset to defaults button
    lv_obj_t *btn_defaults = lv_button_create(tab);
    lv_obj_set_size(btn_defaults, 180, 50);
    lv_obj_set_pos(btn_defaults, 300, 240);
    lv_obj_set_style_bg_color(btn_defaults, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_t *lbl_defaults = lv_label_create(btn_defaults);
    lv_label_set_text(lbl_defaults, "Reset Defaults");
    lv_obj_set_style_text_font(lbl_defaults, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_defaults);
    lv_obj_add_event_cb(btn_defaults, btn_defaults_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Status label
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 100, 310);
}

// Save button event handler
static void btn_save_fluidnc_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Settings saved!");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
        Serial.println("FluidNC Save button clicked");
    }
}

// Reset to defaults button event handler
static void btn_defaults_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("FluidNC Reset button clicked");
    }
}
