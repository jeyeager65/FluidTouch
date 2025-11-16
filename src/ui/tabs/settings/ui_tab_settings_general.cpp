#include "ui/tabs/settings/ui_tab_settings_general.h"
#include "ui/ui_theme.h"
#include "ui/settings_manager.h"
#include "config.h"
#include <Preferences.h>

// Global references for UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *show_machine_select_switch = NULL;
static lv_obj_t *folders_on_top_switch = NULL;

// Forward declarations for event handlers
static void btn_save_general_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);
static void btn_export_event_handler(lv_event_t *e);
static void btn_clear_event_handler(lv_event_t *e);

void UITabSettingsGeneral::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Load current preferences
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    bool folders_on_top = prefs.getBool("folders_on_top", false);  // Default to false (folders at bottom)
    prefs.end();
    
    Serial.printf("UITabSettingsGeneral: Loaded show_mach_sel=%d, folders_on_top=%d\n", show_machine_select, folders_on_top);
    
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
    
    // === Backup & Restore Section (Second column) ===
    lv_obj_t *backup_section_title = lv_label_create(tab);
    lv_label_set_text(backup_section_title, "BACKUP & RESTORE");
    lv_obj_set_style_text_font(backup_section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(backup_section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(backup_section_title, 400, 20);  // Second column
    
    // Export settings button
    lv_obj_t *btn_export = lv_button_create(tab);
    lv_obj_set_size(btn_export, 180, 50);
    lv_obj_set_pos(btn_export, 400, 60);  // Second column
    lv_obj_set_style_bg_color(btn_export, UITheme::ACCENT_SECONDARY, LV_PART_MAIN);
    lv_obj_t *lbl_export = lv_label_create(btn_export);
    lv_label_set_text(lbl_export, LV_SYMBOL_DOWNLOAD " Export");
    lv_obj_set_style_text_font(lbl_export, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_export);
    lv_obj_add_event_cb(btn_export, btn_export_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Clear settings button
    lv_obj_t *btn_clear = lv_button_create(tab);
    lv_obj_set_size(btn_clear, 180, 50);
    lv_obj_set_pos(btn_clear, 400, 120);  // Second column, below Export
    lv_obj_set_style_bg_color(btn_clear, UITheme::STATE_ALARM, LV_PART_MAIN);
    lv_obj_t *lbl_clear = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clear, LV_SYMBOL_TRASH " Clear All");
    lv_obj_set_style_text_font(lbl_clear, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_clear);
    lv_obj_add_event_cb(btn_clear, btn_clear_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Description for backup section
    lv_obj_t *backup_desc_label = lv_label_create(tab);
    lv_label_set_text(backup_desc_label, "Export saves backup to Display SD.\nClear erases all settings and restarts.");
    lv_obj_set_style_text_font(backup_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(backup_desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(backup_desc_label, 400, 180);  // Second column, below buttons
    
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
        
        Serial.printf("UITabSettingsGeneral: Saving show_mach_sel=%d, folders_on_top=%d\n", show_machine_select, folders_on_top);
        
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
        prefs.end();
        
        // Verify it was saved
        prefs.begin(PREFS_SYSTEM_NAMESPACE, true);
        bool verified_machine = prefs.getBool("show_mach_sel", true);
        bool verified_folders = prefs.getBool("folders_on_top", false);
        prefs.end();
        
        Serial.printf("UITabSettingsGeneral: Verified show_mach_sel=%d, folders_on_top=%d\n", verified_machine, verified_folders);
        
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
        // Reset to defaults (show machine selection enabled, folders at bottom)
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
        lv_obj_clear_state(folders_on_top_switch, LV_STATE_CHECKED);  // Default: folders at bottom
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("General Reset button clicked");
    }
}

// Export settings button event handler
static void btn_export_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("[SettingsGeneral] Export button clicked");
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Exporting...");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        
        // Force LVGL update to show status
        lv_task_handler();
        
        // Attempt export
        if (SettingsManager::exportSettings()) {
            Serial.println("[SettingsGeneral] Export successful");
            
            // Create modal backdrop
            lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
            lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
            lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
            lv_obj_set_style_border_width(backdrop, 0, 0);
            lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_center(backdrop);
            
            // Show success dialog with important information
            lv_obj_t *dialog = lv_obj_create(backdrop);
            lv_obj_set_size(dialog, 650, 300);
            lv_obj_center(dialog);
            lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
            lv_obj_set_style_border_width(dialog, 3, 0);
            lv_obj_set_style_border_color(dialog, UITheme::UI_SUCCESS, 0);
            lv_obj_set_style_pad_all(dialog, 20, 0);
            lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
            
            // Title
            lv_obj_t *title = lv_label_create(dialog);
            lv_label_set_text(title, LV_SYMBOL_OK " Export Successful");
            lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
            lv_obj_set_style_text_color(title, UITheme::UI_SUCCESS, 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
            
            // Message
            lv_obj_t *message = lv_label_create(dialog);
            lv_label_set_text(message, 
                "Settings exported to fluidtouch_settings.json on Display SD.\n\n"
                "IMPORTANT: WiFi passwords are NOT included\n"
                "for security reasons.\n\n"
                "When importing this file, you will need to\n"
                "manually set WiFi passwords for each machine.");
            lv_obj_set_style_text_font(message, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(message, UITheme::TEXT_LIGHT, 0);
            lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(message, 610);
            lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 50);
            
            // Button container
            lv_obj_t *btn_container = lv_obj_create(dialog);
            lv_obj_set_size(btn_container, 610, 60);
            lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(btn_container, 0, 0);
            lv_obj_set_style_pad_all(btn_container, 0, 0);
            lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
            
            // OK button
            lv_obj_t *btn_ok = lv_button_create(btn_container);
            lv_obj_set_size(btn_ok, 160, 50);
            lv_obj_center(btn_ok);
            lv_obj_set_style_bg_color(btn_ok, UITheme::BTN_PLAY, 0);
            lv_obj_t *lbl_ok = lv_label_create(btn_ok);
            lv_label_set_text(lbl_ok, "OK");
            lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_ok);
            lv_obj_add_event_cb(btn_ok, [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                    lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
                }
            }, LV_EVENT_CLICKED, backdrop);
            
            if (status_label != NULL) {
                lv_label_set_text(status_label, "");
            }
        } else {
            Serial.println("[SettingsGeneral] Export failed");
            if (status_label != NULL) {
                lv_label_set_text(status_label, "Export failed! Check SD card.");
                lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
            }
        }
    }
}

