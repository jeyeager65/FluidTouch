#include "ui/tabs/settings/ui_tab_settings_general.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/machine_config.h"
#include "ui/settings_manager.h"
#include "core/display_driver.h"
#include "config.h"
#include <Preferences.h>

// Global references for UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *show_machine_select_switch = NULL;
static lv_obj_t *folders_on_top_switch = NULL;
static lv_obj_t *rotate_display_switch = NULL;
static lv_obj_t *enable_a_axis_switch = NULL;

// Forward declarations for event handlers
static void btn_save_general_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);
static void showRotationRestartDialog();
static void showAAxisRestartDialog();

void UITabSettingsGeneral::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    bool folders_on_top = prefs.getBool("folders_on_top", false);  // Default to false (folders at bottom)
    uint8_t display_rotation = prefs.getUChar("display_rot", 0);  // Default to 0 (normal)
    prefs.end();

    // Load enable_a_axis from the selected machine config (it's machine-specific)
    bool enable_a_axis = false;
    MachineConfig mc;
    if (MachineConfigManager::getSelectedMachine(mc)) {
        enable_a_axis = mc.enable_a_axis;
    }

    Serial.printf("UITabSettingsGeneral: Loaded show_mach_sel=%d, folders_on_top=%d, display_rot=%d, enable_a_axis=%d\n", show_machine_select, folders_on_top, display_rotation, enable_a_axis);
    
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
    lv_obj_set_pos(machine_sel_label, 20, 70);  // 20 + 40 (title spacing) + 12 (vertical alignment)
    
    show_machine_select_switch = lv_switch_create(tab);
    lv_obj_set_pos(show_machine_select_switch, 140, 65);  // 20 + 40 (title spacing) + 7 (switch alignment)
    if (show_machine_select) {
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
    }
    
    // Description text
    lv_obj_t *desc_label = lv_label_create(tab);
    lv_label_set_text(desc_label, "When disabled, the first configured machine\nwill be loaded automatically at startup.");
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(desc_label, 20, 107);  // 20 + 40 (title) + 40 (switch row) + 7 (spacing)
    
    // === Display Section (Top right) ===
    lv_obj_t *display_section_title = lv_label_create(tab);
    lv_label_set_text(display_section_title, "DISPLAY");
    lv_obj_set_style_text_font(display_section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(display_section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(display_section_title, 400, 20);  // Top right column
    
    // Rotate display label and switch
    lv_obj_t *rotate_label = lv_label_create(tab);
    lv_label_set_text(rotate_label, "Rotate 180°:");
    lv_obj_set_style_text_font(rotate_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(rotate_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(rotate_label, 400, 70);  // Top right, aligned with Machine Selection Show label
    
    rotate_display_switch = lv_switch_create(tab);
    lv_obj_set_pos(rotate_display_switch, 560, 65);  // Aligned with label
    if (display_rotation == 2) {  // Rotation 2 = 180 degrees
        lv_obj_add_state(rotate_display_switch, LV_STATE_CHECKED);
    }
    
    // Description text for rotation setting
    lv_obj_t *rotate_desc_label = lv_label_create(tab);
    lv_label_set_text(rotate_desc_label, "Rotate display 180° for\nupside-down mounting.\nRequires restart.");
    lv_obj_set_style_text_font(rotate_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rotate_desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(rotate_desc_label, 400, 107);  // Top right, aligned with Machine Selection description
    
    // === Files Section (First column, below Machine Selection) ===
    lv_obj_t *files_section_title = lv_label_create(tab);
    lv_label_set_text(files_section_title, "FILES");
    lv_obj_set_style_text_font(files_section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(files_section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(files_section_title, 20, 155);  // First column, below Machine Selection
    
    // Folders on top label and switch
    lv_obj_t *folders_label = lv_label_create(tab);
    lv_label_set_text(folders_label, "Folders on Top:");
    lv_obj_set_style_text_font(folders_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(folders_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(folders_label, 20, 205);  // First column
    
    folders_on_top_switch = lv_switch_create(tab);
    lv_obj_set_pos(folders_on_top_switch, 200, 200);  // Aligned with label
    if (folders_on_top) {
        lv_obj_add_state(folders_on_top_switch, LV_STATE_CHECKED);
    }
    
    // Description text for folders setting
    lv_obj_t *folders_desc_label = lv_label_create(tab);
    lv_label_set_text(folders_desc_label, "When enabled, folders appear at the top\nof the file list instead of the bottom.");
    lv_obj_set_style_text_font(folders_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(folders_desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(folders_desc_label, 20, 242);  // First column

    // Enable A-Axis label and switch (Right column, below Display)
    lv_obj_t *enable_a_axis_label = lv_label_create(tab);
    lv_label_set_text(enable_a_axis_label, "Enable A-Axis:");
    lv_obj_set_style_text_font(enable_a_axis_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(enable_a_axis_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(enable_a_axis_label, 400, 205);  // Right column

    enable_a_axis_switch = lv_switch_create(tab);
    lv_obj_set_pos(enable_a_axis_switch, 560, 200);  // Aligned with label
    if (enable_a_axis) {
        lv_obj_add_state(enable_a_axis_switch, LV_STATE_CHECKED);
    }

    // Description text for A-axis setting
    lv_obj_t *a_axis_desc_label = lv_label_create(tab);
    lv_label_set_text(a_axis_desc_label, "Enables 4th axis (rotary) support.\nAdds A-axis controls and display.");
    lv_obj_set_style_text_font(a_axis_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(a_axis_desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(a_axis_desc_label, 400, 242);  // Right column

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
        // Save preferences
        bool show_machine_select = lv_obj_has_state(show_machine_select_switch, LV_STATE_CHECKED);
        bool folders_on_top = lv_obj_has_state(folders_on_top_switch, LV_STATE_CHECKED);
        bool rotate_display = lv_obj_has_state(rotate_display_switch, LV_STATE_CHECKED);
        bool enable_a_axis = lv_obj_has_state(enable_a_axis_switch, LV_STATE_CHECKED);
        uint8_t rotation = rotate_display ? 2 : 0;  // 2 = 180 degrees, 0 = normal

        Serial.printf("UITabSettingsGeneral: Saving show_mach_sel=%d, folders_on_top=%d, display_rot=%d, enable_a_axis=%d\n", show_machine_select, folders_on_top, rotation, enable_a_axis);
        
        Preferences prefs;
        if (!prefs.begin(PREFS_SYSTEM_NAMESPACE, false)) {  // Read-write
            Serial.println("UITabSettingsGeneral: ERROR - Failed to open Preferences for writing!");
            if (status_label != NULL) {
                lv_label_set_text(status_label, "Error: Failed to save!");
                lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
            }
            return;
        }
        
        prefs.putBool("show_mach_sel", show_machine_select);
        prefs.putBool("folders_on_top", folders_on_top);

        // Check if rotation changed - requires restart
        uint8_t old_rotation = prefs.getUChar("display_rot", 0);
        bool rotation_changed = (rotation != old_rotation);
        
        prefs.putUChar("display_rot", rotation);
        prefs.end();

        // Save enable_a_axis to the selected machine config
        bool a_axis_changed = false;
        {
            MachineConfig mc;
            int sel_idx = MachineConfigManager::getSelectedMachineIndex();
            if (sel_idx >= 0 && MachineConfigManager::getMachine(sel_idx, mc)) {
                a_axis_changed = (enable_a_axis != mc.enable_a_axis);
                mc.enable_a_axis = enable_a_axis;
                MachineConfigManager::saveMachine(sel_idx, mc);
            }
        }

        // Update cached A-axis setting immediately (no restart needed)
        UICommon::setAAxisEnabled(enable_a_axis);

        // Verify system prefs were saved
        prefs.begin(PREFS_SYSTEM_NAMESPACE, true);
        bool verified_machine = prefs.getBool("show_mach_sel", true);
        bool verified_folders = prefs.getBool("folders_on_top", false);
        uint8_t verified_rotation = prefs.getUChar("display_rot", 0);
        prefs.end();

        Serial.printf("UITabSettingsGeneral: Verified show_mach_sel=%d, folders_on_top=%d, display_rot=%d, enable_a_axis=%d\n", verified_machine, verified_folders, verified_rotation, enable_a_axis);

        if (status_label != NULL) {
            lv_label_set_text(status_label, "Settings saved!");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }

        // If rotation changed, show restart confirmation dialog
        if (rotation_changed) {
            showRotationRestartDialog();
        }
        // If A-axis setting changed, show restart confirmation dialog
        else if (a_axis_changed) {
            showAAxisRestartDialog();
        }
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to defaults (show machine selection enabled, folders at bottom, rotation 0, A-axis disabled)
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
        lv_obj_clear_state(folders_on_top_switch, LV_STATE_CHECKED);  // Default: folders at bottom
        lv_obj_clear_state(rotate_display_switch, LV_STATE_CHECKED);  // Default: rotation 0 (normal)
        lv_obj_clear_state(enable_a_axis_switch, LV_STATE_CHECKED);  // Default: A-axis disabled
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("General Reset button clicked");
    }
}

// Show rotation restart confirmation dialog
static void showRotationRestartDialog() {
    Serial.println("[SettingsGeneral] Showing rotation restart dialog");
    
    // Create modal backdrop
    lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(backdrop);
    
    // Create dialog
    lv_obj_t *dialog = lv_obj_create(backdrop);
    lv_obj_set_size(dialog, 600, 250);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_border_color(dialog, UITheme::UI_WARNING, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Restart Required");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::UI_WARNING, 0);
    lv_obj_set_pos(title, 0, 0);
    
    // Message
    lv_obj_t *message = lv_label_create(dialog);
    lv_label_set_text(message, "Display rotation requires a restart\nto take effect.\n\nRestart now?");
    lv_obj_set_style_text_font(message, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(message, 0, 45);
    lv_obj_set_width(message, 560);
    
    // Button container for horizontal layout
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_pos(btn_container, 0, 150);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Restart button (left)
    lv_obj_t *btn_restart = lv_button_create(btn_container);
    lv_obj_set_size(btn_restart, 240, 50);
    lv_obj_set_style_bg_color(btn_restart, UITheme::UI_WARNING, 0);
    lv_obj_t *lbl_restart = lv_label_create(btn_restart);
    lv_label_set_text(lbl_restart, LV_SYMBOL_REFRESH " Restart");
    lv_obj_set_style_text_font(lbl_restart, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_restart);
    lv_obj_add_event_cb(btn_restart, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            Serial.println("[SettingsGeneral] Restart button clicked - restarting ESP32");
            
            // Show restart message
            lv_obj_t *backdrop = (lv_obj_t*)lv_event_get_user_data(e);
            lv_obj_clean(backdrop);
            
            lv_obj_t *restart_label = lv_label_create(backdrop);
            lv_label_set_text(restart_label, "Restarting...");
            lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_color(restart_label, UITheme::UI_INFO, 0);
            lv_obj_set_style_text_align(restart_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(restart_label);
            
            // Force UI update
            lv_task_handler();
            delay(2000);
            
            // Restart ESP32
            ESP.restart();
        }
    }, LV_EVENT_CLICKED, backdrop);
    
    // Later button (right)
    lv_obj_t *btn_later = lv_button_create(btn_container);
    lv_obj_set_size(btn_later, 240, 50);
    lv_obj_set_style_bg_color(btn_later, UITheme::BG_BUTTON, 0);
    lv_obj_t *lbl_later = lv_label_create(btn_later);
    lv_label_set_text(lbl_later, "Later");
    lv_obj_set_style_text_font(lbl_later, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_later);
    lv_obj_add_event_cb(btn_later, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            Serial.println("[SettingsGeneral] Later button clicked - rotation will apply on next restart");
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
        }
    }, LV_EVENT_CLICKED, backdrop);
}

// Show A-axis restart confirmation dialog
static void showAAxisRestartDialog() {
    Serial.println("[SettingsGeneral] Showing A-axis restart dialog");

    // Create modal backdrop
    lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(backdrop);

    // Create dialog
    lv_obj_t *dialog = lv_obj_create(backdrop);
    lv_obj_set_size(dialog, 600, 250);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_border_color(dialog, UITheme::UI_WARNING, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Restart Required");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::UI_WARNING, 0);
    lv_obj_set_pos(title, 0, 0);

    // Message
    lv_obj_t *message = lv_label_create(dialog);
    lv_label_set_text(message, "A-axis setting requires a restart\nto take effect.\n\nRestart now?");
    lv_obj_set_style_text_font(message, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(message, 0, 45);
    lv_obj_set_width(message, 560);

    // Button container for horizontal layout
    lv_obj_t *btn_container = lv_obj_create(dialog);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_pos(btn_container, 0, 150);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Restart button (left)
    lv_obj_t *btn_restart = lv_button_create(btn_container);
    lv_obj_set_size(btn_restart, 240, 50);
    lv_obj_set_style_bg_color(btn_restart, UITheme::UI_WARNING, 0);
    lv_obj_t *lbl_restart = lv_label_create(btn_restart);
    lv_label_set_text(lbl_restart, LV_SYMBOL_REFRESH " Restart");
    lv_obj_set_style_text_font(lbl_restart, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_restart);
    lv_obj_add_event_cb(btn_restart, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            Serial.println("[SettingsGeneral] Restart button clicked - restarting ESP32");

            // Show restart message
            lv_obj_t *backdrop = (lv_obj_t*)lv_event_get_user_data(e);
            lv_obj_clean(backdrop);

            lv_obj_t *restart_label = lv_label_create(backdrop);
            lv_label_set_text(restart_label, "Restarting...");
            lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_color(restart_label, UITheme::UI_INFO, 0);
            lv_obj_set_style_text_align(restart_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_center(restart_label);

            // Force UI update
            lv_task_handler();
            delay(2000);

            // Restart ESP32
            ESP.restart();
        }
    }, LV_EVENT_CLICKED, backdrop);

    // Later button (right)
    lv_obj_t *btn_later = lv_button_create(btn_container);
    lv_obj_set_size(btn_later, 240, 50);
    lv_obj_set_style_bg_color(btn_later, UITheme::BG_BUTTON, 0);
    lv_obj_t *lbl_later = lv_label_create(btn_later);
    lv_label_set_text(lbl_later, "Later");
    lv_obj_set_style_text_font(lbl_later, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_later);
    lv_obj_add_event_cb(btn_later, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            Serial.println("[SettingsGeneral] Later button clicked - A-axis will apply on next restart");
            lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
        }
    }, LV_EVENT_CLICKED, backdrop);
}
