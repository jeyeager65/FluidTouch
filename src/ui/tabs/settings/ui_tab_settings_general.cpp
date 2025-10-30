#include "ui/tabs/settings/ui_tab_settings_general.h"
#include "ui/ui_theme.h"
#include "config.h"
#include <Preferences.h>

// Global references for UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *show_machine_select_switch = NULL;

// Forward declarations for event handlers
static void btn_save_general_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);

void UITabSettingsGeneral::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Load current preference
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    prefs.end();
    
    Serial.printf("UITabSettingsGeneral: Loaded show_mach_sel=%d\n", show_machine_select);
    
    // === Machine Selection Section ===
    lv_obj_t *section_title = lv_label_create(tab);
    lv_label_set_text(section_title, "MACHINE SELECTION");
    lv_obj_set_style_text_font(section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(section_title, 20, 20);
    
    // Show label and switch on same line
    lv_obj_t *machine_sel_label = lv_label_create(tab);
    lv_label_set_text(machine_sel_label, "Show:");
    lv_obj_set_style_text_font(machine_sel_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(machine_sel_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(machine_sel_label, 20, 72);  // 20 + 40 (title spacing) + 12 (vertical alignment)
    
    show_machine_select_switch = lv_switch_create(tab);
    lv_obj_set_pos(show_machine_select_switch, 100, 67);  // 20 + 40 (title spacing) + 7 (switch alignment)
    if (show_machine_select) {
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
    }
    
    // Description text
    lv_obj_t *desc_label = lv_label_create(tab);
    lv_label_set_text(desc_label, "When disabled, the first configured machine\nwill be loaded automatically at startup.");
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(desc_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(desc_label, 20, 107);  // 20 + 40 (title) + 40 (switch row) + 7 (spacing)
    
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
    lv_obj_add_event_cb(btn_save, btn_save_general_event_handler, LV_EVENT_CLICKED, NULL);
    
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
    
    // Status label (positioned above buttons)
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 20, 335);
}

// Save button event handler
static void btn_save_general_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Save machine selection preference
        bool show_machine_select = lv_obj_has_state(show_machine_select_switch, LV_STATE_CHECKED);
        
        Serial.printf("UITabSettingsGeneral: Saving show_mach_sel=%d\n", show_machine_select);
        
        Preferences prefs;
        if (!prefs.begin(PREFS_NAMESPACE, false)) {  // Read-write
            Serial.println("UITabSettingsGeneral: ERROR - Failed to open Preferences for writing!");
            if (status_label != NULL) {
                lv_label_set_text(status_label, "Error: Failed to save!");
                lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
            }
            return;
        }
        
        prefs.putBool("show_mach_sel", show_machine_select);
        prefs.end();
        
        // Verify it was saved
        prefs.begin(PREFS_NAMESPACE, true);
        bool verified = prefs.getBool("show_mach_sel", true);
        prefs.end();
        
        Serial.printf("UITabSettingsGeneral: Verified show_mach_sel=%d\n", verified);
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Settings saved! Restart to apply.");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to default (show machine selection enabled)
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("General Reset button clicked");
    }
}