// Clear all settings button event handler
static void btn_clear_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("[SettingsGeneral] Clear All button clicked");
        
        // Create modal backdrop
        lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
        lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
        lv_obj_set_style_border_width(backdrop, 0, 0);
        lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(backdrop);
        
        // Show confirmation dialog
        lv_obj_t *dialog = lv_obj_create(backdrop);
        lv_obj_set_size(dialog, 600, 350);
        lv_obj_center(dialog);
        lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
        lv_obj_set_style_border_width(dialog, 3, 0);
        lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
        lv_obj_set_style_pad_all(dialog, 20, 0);
        lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
        
        // Warning title
        lv_obj_t *title = lv_label_create(dialog);
        lv_label_set_text(title, LV_SYMBOL_WARNING " WARNING");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
        
        // Warning intro text (centered)
        lv_obj_t *intro = lv_label_create(dialog);
        lv_label_set_text(intro, "This will PERMANENTLY DELETE all settings:");
        lv_obj_set_style_text_font(intro, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(intro, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(intro, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(intro, 560);
        lv_obj_align(intro, LV_ALIGN_TOP_MID, 0, 40);
        
        // Bullet list container (centered, with left-aligned content)
        lv_obj_t *list_container = lv_obj_create(dialog);
        lv_obj_set_size(list_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(list_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list_container, 0, 0);
        lv_obj_set_style_pad_all(list_container, 5, 0);
        lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 60);
        lv_obj_clear_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t *bullets = lv_label_create(list_container);
        lv_label_set_text(bullets, 
            "• All machine configurations\n"
            "• All macros\n"
            "• Power management settings\n"
            "• UI preferences");
        lv_obj_set_style_text_font(bullets, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(bullets, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(bullets, LV_TEXT_ALIGN_LEFT, 0);
        
        // Warning footer text (centered)
        lv_obj_t *footer = lv_label_create(dialog);
        lv_label_set_text(footer, 
            "Controller will restart immediately. If exported\nsettings are on Display SD, they will be automatically imported.\n\n"
            "Consider exporting settings first!");
        lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(footer, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(footer, 560);
        lv_obj_align(footer, LV_ALIGN_TOP_MID, 0, 155);
        
        // Button container
        lv_obj_t *btn_container = lv_obj_create(dialog);
        lv_obj_set_size(btn_container, 560, 60);
        lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_style_pad_all(btn_container, 0, 0);
        lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
        
        // Confirm button (left)
        lv_obj_t *btn_confirm = lv_button_create(btn_container);
        lv_obj_set_size(btn_confirm, 200, 50);
        lv_obj_set_style_bg_color(btn_confirm, UITheme::STATE_ALARM, 0);
        lv_obj_t *lbl_confirm = lv_label_create(btn_confirm);
        lv_label_set_text(lbl_confirm, LV_SYMBOL_TRASH " Clear & Restart");
        lv_obj_set_style_text_font(lbl_confirm, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl_confirm);
        lv_obj_add_event_cb(btn_confirm, [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                Serial.println("[SettingsGeneral] User confirmed clear all settings");
                
                // Clear all settings
                SettingsManager::clearAllSettings();
                
                // Show restart message
                lv_obj_t *backdrop = (lv_obj_t*)lv_event_get_user_data(e);
                lv_obj_clean(backdrop);
                
                lv_obj_t *restart_label = lv_label_create(backdrop);
                lv_label_set_text(restart_label, "Settings cleared!\n\nRestarting...");
                lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_24, 0);
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
        
        // Cancel button (right)
        lv_obj_t *btn_cancel = lv_button_create(btn_container);
        lv_obj_set_size(btn_cancel, 180, 50);
        lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_BUTTON, 0);
        lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
        lv_label_set_text(lbl_cancel, "Cancel");
        lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
        lv_obj_center(lbl_cancel);
        lv_obj_add_event_cb(btn_cancel, [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
            }
        }, LV_EVENT_CLICKED, backdrop);
    }
}
