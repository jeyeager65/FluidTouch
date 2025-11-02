#include "ui/tabs/ui_tab_macros.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "config.h"
#include "network/fluidnc_client.h"
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

// Static member initialization
lv_obj_t *UITabMacros::parent_tab = nullptr;
lv_obj_t *UITabMacros::macro_container = nullptr;
lv_obj_t *UITabMacros::btn_edit = nullptr;
lv_obj_t *UITabMacros::btn_add = nullptr;
lv_obj_t *UITabMacros::btn_done = nullptr;
lv_obj_t *UITabMacros::lbl_empty_message = nullptr;
lv_obj_t *UITabMacros::progress_container = nullptr;
lv_obj_t *UITabMacros::lbl_macro_name = nullptr;
lv_obj_t *UITabMacros::bar_progress = nullptr;
lv_obj_t *UITabMacros::lbl_percent = nullptr;
lv_obj_t *UITabMacros::lbl_message = nullptr;
char UITabMacros::running_macro_name[32] = "";
bool UITabMacros::is_edit_mode = false;
MacroConfig UITabMacros::macros[MAX_MACROS];
lv_obj_t *UITabMacros::macro_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::edit_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::up_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::down_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::delete_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::config_dialog = nullptr;
lv_obj_t *UITabMacros::config_name_textarea = nullptr;
lv_obj_t *UITabMacros::config_path_dropdown = nullptr;
lv_obj_t *UITabMacros::config_color_buttons[8] = {nullptr};
int UITabMacros::selected_color_index = 0;
lv_obj_t *UITabMacros::keyboard = nullptr;
int UITabMacros::editing_index = -1;
lv_obj_t *UITabMacros::delete_dialog = nullptr;
std::vector<std::string> UITabMacros::macro_files;

void UITabMacros::create(lv_obj_t *tab) {
    parent_tab = tab;
    is_edit_mode = false;
    
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Remove default padding on tab
    lv_obj_set_style_pad_all(tab, 0, 0);

    // Load macros from preferences
    loadMacros();

    // PROGRESS DISPLAY - Top area (hidden by default, shown during macro execution)
    progress_container = lv_obj_create(tab);
    lv_obj_set_size(progress_container, 630, 65);  // Increased from 60 to 65
    lv_obj_set_pos(progress_container, 15, 5);
    lv_obj_set_style_bg_color(progress_container, UITheme::BG_DARKER, 0);
    lv_obj_set_style_border_width(progress_container, 1, 0);
    lv_obj_set_style_border_color(progress_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(progress_container, 5, 0);
    lv_obj_clear_flag(progress_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Macro name label
    lbl_macro_name = lv_label_create(progress_container);
    lv_label_set_text(lbl_macro_name, "Running: Macro Name");
    lv_obj_set_style_text_font(lbl_macro_name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_macro_name, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_macro_name, 0, 0);
    lv_label_set_long_mode(lbl_macro_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_macro_name, 400);
    
    // Progress bar
    bar_progress = lv_bar_create(progress_container);
    lv_obj_set_size(bar_progress, 500, 15);
    lv_obj_set_pos(bar_progress, 0, 20);
    lv_obj_set_style_bg_color(bar_progress, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_progress, UITheme::UI_SUCCESS, LV_PART_INDICATOR);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    
    // Percentage label
    lbl_percent = lv_label_create(progress_container);
    lv_label_set_text(lbl_percent, "0%");
    lv_obj_set_style_text_font(lbl_percent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_percent, UITheme::UI_SUCCESS, 0);
    lv_obj_set_pos(lbl_percent, 510, 18);
    
    // Message label
    lbl_message = lv_label_create(progress_container);
    lv_label_set_text(lbl_message, "");
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_14, 0);  // Increased from 12 to 14
    lv_obj_set_style_text_color(lbl_message, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_message, 0, 40);
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_message, 620);

    // Edit button (upper right corner, absolute positioning on tab)
    btn_edit = lv_btn_create(tab);
    lv_obj_set_size(btn_edit, 120, 45);
    lv_obj_set_style_bg_color(btn_edit, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(btn_edit, 665, 15);
    lv_obj_add_event_cb(btn_edit, onEditModeToggle, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *edit_label = lv_label_create(btn_edit);
    lv_label_set_text(edit_label, LV_SYMBOL_EDIT " Edit");
    lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_16, 0);
    lv_obj_center(edit_label);

    // Add button (initially hidden)
    btn_add = lv_btn_create(tab);
    lv_obj_set_size(btn_add, 120, 45);
    lv_obj_set_style_bg_color(btn_add, UITheme::BTN_PLAY, 0);
    lv_obj_set_pos(btn_add, 545, 15);  // Left of Done button
    lv_obj_add_event_cb(btn_add, onAddMacro, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *add_label = lv_label_create(btn_add);
    lv_label_set_text(add_label, LV_SYMBOL_PLUS " Add");
    lv_obj_set_style_text_font(add_label, &lv_font_montserrat_16, 0);
    lv_obj_center(add_label);

    // Done button (initially hidden)
    btn_done = lv_btn_create(tab);
    lv_obj_set_size(btn_done, 120, 45);
    lv_obj_set_style_bg_color(btn_done, UITheme::BTN_PLAY, 0);
    lv_obj_set_pos(btn_done, 670, 15);  // Same position as Edit button
    lv_obj_add_event_cb(btn_done, onEditModeToggle, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *done_label = lv_label_create(btn_done);
    lv_label_set_text(done_label, LV_SYMBOL_OK " Done");
    lv_obj_set_style_text_font(done_label, &lv_font_montserrat_16, 0);
    lv_obj_center(done_label);

    // Macro container for flex layout
    macro_container = lv_obj_create(tab);
    lv_obj_set_size(macro_container, 770, 270);
    lv_obj_set_style_bg_color(macro_container, UITheme::BG_DARKER, LV_PART_MAIN);
    lv_obj_set_style_border_color(macro_container, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(macro_container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(macro_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(macro_container, 0, 0);
    lv_obj_set_pos(macro_container, 15, 75);  // Position below Edit button
    lv_obj_clear_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);

    // Empty message label (shown when no macros configured)
    lbl_empty_message = lv_label_create(macro_container);
    lv_label_set_text(lbl_empty_message, "No macros configured.\n\nClick " LV_SYMBOL_EDIT " Edit to add macros.");
    lv_obj_set_style_text_font(lbl_empty_message, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_empty_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(lbl_empty_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_empty_message, 700);  // Constrain width for better text layout
    lv_obj_center(lbl_empty_message);  // Center both horizontally and vertically
    lv_obj_add_flag(lbl_empty_message, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    loadMacros();
    refreshMacroList();
}

// Load macros from Preferences
void UITabMacros::loadMacros() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    
    int machine_index = prefs.getInt("sel_machine", 0);
    
    char key[32];
    snprintf(key, sizeof(key), "m%d_macros", machine_index);
    
    size_t size = prefs.getBytesLength(key);
    if (size == sizeof(macros)) {
        prefs.getBytes(key, macros, size);
        Serial.printf("Loaded %d macros for machine %d\n", getConfiguredMacroCount(), machine_index);
    } else {
        // Initialize with default empty macros
        for (int i = 0; i < MAX_MACROS; i++) {
            macros[i].is_configured = false;
            memset(macros[i].name, 0, sizeof(macros[i].name));
            memset(macros[i].file_path, 0, sizeof(macros[i].file_path));
            macros[i].color_index = i % 8;  // Cycle through colors
        }
        Serial.printf("Initialized empty macros for machine %d\n", machine_index);
    }
    
    prefs.end();
}

// Save macros to Preferences
void UITabMacros::saveMacros() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);  // Read-write
    
    int machine_index = prefs.getInt("sel_machine", 0);
    
    char key[32];
    snprintf(key, sizeof(key), "m%d_macros", machine_index);
    
    prefs.putBytes(key, macros, sizeof(macros));
    Serial.printf("Saved %d macros for machine %d\n", getConfiguredMacroCount(), machine_index);
    
    prefs.end();
}

// Get count of configured macros
int UITabMacros::getConfiguredMacroCount() {
    int count = 0;
    for (int i = 0; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            count++;
        }
    }
    return count;
}

// Get color by index (0-7)
lv_color_t UITabMacros::getColorByIndex(int index) {
    switch (index) {
        case 0: return UITheme::MACRO_COLOR_1;
        case 1: return UITheme::MACRO_COLOR_2;
        case 2: return UITheme::MACRO_COLOR_3;
        case 3: return UITheme::MACRO_COLOR_4;
        case 4: return UITheme::MACRO_COLOR_5;
        case 5: return UITheme::MACRO_COLOR_6;
        case 6: return UITheme::MACRO_COLOR_7;
        case 7: return UITheme::MACRO_COLOR_8;
        default: return UITheme::BTN_CONNECT;
    }
}

// Swap two macros in the array
void UITabMacros::swapMacros(int index1, int index2) {
    if (index1 < 0 || index1 >= MAX_MACROS || index2 < 0 || index2 >= MAX_MACROS) {
        return;
    }
    
    MacroConfig temp = macros[index1];
    macros[index1] = macros[index2];
    macros[index2] = temp;
    
    saveMacros();
    refreshMacroList();
}

// Refresh the macro list display
void UITabMacros::refreshMacroList() {
    // Clear all children from container (prevents duplication)
    lv_obj_clean(macro_container);
    
    // Reset button pointers
    for (int i = 0; i < MAX_MACROS; i++) {
        macro_buttons[i] = nullptr;
        up_buttons[i] = nullptr;
        down_buttons[i] = nullptr;
        edit_buttons[i] = nullptr;
        delete_buttons[i] = nullptr;
    }
    
    if (is_edit_mode) {
        // EDIT MODE: Absolute positioning with control buttons
        lv_obj_set_layout(macro_container, LV_LAYOUT_NONE);
        lv_obj_set_style_pad_all(macro_container, 5, 0);
        lv_obj_add_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);  // Enable scrolling in edit mode
        lv_obj_set_scroll_dir(macro_container, LV_DIR_VER);  // Vertical scrolling only
        
        int displayed_index = 0;
        for (int i = 0; i < MAX_MACROS; i++) {
            if (!macros[i].is_configured) continue;
            
            int y_pos = displayed_index * 65; // 60px button + 5px gap
            
            // Macro button (shows name + color) - increased by 10px to 488px
            // Note: No click event in edit mode - button is just for display
            macro_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(macro_buttons[i], 488, 60);  // Increased from 478 to 488
            lv_obj_set_pos(macro_buttons[i], 0, y_pos);
            lv_obj_set_style_bg_color(macro_buttons[i], getColorByIndex(macros[i].color_index), 0);
            lv_obj_add_flag(macro_buttons[i], LV_OBJ_FLAG_CLICKABLE);  // Make non-clickable
            
            lv_obj_t *btn_label = lv_label_create(macro_buttons[i]);
            lv_label_set_text(btn_label, macros[i].name);
            lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_22, 0);
            lv_obj_align(btn_label, LV_ALIGN_LEFT_MID, 10, 0);
            
            // Up button
            up_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(up_buttons[i], 60, 60);
            lv_obj_set_pos(up_buttons[i], 493, y_pos);  // Shifted right by 10px (was 483)
            lv_obj_set_style_bg_color(up_buttons[i], UITheme::BG_BUTTON, 0);
            lv_obj_add_event_cb(up_buttons[i], onMoveUpMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            if (displayed_index == 0) {
                lv_obj_add_state(up_buttons[i], LV_STATE_DISABLED);
            }
            
            lv_obj_t *up_label = lv_label_create(up_buttons[i]);
            lv_label_set_text(up_label, LV_SYMBOL_UP);
            lv_obj_set_style_text_font(up_label, &lv_font_montserrat_22, 0);
            lv_obj_center(up_label);
            
            // Down button
            down_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(down_buttons[i], 60, 60);
            lv_obj_set_pos(down_buttons[i], 558, y_pos);  // Shifted right by 10px (was 548)
            lv_obj_set_style_bg_color(down_buttons[i], UITheme::BG_BUTTON, 0);
            lv_obj_add_event_cb(down_buttons[i], onMoveDownMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            // Count total configured to disable last item's down button
            int configured_count = 0;
            for (int j = 0; j < MAX_MACROS; j++) {
                if (macros[j].is_configured) configured_count++;
            }
            if (displayed_index == configured_count - 1) {
                lv_obj_add_state(down_buttons[i], LV_STATE_DISABLED);
            }
            
            lv_obj_t *down_label = lv_label_create(down_buttons[i]);
            lv_label_set_text(down_label, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_font(down_label, &lv_font_montserrat_22, 0);
            lv_obj_center(down_label);
            
            // Edit button - same width as ordering buttons
            edit_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(edit_buttons[i], 60, 60);  // Increased from 55 to 60
            lv_obj_set_pos(edit_buttons[i], 623, y_pos);  // Shifted right by 10px (was 613)
            lv_obj_set_style_bg_color(edit_buttons[i], UITheme::ACCENT_SECONDARY, 0);
            lv_obj_add_event_cb(edit_buttons[i], onEditMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            lv_obj_t *edit_label = lv_label_create(edit_buttons[i]);
            lv_label_set_text(edit_label, LV_SYMBOL_EDIT);
            lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_22, 0);
            lv_obj_center(edit_label);
            
            // Delete button - same width as ordering buttons
            delete_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(delete_buttons[i], 60, 60);  // Increased from 55 to 60
            lv_obj_set_pos(delete_buttons[i], 688, y_pos);  // Shifted right by 15px (was 673)
            lv_obj_set_style_bg_color(delete_buttons[i], UITheme::STATE_ALARM, 0);
            lv_obj_add_event_cb(delete_buttons[i], onDeleteMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            lv_obj_t *delete_label = lv_label_create(delete_buttons[i]);
            lv_label_set_text(delete_label, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_20, 0);
            lv_obj_center(delete_label);
            
            displayed_index++;
        }
    } else {
        // NORMAL MODE: 3x3 flex grid
        lv_obj_clear_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling in normal mode
        lv_obj_set_layout(macro_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(macro_container, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(macro_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(macro_container, 10, 0);
        lv_obj_set_style_pad_gap(macro_container, 13, 0);
        
        for (int i = 0; i < MAX_MACROS; i++) {
            if (!macros[i].is_configured) continue;
            
            macro_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(macro_buttons[i], 240, 73);
            lv_obj_set_style_bg_color(macro_buttons[i], getColorByIndex(macros[i].color_index), 0);
            lv_obj_set_style_pad_all(macro_buttons[i], 10, 0);
            lv_obj_add_event_cb(macro_buttons[i], onMacroClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            lv_obj_t *label = lv_label_create(macro_buttons[i]);
            lv_label_set_text(label, macros[i].name);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_obj_center(label);
        }
    }
    
    // Show/hide empty message based on whether any macros are configured
    int configured_count = getConfiguredMacroCount();
    if (configured_count == 0 && !is_edit_mode) {
        // Clear flex layout and padding for proper centering
        lv_obj_set_layout(macro_container, LV_LAYOUT_NONE);
        lv_obj_set_style_pad_all(macro_container, 0, 0);
        
        // Recreate empty message label since we cleared the container
        lbl_empty_message = lv_label_create(macro_container);
        lv_label_set_text(lbl_empty_message, "No macros configured.\n\nClick " LV_SYMBOL_EDIT " Edit to add macros.");
        lv_obj_set_style_text_font(lbl_empty_message, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_empty_message, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(lbl_empty_message, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_empty_message, 700);  // Constrain width for better text layout
        lv_obj_center(lbl_empty_message);  // Center both horizontally and vertically
    }
}

// Find previous configured macro index
bool UITabMacros::findPreviousConfiguredIndex(int current_index) {
    for (int i = current_index - 1; i >= 0; i--) {
        if (macros[i].is_configured) {
            return true;
        }
    }
    return false;
}

// Find next configured macro index
bool UITabMacros::findNextConfiguredIndex(int current_index) {
    for (int i = current_index + 1; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            return true;
        }
    }
    return false;
}

// Toggle between Normal and Edit modes
void UITabMacros::onEditModeToggle(lv_event_t *e) {
    is_edit_mode = !is_edit_mode;
    
    if (is_edit_mode) {
        lv_obj_add_flag(btn_edit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
        hideProgress();  // Hide progress in edit mode
    } else {
        lv_obj_clear_flag(btn_edit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
    }
    
    refreshMacroList();
}

// Handle macro button click (execute macro)
void UITabMacros::onMacroClicked(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (index < 0 || index >= MAX_MACROS || !macros[index].is_configured) {
        return;
    }
    
    Serial.printf("Macro clicked: %s (%s)\n", macros[index].name, macros[index].file_path);
    
    // Store the macro name for progress display
    strncpy(running_macro_name, macros[index].name, sizeof(running_macro_name) - 1);
    running_macro_name[sizeof(running_macro_name) - 1] = '\0';
    
    // Build the $SD/Run command with full path
    // FluidNC expects: $SD/Run=/sd/fluidtouch/macros/filename.gcode
    char command[256];
    snprintf(command, sizeof(command), "$SD/Run=/sd/fluidtouch/macros/%s\n", macros[index].file_path);
    
    Serial.printf("Executing macro: %s\n", command);
    FluidNCClient::sendCommand(command);
}

// Show Add Macro dialog
void UITabMacros::onAddMacro(lv_event_t *e) {
    // Find first empty slot
    for (int i = 0; i < MAX_MACROS; i++) {
        if (!macros[i].is_configured) {
            editing_index = i;
            showConfigDialog(true);
            return;
        }
    }
    
    Serial.println("No empty macro slots available");
}

// Refresh file list button handler
void UITabMacros::onRefreshFiles(lv_event_t *e) {
    Serial.println("[Macros] Refresh button clicked");
    loadMacroFilesFromSD();
}

// Show Edit Macro dialog
void UITabMacros::onEditMacro(lv_event_t *e) {
    editing_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    showConfigDialog(false);
}

// Show Delete Confirmation dialog
void UITabMacros::onDeleteMacro(lv_event_t *e) {
    editing_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    showDeleteConfirmDialog();
}

// Move macro up
void UITabMacros::onMoveUpMacro(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find previous configured macro
    for (int i = index - 1; i >= 0; i--) {
        if (macros[i].is_configured) {
            swapMacros(index, i);
            return;
        }
    }
}

// Move macro down
void UITabMacros::onMoveDownMacro(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find next configured macro
    for (int i = index + 1; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            swapMacros(index, i);
            return;
        }
    }
}

// Show configuration dialog (Add or Edit)
void UITabMacros::showConfigDialog(bool is_add) {
    // Create modal background
    config_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(config_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(config_dialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(config_dialog, LV_OPA_60, 0);
    lv_obj_set_style_border_width(config_dialog, 0, 0);
    lv_obj_clear_flag(config_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create dialog container
    lv_obj_t *dialog = lv_obj_create(config_dialog);
    lv_obj_set_size(dialog, 600, 450);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_pad_all(dialog, 20, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, is_add ? "Add Macro" : "Edit Macro");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(title, 0, 0);
    
    // Name label
    lv_obj_t *name_label = lv_label_create(dialog);
    lv_label_set_text(name_label, "Name:");
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(name_label, 0, 50);
    
    // Name textarea
    config_name_textarea = lv_textarea_create(dialog);
    lv_obj_set_size(config_name_textarea, 560, 50);
    lv_obj_set_pos(config_name_textarea, 0, 80);
    lv_obj_set_style_text_font(config_name_textarea, &lv_font_montserrat_20, 0);
    lv_textarea_set_max_length(config_name_textarea, 31);
    lv_textarea_set_one_line(config_name_textarea, true);
    lv_obj_add_event_cb(config_name_textarea, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    if (!is_add) {
        lv_textarea_set_text(config_name_textarea, macros[editing_index].name);
    }
    
    // File Path label
    lv_obj_t *path_label = lv_label_create(dialog);
    lv_label_set_text(path_label, "File: /sd/fluidtouch/macros/");
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(path_label, 0, 145);
    
    // Load macro files from SD card
    loadMacroFilesFromSD();
    
    // File Path dropdown
    config_path_dropdown = lv_dropdown_create(dialog);
    lv_obj_set_size(config_path_dropdown, 490, 50);
    lv_obj_set_pos(config_path_dropdown, 0, 175);
    lv_obj_set_style_text_font(config_path_dropdown, &lv_font_montserrat_20, 0);
    
    // Refresh button for file list
    lv_obj_t *btn_refresh = lv_button_create(dialog);
    lv_obj_set_size(btn_refresh, 50, 50);
    lv_obj_set_pos(btn_refresh, 510, 175);
    lv_obj_set_style_bg_color(btn_refresh, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(btn_refresh, onRefreshFiles, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *refresh_icon = lv_label_create(btn_refresh);
    lv_label_set_text(refresh_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(refresh_icon, &lv_font_montserrat_20, 0);
    lv_obj_center(refresh_icon);
    
    // Populate dropdown with files
    if (UITabMacros::macro_files.empty()) {
        lv_dropdown_set_options(config_path_dropdown, "Loading files...");
    } else {
        String options = "";
        int selected_idx = 0;
        for (size_t i = 0; i < UITabMacros::macro_files.size(); i++) {
            if (i > 0) options += "\n";
            options += UITabMacros::macro_files[i].c_str();
            
            // Find the currently selected file if editing
            if (!is_add && macro_files[i] == macros[editing_index].file_path) {
                selected_idx = i;
            }
        }
        lv_dropdown_set_options(config_path_dropdown, options.c_str());
        if (!is_add) {
            lv_dropdown_set_selected(config_path_dropdown, selected_idx);
        }
    }
    
    // Color label
    lv_obj_t *color_label = lv_label_create(dialog);
    lv_label_set_text(color_label, "Color:");
    lv_obj_set_style_text_font(color_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(color_label, 0, 240);
    
    // Color button grid (single row of 8 buttons)
    int initial_color = is_add ? 0 : macros[editing_index].color_index;
    selected_color_index = initial_color;
    
    for (int i = 0; i < 8; i++) {
        config_color_buttons[i] = lv_btn_create(dialog);
        lv_obj_set_size(config_color_buttons[i], 60, 60);
        lv_obj_set_pos(config_color_buttons[i], i * 70, 270);
        lv_obj_set_style_bg_color(config_color_buttons[i], getColorByIndex(i), 0);
        lv_obj_set_style_radius(config_color_buttons[i], 8, 0);
        lv_obj_add_event_cb(config_color_buttons[i], onColorButtonClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Add checkmark to selected color
        if (i == initial_color) {
            lv_obj_set_style_border_width(config_color_buttons[i], 3, 0);
            lv_obj_set_style_border_color(config_color_buttons[i], lv_color_hex(0xFFFFFF), 0);
            
            lv_obj_t *check = lv_label_create(config_color_buttons[i]);
            lv_label_set_text(check, LV_SYMBOL_OK);
            lv_obj_set_style_text_font(check, &lv_font_montserrat_24, 0);
            lv_obj_set_style_text_color(check, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(check);
        } else {
            lv_obj_set_style_border_width(config_color_buttons[i], 1, 0);
            lv_obj_set_style_border_color(config_color_buttons[i], UITheme::BORDER_MEDIUM, 0);
        }
    }
    
    // Cancel button
    // Cancel button (centered: dialog width 600 - 20 padding * 2 = 560 usable, centered pair of 130px buttons with 20px gap)
    lv_obj_t *btn_cancel = lv_btn_create(dialog);
    lv_obj_set_size(btn_cancel, 130, 50);
    lv_obj_set_pos(btn_cancel, 175, 360);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_MEDIUM, 0);
    lv_obj_add_event_cb(btn_cancel, onConfigCancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Save button
    lv_obj_t *btn_save = lv_btn_create(dialog);
    lv_obj_set_size(btn_save, 130, 50);
    lv_obj_set_pos(btn_save, 325, 360);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_save, onConfigSave, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *save_label = lv_label_create(btn_save);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_18, 0);
    lv_obj_center(save_label);
}

// Hide configuration dialog
void UITabMacros::hideConfigDialog() {
    if (keyboard != nullptr) {
        hideKeyboard();
    }
    
    if (config_dialog != nullptr) {
        lv_obj_del(config_dialog);
        config_dialog = nullptr;
        config_name_textarea = nullptr;
        config_path_dropdown = nullptr;
        for (int i = 0; i < 8; i++) {
            config_color_buttons[i] = nullptr;
        }
    }
}

// Save configuration from dialog
void UITabMacros::onConfigSave(lv_event_t *e) {
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    const char *name = lv_textarea_get_text(config_name_textarea);
    int color_index = selected_color_index;
    
    // Get selected file from dropdown
    char path_buffer[128];
    lv_dropdown_get_selected_str(config_path_dropdown, path_buffer, sizeof(path_buffer));
    
    // Validate inputs
    if (strlen(name) == 0) {
        Serial.println("Macro name is required");
        return;
    }
    
    if (strlen(path_buffer) == 0 || strcmp(path_buffer, "Loading files...") == 0) {
        Serial.println("Macro file path is required");
        return;
    }
    
    // Save macro configuration
    strncpy(macros[editing_index].name, name, sizeof(macros[editing_index].name) - 1);
    strncpy(macros[editing_index].file_path, path_buffer, sizeof(macros[editing_index].file_path) - 1);
    macros[editing_index].color_index = color_index;
    macros[editing_index].is_configured = true;
    
    saveMacros();
    hideConfigDialog();
    refreshMacroList();
}

// Cancel configuration dialog
void UITabMacros::onConfigCancel(lv_event_t *e) {
    hideConfigDialog();
}

// Show delete confirmation dialog
void UITabMacros::showDeleteConfirmDialog() {
    // Create modal background
    delete_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(delete_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(delete_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(delete_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(delete_dialog, 0, 0);
    lv_obj_clear_flag(delete_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(delete_dialog);
    lv_obj_set_size(content, 500, 220);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Warning icon and title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text_fmt(title, "%s Delete Macro?", LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    
    // Macro name
    lv_obj_t *name_label = lv_label_create(content);
    lv_label_set_text(name_label, macros[editing_index].name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_label, 450);
    
    // Message
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, "This action cannot be undone.");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::UI_WARNING, 0);
    
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
    lv_obj_add_event_cb(cancel_btn, onDeleteCancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Delete button
    lv_obj_t *delete_btn = lv_btn_create(btn_container);
    lv_obj_set_size(delete_btn, 180, 50);
    lv_obj_set_style_bg_color(delete_btn, UITheme::STATE_ALARM, 0);
    lv_obj_add_event_cb(delete_btn, onDeleteConfirm, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, LV_SYMBOL_TRASH " Delete");
    lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_18, 0);
    lv_obj_center(delete_label);
}

// Hide delete confirmation dialog
void UITabMacros::hideDeleteConfirmDialog() {
    if (delete_dialog != nullptr) {
        lv_obj_del(delete_dialog);
        delete_dialog = nullptr;
    }
}

// Confirm macro deletion
void UITabMacros::onDeleteConfirm(lv_event_t *e) {
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    // Clear macro configuration
    macros[editing_index].is_configured = false;
    memset(macros[editing_index].name, 0, sizeof(macros[editing_index].name));
    memset(macros[editing_index].file_path, 0, sizeof(macros[editing_index].file_path));
    
    saveMacros();
    hideDeleteConfirmDialog();
    refreshMacroList();
}

// Cancel macro deletion
void UITabMacros::onDeleteCancel(lv_event_t *e) {
    hideDeleteConfirmDialog();
}

// Handle color button clicked
void UITabMacros::onColorButtonClicked(lv_event_t *e) {
    int color_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Update selection
    selected_color_index = color_index;
    
    // Update all button styles
    for (int i = 0; i < 8; i++) {
        if (config_color_buttons[i] != nullptr) {
            // Remove any existing checkmark label
            if (lv_obj_get_child_count(config_color_buttons[i]) > 0) {
                lv_obj_clean(config_color_buttons[i]);
            }
            
            if (i == color_index) {
                // Selected: white border and checkmark
                lv_obj_set_style_border_width(config_color_buttons[i], 3, 0);
                lv_obj_set_style_border_color(config_color_buttons[i], lv_color_hex(0xFFFFFF), 0);
                
                lv_obj_t *check = lv_label_create(config_color_buttons[i]);
                lv_label_set_text(check, LV_SYMBOL_OK);
                lv_obj_set_style_text_font(check, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(check, lv_color_hex(0xFFFFFF), 0);
                lv_obj_center(check);
            } else {
                // Unselected: thin gray border
                lv_obj_set_style_border_width(config_color_buttons[i], 1, 0);
                lv_obj_set_style_border_color(config_color_buttons[i], UITheme::BORDER_MEDIUM, 0);
            }
        }
    }
}

// Handle textarea focus (show keyboard)
void UITabMacros::onTextareaFocused(lv_event_t *e) {
    lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);
    showKeyboard(textarea);
}

// Show keyboard
void UITabMacros::showKeyboard(lv_obj_t *textarea) {
    if (keyboard != nullptr) {
        lv_keyboard_set_textarea(keyboard, textarea);
        return;
    }
    
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, 800, 280);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
    lv_keyboard_set_textarea(keyboard, textarea);
    
    // Add event handler for keyboard close button
    lv_obj_add_event_cb(keyboard, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            UITabMacros::hideKeyboard();
        }
    }, LV_EVENT_ALL, nullptr);
}

// Hide keyboard
void UITabMacros::hideKeyboard() {
    if (keyboard != nullptr) {
        lv_obj_del(keyboard);
        keyboard = nullptr;
    }
}

// Load macro files from SD card
void UITabMacros::loadMacroFilesFromSD() {
    UITabMacros::macro_files.clear();
    
    Serial.println("[Macros] Requesting file list from SD card");
    
    // Register callback to receive JSON file list response
    FluidNCClient::setMessageCallback([](const char* message) {
        static String jsonBuffer;
        static bool collecting = false;
        static uint32_t lastMessageTime = 0;
        
        String msg(message);
        msg.trim();
        uint32_t now = millis();
        
        // Reset buffer if timeout
        if (now - lastMessageTime > 3000) {
            if (jsonBuffer.length() > 0 && collecting) {
                Serial.println("[Macros] Timeout - parsing JSON buffer");
                // Parse the accumulated JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, jsonBuffer);
                
                if (!error && doc["files"].is<JsonArray>()) {
                    JsonArray files = doc["files"];
                    for (JsonObject file : files) {
                        if (file["name"].is<const char*>()) {
                            const char* filename = file["name"];
                            UITabMacros::macro_files.push_back(filename);
                        }
                    }
                    Serial.printf("[Macros] Found %d macro files\n", UITabMacros::macro_files.size());
                    
                    // Sort files alphabetically (case-insensitive)
                    std::sort(UITabMacros::macro_files.begin(), UITabMacros::macro_files.end(),
                        [](const std::string &a, const std::string &b) {
                            std::string a_lower = a;
                            std::string b_lower = b;
                            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
                            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
                            return a_lower < b_lower;
                        });
                    
                    // Update dropdown if it exists
                    if (UITabMacros::config_path_dropdown) {
                        String options = "";
                        for (size_t i = 0; i < UITabMacros::macro_files.size(); i++) {
                            if (i > 0) options += "\n";
                            options += UITabMacros::macro_files[i].c_str();
                        }
                        lv_dropdown_set_options(UITabMacros::config_path_dropdown, options.c_str());
                        Serial.println("[Macros] Dropdown updated with file list");
                    }
                }
                FluidNCClient::clearMessageCallback();
            }
            jsonBuffer = "";
            collecting = false;
        }
        lastMessageTime = now;
        
        // Skip status reports and other messages
        if (msg.startsWith("<") || msg.startsWith("[GC:") || msg.startsWith("[MSG:") || msg.startsWith("PING:")) {
            return;
        }
        
        // Check for end of response
        if (msg.equalsIgnoreCase("ok")) {
            if (collecting) {
                Serial.println("[Macros] Received 'ok', parsing JSON");
                // Parse the accumulated JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, jsonBuffer);
                
                if (!error && doc["files"].is<JsonArray>()) {
                    JsonArray files = doc["files"];
                    for (JsonObject file : files) {
                        if (file["name"].is<const char*>()) {
                            const char* filename = file["name"];
                            UITabMacros::macro_files.push_back(filename);
                        }
                    }
                    Serial.printf("[Macros] Found %d macro files\n", UITabMacros::macro_files.size());
                    
                    // Sort files alphabetically (case-insensitive)
                    std::sort(UITabMacros::macro_files.begin(), UITabMacros::macro_files.end(),
                        [](const std::string &a, const std::string &b) {
                            std::string a_lower = a;
                            std::string b_lower = b;
                            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
                            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
                            return a_lower < b_lower;
                        });
                    
                    // Update dropdown if it exists
                    if (UITabMacros::config_path_dropdown) {
                        String options = "";
                        for (size_t i = 0; i < UITabMacros::macro_files.size(); i++) {
                            if (i > 0) options += "\n";
                            options += UITabMacros::macro_files[i].c_str();
                        }
                        lv_dropdown_set_options(UITabMacros::config_path_dropdown, options.c_str());
                        Serial.println("[Macros] Dropdown updated with file list");
                    }
                }
                jsonBuffer = "";
                collecting = false;
                FluidNCClient::clearMessageCallback();
            }
            return;
        }
        
        // Start collecting JSON
        if (msg.startsWith("[JSON:") || msg.startsWith("{\"files")) {
            if (!collecting) {
                Serial.println("[Macros] Starting JSON collection");
                collecting = true;
                jsonBuffer = "";
            }
            String jsonLine = msg;
            if (jsonLine.startsWith("[JSON:")) {
                jsonLine.replace("[JSON:", "");
                if (jsonLine.endsWith("]")) {
                    jsonLine.remove(jsonLine.length() - 1);
                }
            }
            jsonBuffer += jsonLine;
        } else if (collecting) {
            // Continue accumulating JSON lines
            jsonBuffer += msg;
        }
    });
    
    // Send command to list files from macros directory
    FluidNCClient::sendCommand("$Files/ListGcode=/sd/fluidtouch/macros\n");
}

// Update progress display
void UITabMacros::updateProgress(int percent, const char* macro_name, const char* message) {
    if (!progress_container || !bar_progress || !lbl_percent || !lbl_macro_name || !lbl_message) return;
    
    // Update progress bar
    lv_bar_set_value(bar_progress, percent, LV_ANIM_OFF);
    
    // Update percentage label
    char percent_text[8];
    snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
    lv_label_set_text(lbl_percent, percent_text);
    
    // Update macro name - use stored name instead of filename
    if (running_macro_name[0] != '\0') {
        char name_text[64];
        snprintf(name_text, sizeof(name_text), "Running: %s", running_macro_name);
        lv_label_set_text(lbl_macro_name, name_text);
    }
    
    // Update message
    if (message && strlen(message) > 0) {
        lv_label_set_text(lbl_message, message);
    }
}

// Show progress display
void UITabMacros::showProgress() {
    if (progress_container && !is_edit_mode) {
        lv_obj_clear_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
    }
}

// Hide progress display
void UITabMacros::hideProgress() {
    if (progress_container) {
        lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
        // Clear the running macro name when hiding
        running_macro_name[0] = '\0';
    }
}
