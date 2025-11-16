#include "ui/ui_machine_select.h"
#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "core/power_manager.h"
#include "config.h"
#include <Preferences.h>

// Static member initialization
lv_obj_t *UIMachineSelect::screen = nullptr;
lv_display_t *UIMachineSelect::display = nullptr;
MachineConfig UIMachineSelect::machines[MAX_MACHINES];
bool UIMachineSelect::edit_mode = false;
lv_obj_t *UIMachineSelect::edit_mode_button = nullptr;
lv_obj_t *UIMachineSelect::list_container = nullptr;  // Machine list container
lv_obj_t *UIMachineSelect::machine_buttons[MAX_MACHINES] = {nullptr};
lv_obj_t *UIMachineSelect::edit_buttons[MAX_MACHINES] = {nullptr};
lv_obj_t *UIMachineSelect::move_up_buttons[MAX_MACHINES] = {nullptr};
lv_obj_t *UIMachineSelect::move_down_buttons[MAX_MACHINES] = {nullptr};
lv_obj_t *UIMachineSelect::delete_buttons[MAX_MACHINES] = {nullptr};
lv_obj_t *UIMachineSelect::add_button = nullptr;
lv_obj_t *UIMachineSelect::config_dialog = nullptr;
lv_obj_t *UIMachineSelect::dialog_content = nullptr;
lv_obj_t *UIMachineSelect::keyboard = nullptr;
int UIMachineSelect::editing_index = -1;
lv_obj_t *UIMachineSelect::ta_name = nullptr;
lv_obj_t *UIMachineSelect::ta_ssid = nullptr;
lv_obj_t *UIMachineSelect::ta_password = nullptr;
lv_obj_t *UIMachineSelect::ta_url = nullptr;
lv_obj_t *UIMachineSelect::ta_port = nullptr;
lv_obj_t *UIMachineSelect::dd_connection_type = nullptr;
lv_obj_t *UIMachineSelect::delete_dialog = nullptr;
int UIMachineSelect::deleting_index = -1;

void UIMachineSelect::show(lv_display_t *disp) {
    display = disp;
    Serial.println("UIMachineSelect: Creating machine selection screen");
    
    // Initialize edit mode to false
    edit_mode = false;
    
    // Load machines from Preferences
    MachineConfigManager::loadMachines(machines);
    
    // Create screen
    screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, UITheme::BG_DARK, 0);
    
    // Title
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Select Machine");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);  // Larger font
    lv_obj_set_style_text_color(title, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 15);
    
    // Button container for right-aligned buttons
    lv_obj_t *btn_container = lv_obj_create(screen);
    lv_obj_set_size(btn_container, LV_SIZE_CONTENT, 45);
    lv_obj_align(btn_container, LV_ALIGN_TOP_RIGHT, -20, 11);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // Start = left-to-right within container
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_pad_column(btn_container, 5, 0);  // 5px gap between buttons
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    
    // Add Machine button (initially hidden, in edit mode)
    add_button = lv_btn_create(btn_container);
    lv_obj_set_size(add_button, 120, 45);
    lv_obj_set_style_bg_color(add_button, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(add_button, onAddMachine, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(add_button, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    lv_obj_add_flag(add_button, LV_OBJ_FLAG_FLOATING);  // Floating so it doesn't affect layout when hidden
    
    lv_obj_t *add_label = lv_label_create(add_button);
    lv_label_set_text(add_label, LV_SYMBOL_PLUS " Add");
    lv_obj_set_style_text_font(add_label, &lv_font_montserrat_16, 0);
    lv_obj_center(add_label);
    
    // Edit Mode toggle button
    edit_mode_button = lv_btn_create(btn_container);
    lv_obj_set_size(edit_mode_button, 120, 45);
    lv_obj_set_style_bg_color(edit_mode_button, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_add_event_cb(edit_mode_button, onEditModeToggle, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *edit_mode_label = lv_label_create(edit_mode_button);
    lv_label_set_text(edit_mode_label, LV_SYMBOL_EDIT " Edit");
    lv_obj_set_style_text_font(edit_mode_label, &lv_font_montserrat_16, 0);
    lv_obj_center(edit_mode_label);
    
    // Power Off button - only show if power management is enabled
    if (PowerManager::isEnabled()) {
        lv_obj_t *power_off_btn = lv_btn_create(btn_container);
        lv_obj_set_size(power_off_btn, 120, 45);
        lv_obj_set_style_bg_color(power_off_btn, lv_color_hex(0xB43000), 0);  // Orange
        lv_obj_add_event_cb(power_off_btn, [](lv_event_t *e) {
            UICommon::showPowerOffConfirmDialog();
        }, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t *power_off_label = lv_label_create(power_off_btn);
        lv_label_set_text(power_off_label, LV_SYMBOL_POWER);  // Just icon to fit
        lv_obj_set_style_text_font(power_off_label, &lv_font_montserrat_20, 0);
        lv_obj_center(power_off_label);
    }
    
    // Machine list container (760px width x 395px height with 20px padding = 720x355 usable)
    list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, 760, 395);  // Taller since no subtitle
    lv_obj_set_style_bg_color(list_container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(list_container, 20, 0);  // Doubled padding (was 10, now 20)
    lv_obj_align(list_container, LV_ALIGN_BOTTOM_MID, 0, -20);  // Moved up 10px (was -10, now -20)
    lv_obj_clear_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    
    refreshMachineList();
    
    // Load screen
    lv_scr_load(screen);
    
    // Automatically show Add Machine dialog if no machines configured
    if (!MachineConfigManager::hasConfiguredMachines()) {
        Serial.println("UIMachineSelect: No machines configured, showing Add Machine dialog");
        // Use a timer to show dialog after screen is fully loaded
        lv_timer_t *timer = lv_timer_create([](lv_timer_t *t) {
            onAddMachine(nullptr);
            lv_timer_del(t);
        }, 100, nullptr);
        lv_timer_set_repeat_count(timer, 1);
    }
    
    Serial.println("UIMachineSelect: Machine selection screen displayed");
}

void UIMachineSelect::hide() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
    }
}

void UIMachineSelect::refreshMachineList() {
    Serial.println("UIMachineSelect::refreshMachineList: Starting refresh");
    
    // Clear existing buttons
    lv_obj_clean(list_container);
    
    // Count configured machines
    int configured_count = getConfiguredMachineCount();
    Serial.printf("UIMachineSelect: Found %d configured machines\n", configured_count);
    
    // Debug: Print each machine state
    for (int i = 0; i < MAX_MACHINES; i++) {
        Serial.printf("  Machine %d: is_configured=%d, name='%s'\n", 
                     i, machines[i].is_configured, machines[i].name);
    }
    
    // Update Add button visibility (only show in edit mode)
    if (edit_mode && configured_count < MAX_MACHINES) {
        lv_obj_clear_flag(add_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(add_button, LV_OBJ_FLAG_FLOATING);  // Remove floating so it participates in flex layout
    } else {
        lv_obj_add_flag(add_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(add_button, LV_OBJ_FLAG_FLOATING);  // Make floating so it doesn't affect layout
    }
    
    // Force flex container to recalculate layout
    lv_obj_update_layout(lv_obj_get_parent(add_button));
    
    if (edit_mode) {
        // EDIT MODE: Show control buttons with narrower machine buttons
        // Clear flex layout for absolute positioning
        lv_obj_set_layout(list_container, LV_LAYOUT_NONE);
        
        int displayed_index = 0;
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (machines[i].is_configured) {
                int y_pos = (displayed_index * 65); // 60px button + 5px gap (matches macro list)
                
                // Machine button - 30px shorter to accommodate control buttons
                machine_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(machine_buttons[i], 458, 60);  // Reduced from 488 to 458 (-30px)
                lv_obj_set_pos(machine_buttons[i], 0, y_pos);
                lv_obj_set_style_bg_color(machine_buttons[i], UITheme::ACCENT_PRIMARY, 0);
                lv_obj_set_style_bg_color(machine_buttons[i], UITheme::ACCENT_SECONDARY, LV_STATE_PRESSED);
                // Don't add click event in edit mode - user should use Edit button instead
                
                // Machine label with symbol
                lv_obj_t *label = lv_label_create(machine_buttons[i]);
                const char *symbol = (machines[i].connection_type == CONN_WIRELESS) ? LV_SYMBOL_WIFI : LV_SYMBOL_USB;
                String text = String(symbol) + " " + String(machines[i].name);
                lv_label_set_text(label, text.c_str());
                lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
                lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
                
                // Move Up button (5px gap from machine button)
                move_up_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(move_up_buttons[i], 60, 60);  // Match macro list: 60×60
                lv_obj_set_pos(move_up_buttons[i], 463, y_pos);  // 458 + 5px gap
                lv_obj_set_style_bg_color(move_up_buttons[i], UITheme::BG_BUTTON, 0);
                lv_obj_add_event_cb(move_up_buttons[i], onMoveUpMachine, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                
                // Disable if first item
                if (displayed_index == 0) {
                    lv_obj_add_state(move_up_buttons[i], LV_STATE_DISABLED);
                }
                
                lv_obj_t *up_label = lv_label_create(move_up_buttons[i]);
                lv_label_set_text(up_label, LV_SYMBOL_UP);
                lv_obj_set_style_text_font(up_label, &lv_font_montserrat_22, 0);
                lv_obj_center(up_label);
                
                // Move Down button (5px gap from up button)
                move_down_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(move_down_buttons[i], 60, 60);  // Match macro list: 60×60
                lv_obj_set_pos(move_down_buttons[i], 528, y_pos);  // 463 + 60 + 5px gap
                lv_obj_set_style_bg_color(move_down_buttons[i], UITheme::BG_BUTTON, 0);
                lv_obj_add_event_cb(move_down_buttons[i], onMoveDownMachine, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                
                // Disable if last item
                if (displayed_index == configured_count - 1) {
                    lv_obj_add_state(move_down_buttons[i], LV_STATE_DISABLED);
                }
                
                lv_obj_t *down_label = lv_label_create(move_down_buttons[i]);
                lv_label_set_text(down_label, LV_SYMBOL_DOWN);
                lv_obj_set_style_text_font(down_label, &lv_font_montserrat_22, 0);
                lv_obj_center(down_label);
                
                // Edit button (5px gap from down button)
                edit_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(edit_buttons[i], 60, 60);  // Match macro list: 60×60
                lv_obj_set_pos(edit_buttons[i], 593, y_pos);  // 528 + 60 + 5px gap
                lv_obj_set_style_bg_color(edit_buttons[i], UITheme::ACCENT_SECONDARY, 0);
                lv_obj_add_event_cb(edit_buttons[i], onEditMachine, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                
                lv_obj_t *edit_label = lv_label_create(edit_buttons[i]);
                lv_label_set_text(edit_label, LV_SYMBOL_EDIT);
                lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_22, 0);
                lv_obj_center(edit_label);
                
                // Delete button (5px gap from edit button)
                delete_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(delete_buttons[i], 60, 60);  // Match macro list: 60×60
                lv_obj_set_pos(delete_buttons[i], 658, y_pos);  // 593 + 60 + 5px gap
                lv_obj_set_style_bg_color(delete_buttons[i], UITheme::STATE_ALARM, 0);
                lv_obj_add_event_cb(delete_buttons[i], onDeleteMachine, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                
                lv_obj_t *del_label = lv_label_create(delete_buttons[i]);
                lv_label_set_text(del_label, LV_SYMBOL_TRASH);
                lv_obj_set_style_text_font(del_label, &lv_font_montserrat_20, 0);
                lv_obj_center(del_label);
                
                displayed_index++;
            }
        }
    } else {
        // NORMAL MODE: Full-width machine buttons with flex layout
        Serial.println("UIMachineSelect: NORMAL MODE - creating machine buttons");
        
        // Set list container to use flex layout
        lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_gap(list_container, 20, 0);  // Doubled gap between buttons (was 10, now 20)
        
        // Display configured machines with flex layout
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (machines[i].is_configured) {
                Serial.printf("  Creating button for machine %d: %s\n", i, machines[i].name);
                
                // Machine button - sized for 2 rows of 2 buttons (4 machines total)
                // Container: 720px usable (760 - 40 padding), with 20px gap = 340px per button (conservative)
                machine_buttons[i] = lv_btn_create(list_container);
                lv_obj_set_size(machine_buttons[i], 349, 167);  // Width: 349px (+1), Height: 167px
                lv_obj_set_style_bg_color(machine_buttons[i], UITheme::ACCENT_PRIMARY, 0);
                lv_obj_set_style_bg_color(machine_buttons[i], UITheme::ACCENT_SECONDARY, LV_STATE_PRESSED);
                lv_obj_add_event_cb(machine_buttons[i], onMachineSelected, LV_EVENT_CLICKED, (void*)(intptr_t)i);
                lv_obj_set_style_pad_all(machine_buttons[i], 20, 0);  // Double padding (was 10, now 20)
                
                // Line 1: Machine Name (centered, supports 2 lines)
                lv_obj_t *name_label = lv_label_create(machine_buttons[i]);
                lv_label_set_text(name_label, machines[i].name);
                lv_obj_set_style_text_font(name_label, &lv_font_montserrat_32, 0);  // Large font
                lv_obj_set_style_text_color(name_label, UITheme::TEXT_LIGHT, 0);
                lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);  // Enable text wrapping
                lv_obj_set_width(name_label, 309);  // Set width for wrapping (349 - 40px padding)
                lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 0);  // Centered horizontally
                
                // Line 2: Connection type symbol + SSID/Wired (bottom area)
                lv_obj_t *connection_label = lv_label_create(machine_buttons[i]);
                String connection_text;
                if (machines[i].connection_type == CONN_WIRELESS) {
                    connection_text = String(LV_SYMBOL_WIFI) + " " + String(machines[i].ssid);
                } else {
                    connection_text = String(LV_SYMBOL_USB) + " Wired";
                }
                lv_label_set_text(connection_label, connection_text.c_str());
                lv_obj_set_style_text_font(connection_label, &lv_font_montserrat_26, 0);  // Larger font
                lv_obj_set_style_text_color(connection_label, UITheme::UI_INFO, 0);
                lv_obj_align(connection_label, LV_ALIGN_BOTTOM_LEFT, 0, -30);  // 30px from bottom
                
                // Line 3: FluidNC URL:Port (bottom area)
                lv_obj_t *url_label = lv_label_create(machine_buttons[i]);
                char url_text[128];
                snprintf(url_text, sizeof(url_text), "%s:%d", 
                        machines[i].fluidnc_url, machines[i].websocket_port);
                lv_label_set_text(url_label, url_text);
                lv_obj_set_style_text_font(url_label, &lv_font_montserrat_24, 0);  // Larger font
                lv_obj_set_style_text_color(url_label, UITheme::TEXT_MEDIUM, 0);
                lv_obj_align(url_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);  // At bottom
                
                // Store null for control buttons (not displayed in normal mode)
                move_up_buttons[i] = nullptr;
                move_down_buttons[i] = nullptr;
                edit_buttons[i] = nullptr;
                delete_buttons[i] = nullptr;
            }
        }
    }
}

// Helper function: Count configured machines
int UIMachineSelect::getConfiguredMachineCount() {
    int count = 0;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].is_configured) {
            count++;
        }
    }
    return count;
}

// Helper function: Swap two machines in the array
void UIMachineSelect::swapMachines(int index1, int index2) {
    if (index1 < 0 || index1 >= MAX_MACHINES || index2 < 0 || index2 >= MAX_MACHINES) {
        Serial.printf("UIMachineSelect::swapMachines: Invalid indices %d, %d\n", index1, index2);
        return;
    }
    
    if (!machines[index1].is_configured || !machines[index2].is_configured) {
        Serial.printf("UIMachineSelect::swapMachines: Cannot swap unconfigured machines\n");
        return;
    }
    
    Serial.printf("UIMachineSelect::swapMachines: Swapping %d <-> %d\n", index1, index2);
    
    // Swap the machines
    MachineConfig temp = machines[index1];
    machines[index1] = machines[index2];
    machines[index2] = temp;
    
    // Save to preferences
    MachineConfigManager::saveMachines(machines);
    
    // Refresh the display
    refreshMachineList();
}

void UIMachineSelect::onMoveUpMachine(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find previous configured machine
    int prev_index = -1;
    for (int i = index - 1; i >= 0; i--) {
        if (machines[i].is_configured) {
            prev_index = i;
            break;
        }
    }
    
    if (prev_index >= 0) {
        Serial.printf("UIMachineSelect: Move up machine %d\n", index);
        swapMachines(index, prev_index);
    }
}

void UIMachineSelect::onMoveDownMachine(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find next configured machine
    int next_index = -1;
    for (int i = index + 1; i < MAX_MACHINES; i++) {
        if (machines[i].is_configured) {
            next_index = i;
            break;
        }
    }
    
    if (next_index >= 0) {
        Serial.printf("UIMachineSelect: Move down machine %d\n", index);
        swapMachines(index, next_index);
    }
}

void UIMachineSelect::onEditModeToggle(lv_event_t *e) {
    // Toggle edit mode
    edit_mode = !edit_mode;
    
    Serial.printf("UIMachineSelect: Edit mode %s\n", edit_mode ? "enabled" : "disabled");
    
    // Update button text and color
    lv_obj_t *label = lv_obj_get_child(edit_mode_button, 0);
    if (edit_mode) {
        lv_label_set_text(label, LV_SYMBOL_CLOSE " Done");
        lv_obj_set_style_bg_color(edit_mode_button, UITheme::BTN_PLAY, 0);
    } else {
        lv_label_set_text(label, LV_SYMBOL_EDIT " Edit");
        lv_obj_set_style_bg_color(edit_mode_button, UITheme::ACCENT_SECONDARY, 0);
    }
    
    // Refresh the machine list to show/hide control buttons
    refreshMachineList();
}

void UIMachineSelect::onMachineSelected(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    Serial.printf("UIMachineSelect: Machine selected: %s (index %d)\n", machines[index].name, index);
    
    // Check if WiFi password is set for wireless machines
    if (machines[index].connection_type == CONN_WIRELESS && strlen(machines[index].password) == 0) {
        Serial.println("UIMachineSelect: WiFi password not set!");
        
        // Create modal backdrop
        lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
        lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
        lv_obj_set_style_border_width(backdrop, 0, 0);
        lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(backdrop);
        
        // Show warning dialog (consistent with System Options popup style)
        lv_obj_t *dialog = lv_obj_create(backdrop);
        lv_obj_set_size(dialog, 600, 300);
        lv_obj_center(dialog);
        lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
        lv_obj_set_style_border_width(dialog, 3, 0);
        lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
        lv_obj_set_style_pad_all(dialog, 20, 0);
        lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
        
        // Title (positioned near top)
        lv_obj_t *title = lv_label_create(dialog);
        lv_label_set_text(title, LV_SYMBOL_WARNING " WiFi Password Required");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
        
        // Message (centered in available space)
        lv_obj_t *message = lv_label_create(dialog);
        char msg[256];
        snprintf(msg, sizeof(msg), 
            "Machine: %s\n\n"
            "This machine requires a WiFi password\n"
            "but none is configured.\n\n"
            "Please edit the machine configuration and set the\n"
            "WiFi password before attempting to connect.",
            machines[index].name);
        lv_label_set_text(message, msg);
        lv_obj_set_style_text_font(message, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(message, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(message, 560);
        lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 50);
        
        // Button container (positioned at bottom)
        lv_obj_t *btn_container = lv_obj_create(dialog);
        lv_obj_set_size(btn_container, 560, 60);
        lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_style_pad_all(btn_container, 0, 0);
        lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
        
        // OK button (centered in button container)
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
        
        return;  // Don't proceed with machine selection
    }
    
    // Save selection index to preferences
    MachineConfigManager::setSelectedMachineIndex(index);
    
    // Create main UI (creates and loads new screen first)
    UICommon::createMainUI();
    
    // Now safe to delete the machine selection screen
    hide();
}

void UIMachineSelect::onEditMachine(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("UIMachineSelect: Edit machine %d\n", index);
    showConfigDialog(index);
}

void UIMachineSelect::onAddMachine(lv_event_t *e) {
    // Find first available unconfigured slot
    int index = -1;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (!machines[i].is_configured) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        Serial.printf("UIMachineSelect: Add machine to slot %d\n", index);
        showConfigDialog(index);
    } else {
        Serial.println("UIMachineSelect: No available slots");
    }
}

void UIMachineSelect::onDeleteMachine(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("UIMachineSelect: Delete machine %d - showing confirmation\n", index);
    showDeleteConfirmDialog(index);
}

void UIMachineSelect::onDeleteConfirm(lv_event_t *e) {
    Serial.printf("UIMachineSelect: Delete confirmed for machine %d\n", deleting_index);
    
    MachineConfigManager::deleteMachine(deleting_index);
    MachineConfigManager::loadMachines(machines);
    
    hideDeleteConfirmDialog();
    refreshMachineList();
}

void UIMachineSelect::onDeleteCancel(lv_event_t *e) {
    Serial.println("UIMachineSelect: Delete cancelled");
    hideDeleteConfirmDialog();
}

void UIMachineSelect::onConfigSave(lv_event_t *e) {
    Serial.println("UIMachineSelect: Save configuration");
    
    // Read values from dialog
    const char *name = lv_textarea_get_text(ta_name);
    const char *ssid = lv_textarea_get_text(ta_ssid);
    const char *password = lv_textarea_get_text(ta_password);
    const char *url = lv_textarea_get_text(ta_url);
    const char *port_str = lv_textarea_get_text(ta_port);
    uint16_t sel = lv_dropdown_get_selected(dd_connection_type);
    
    // Validate
    if (strlen(name) == 0) {
        Serial.println("UIMachineSelect: Machine name required");
        return;
    }
    
    // Create config
    MachineConfig config;
    strncpy(config.name, name, sizeof(config.name) - 1);
    config.connection_type = (sel == 0) ? CONN_WIRELESS : CONN_WIRED;
    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    strncpy(config.password, password, sizeof(config.password) - 1);
    strncpy(config.fluidnc_url, url, sizeof(config.fluidnc_url) - 1);
    config.websocket_port = atoi(port_str);
    if (config.websocket_port == 0) config.websocket_port = 81;
    config.is_configured = true;
    
    // Save
    MachineConfigManager::saveMachine(editing_index, config);
    MachineConfigManager::loadMachines(machines);
    
    hideConfigDialog();
    refreshMachineList();
}

void UIMachineSelect::onConfigCancel(lv_event_t *e) {
    Serial.println("UIMachineSelect: Cancel configuration");
    hideConfigDialog();
}

void UIMachineSelect::onConnectionTypeChanged(lv_event_t *e) {
    updateConnectionFields();
}

void UIMachineSelect::updateConnectionFields() {
    uint16_t sel = lv_dropdown_get_selected(dd_connection_type);
    bool is_wireless = (sel == 0);
    
    // Enable/disable wireless-specific fields
    if (is_wireless) {
        lv_obj_clear_state(ta_ssid, LV_STATE_DISABLED);
        lv_obj_clear_state(ta_password, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ta_ssid, LV_STATE_DISABLED);
        lv_obj_add_state(ta_password, LV_STATE_DISABLED);
    }
}

void UIMachineSelect::showConfigDialog(int index) {
    editing_index = index;
    bool is_new = !machines[index].is_configured;
    
    // Create modal background
    config_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(config_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(config_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(config_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(config_dialog, 0, 0);
    lv_obj_clear_flag(config_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Scrollable dialog content container (780x460 to fit on screen with margin)
    dialog_content = lv_obj_create(config_dialog);
    lv_obj_set_size(dialog_content, 780, 460);
    lv_obj_center(dialog_content);
    lv_obj_set_style_bg_color(dialog_content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(dialog_content, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_border_width(dialog_content, 2, 0);
    lv_obj_set_layout(dialog_content, LV_LAYOUT_NONE);  // Use absolute positioning instead of flex
    lv_obj_set_style_pad_all(dialog_content, 20, 0);
    lv_obj_clear_flag(dialog_content, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    
    // Title (uppercase, gray like settings section titles)
    lv_obj_t *dlg_title = lv_label_create(dialog_content);
    lv_label_set_text(dlg_title, is_new ? "ADD MACHINE" : "EDIT MACHINE");
    lv_obj_set_style_text_font(dlg_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(dlg_title, UITheme::TEXT_DISABLED, 0);  // Gray color
    lv_obj_set_pos(dlg_title, 0, 0);
    lv_obj_set_width(dlg_title, 740);  // 780 - 40px padding
    
    // Main 2-column container for fields
    lv_obj_t *fields_container = lv_obj_create(dialog_content);
    lv_obj_set_size(fields_container, 740, LV_SIZE_CONTENT);  // 780 - 40px padding
    lv_obj_set_pos(fields_container, 0, 35);  // Below title
    lv_obj_set_flex_flow(fields_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fields_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(fields_container, 0, 0);
    lv_obj_set_style_border_width(fields_container, 0, 0);
    lv_obj_set_style_bg_opa(fields_container, LV_OPA_TRANSP, 0);
    
    // LEFT COLUMN (2/3 width): Name, WiFi SSID, FluidNC URL
    lv_obj_t *left_col = lv_obj_create(fields_container);
    lv_obj_set_size(left_col, LV_PCT(64), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(left_col, 0, 0);
    lv_obj_set_style_pad_gap(left_col, 10, 0);
    lv_obj_set_style_border_width(left_col, 0, 0);
    lv_obj_set_style_bg_opa(left_col, LV_OPA_TRANSP, 0);
    
    // Name field
    lv_obj_t *lbl_name = lv_label_create(left_col);
    lv_label_set_text(lbl_name, "Name:");
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_18, 0);
    
    ta_name = lv_textarea_create(left_col);
    lv_obj_set_width(ta_name, LV_PCT(100));
    lv_obj_set_height(ta_name, 40);
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_max_length(ta_name, 31);
    lv_obj_set_style_text_font(ta_name, &lv_font_montserrat_18, 0);
    if (!is_new) lv_textarea_set_text(ta_name, machines[index].name);
    lv_obj_add_event_cb(ta_name, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    // WiFi SSID field
    lv_obj_t *lbl_ssid = lv_label_create(left_col);
    lv_label_set_text(lbl_ssid, "WiFi SSID:");
    lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_18, 0);
    
    ta_ssid = lv_textarea_create(left_col);
    lv_obj_set_width(ta_ssid, LV_PCT(100));
    lv_obj_set_height(ta_ssid, 40);
    lv_textarea_set_one_line(ta_ssid, true);
    lv_textarea_set_max_length(ta_ssid, 32);
    lv_obj_set_style_text_font(ta_ssid, &lv_font_montserrat_18, 0);
    if (!is_new) lv_textarea_set_text(ta_ssid, machines[index].ssid);
    lv_obj_add_event_cb(ta_ssid, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    // FluidNC URL field
    lv_obj_t *lbl_url = lv_label_create(left_col);
    lv_label_set_text(lbl_url, "FluidNC URL:");
    lv_obj_set_style_text_font(lbl_url, &lv_font_montserrat_18, 0);
    
    ta_url = lv_textarea_create(left_col);
    lv_obj_set_width(ta_url, LV_PCT(100));
    lv_obj_set_height(ta_url, 40);
    lv_textarea_set_one_line(ta_url, true);
    lv_textarea_set_max_length(ta_url, 127);
    lv_obj_set_style_text_font(ta_url, &lv_font_montserrat_18, 0);
    if (!is_new) {
        lv_textarea_set_text(ta_url, machines[index].fluidnc_url);
    } else {
        lv_textarea_set_text(ta_url, "fluidnc.local");
    }
    lv_obj_add_event_cb(ta_url, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    // RIGHT COLUMN (1/3 width): Connection, Password, Port
    lv_obj_t *right_col = lv_obj_create(fields_container);
    lv_obj_set_size(right_col, LV_PCT(33), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(right_col, 0, 0);
    lv_obj_set_style_pad_gap(right_col, 10, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    
    // Connection Type
    lv_obj_t *lbl_type = lv_label_create(right_col);
    lv_label_set_text(lbl_type, "Connection:");
    lv_obj_set_style_text_font(lbl_type, &lv_font_montserrat_18, 0);
    
    dd_connection_type = lv_dropdown_create(right_col);
    lv_obj_set_width(dd_connection_type, LV_PCT(100));
    lv_obj_set_height(dd_connection_type, 48);
    lv_obj_set_style_text_font(dd_connection_type, &lv_font_montserrat_18, 0);
    lv_obj_set_style_pad_top(dd_connection_type, 12, LV_PART_MAIN);  // Adjust top padding to vertically center text
    lv_dropdown_set_options(dd_connection_type, "Wireless");  // Wired option hidden for now, reserved for future
    if (!is_new) lv_dropdown_set_selected(dd_connection_type, machines[index].connection_type);
    lv_obj_add_event_cb(dd_connection_type, onConnectionTypeChanged, LV_EVENT_VALUE_CHANGED, nullptr);
    
    // Password field
    lv_obj_t *lbl_pwd = lv_label_create(right_col);
    lv_label_set_text(lbl_pwd, "Password:");
    lv_obj_set_style_text_font(lbl_pwd, &lv_font_montserrat_18, 0);
    
    ta_password = lv_textarea_create(right_col);
    lv_obj_set_width(ta_password, LV_PCT(100));
    lv_obj_set_height(ta_password, 40);
    lv_textarea_set_one_line(ta_password, true);
    lv_textarea_set_max_length(ta_password, 63);
    lv_textarea_set_password_mode(ta_password, true);
    lv_obj_set_style_text_font(ta_password, &lv_font_montserrat_18, 0);
    if (!is_new) lv_textarea_set_text(ta_password, machines[index].password);
    lv_obj_add_event_cb(ta_password, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    // Port field
    lv_obj_t *lbl_port = lv_label_create(right_col);
    lv_label_set_text(lbl_port, "Port:");
    lv_obj_set_style_text_font(lbl_port, &lv_font_montserrat_18, 0);
    
    ta_port = lv_textarea_create(right_col);
    lv_obj_set_width(ta_port, LV_PCT(100));
    lv_obj_set_height(ta_port, 40);
    lv_textarea_set_one_line(ta_port, true);
    lv_textarea_set_max_length(ta_port, 5);
    lv_textarea_set_accepted_chars(ta_port, "0123456789");
    lv_obj_set_style_text_font(ta_port, &lv_font_montserrat_18, 0);
    if (!is_new) {
        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", machines[index].websocket_port);
        lv_textarea_set_text(ta_port, port_str);
    } else {
        lv_textarea_set_text(ta_port, "81");
    }
    lv_obj_add_event_cb(ta_port, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    // Button container at bottom with absolute positioning
    lv_obj_t *btn_container = lv_obj_create(dialog_content);
    lv_obj_set_size(btn_container, 740, 50);  // 780 - 40px padding
    lv_obj_set_pos(btn_container, 0, 370);  // 460 - 20 padding - 50 height - 20 gap = 370
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    
    lv_obj_t *btn_save = lv_btn_create(btn_container);
    lv_obj_set_size(btn_save, LV_PCT(48), 50);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_save, onConfigSave, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_OK " Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_save);
    
    lv_obj_t *btn_cancel = lv_btn_create(btn_container);
    lv_obj_set_size(btn_cancel, LV_PCT(48), 50);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_cancel, onConfigCancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
    
    // Initialize connection fields state
    updateConnectionFields();
}

void UIMachineSelect::hideConfigDialog() {
    hideKeyboard();
    if (config_dialog) {
        lv_obj_del(config_dialog);
        config_dialog = nullptr;
        dialog_content = nullptr;
    }
    editing_index = -1;
}

void UIMachineSelect::showDeleteConfirmDialog(int index) {
    deleting_index = index;
    
    // Create modal background
    delete_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(delete_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(delete_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(delete_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(delete_dialog, 0, 0);
    lv_obj_clear_flag(delete_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(delete_dialog);
    lv_obj_set_size(content, 500, 200);
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
    lv_label_set_text_fmt(title, "%s Delete Machine?", LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    
    // Machine name
    lv_obj_t *name_label = lv_label_create(content);
    lv_label_set_text_fmt(name_label, "\"%s\"", machines[index].name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name_label, UITheme::TEXT_LIGHT, 0);
    
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

void UIMachineSelect::hideDeleteConfirmDialog() {
    if (delete_dialog) {
        lv_obj_del(delete_dialog);
        delete_dialog = nullptr;
    }
    deleting_index = -1;
}

void UIMachineSelect::onTextareaFocused(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t*)lv_event_get_target(e);
    showKeyboard(ta);
}

void UIMachineSelect::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        // Create keyboard at screen level (not inside dialog) so it stays fixed at bottom
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, 220);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
        
        // Add event handler for keyboard ready/cancel events
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) {
            UIMachineSelect::hideKeyboard();
        }, LV_EVENT_READY, nullptr);
        
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) {
            UIMachineSelect::hideKeyboard();
        }, LV_EVENT_CANCEL, nullptr);
        
        // Add click event to config_dialog background to close keyboard when clicking outside
        lv_obj_add_event_cb(config_dialog, [](lv_event_t *e) {
            UIMachineSelect::hideKeyboard();
        }, LV_EVENT_CLICKED, nullptr);
        
        // Add click event to dialog_content to close keyboard when clicking on dialog
        lv_obj_add_event_cb(dialog_content, [](lv_event_t *e) {
            lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
            // Only close if clicking directly on dialog_content (not its children like textareas)
            if (target == dialog_content) {
                UIMachineSelect::hideKeyboard();
            }
        }, LV_EVENT_CLICKED, nullptr);
        
        // Make dialog_content scrollable and add extra padding at bottom for keyboard
        lv_obj_add_flag(dialog_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_bottom(dialog_content, 240, 0); // Extra space for scrolling (keyboard height + margin)
    }
    
    lv_keyboard_set_textarea(keyboard, ta);
    
    // Switch to number mode if editing the port field
    if (ta == ta_port) {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    } else {
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    }
    
    // Scroll the dialog content to position the focused textarea just above keyboard
    if (dialog_content && ta) {
        // Get textarea position within dialog_content
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_obj_t *parent = lv_obj_get_parent(ta);
        
        // Walk up parent hierarchy to get cumulative Y position
        while (parent && parent != dialog_content) {
            ta_y += lv_obj_get_y(parent);
            parent = lv_obj_get_parent(parent);
        }
        
        // Calculate scroll position to place textarea just above keyboard
        // Dialog is 460px tall, keyboard is 220px, so visible area is 240px
        // Position textarea at bottom of visible area (240px from dialog top) minus field height and margin
        lv_coord_t dialog_height = lv_obj_get_height(dialog_content);
        lv_coord_t visible_height = 240; // Height above keyboard
        lv_coord_t ta_height = lv_obj_get_height(ta);
        lv_coord_t target_position = visible_height - ta_height - 15; // 15px margin above keyboard (5px gap + 10px padding)
        
        // Scroll amount = (textarea Y position) - (where we want it)
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) scroll_y = 0; // Don't scroll past top
        
        lv_obj_scroll_to_y(dialog_content, scroll_y, LV_ANIM_ON);
    }
}

void UIMachineSelect::hideKeyboard() {
    if (keyboard) {
        lv_obj_del(keyboard);
        keyboard = nullptr;
        
        // Restore dialog_content to non-scrollable and remove extra padding
        if (dialog_content) {
            lv_obj_clear_flag(dialog_content, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_bottom(dialog_content, 20, 0); // Back to original padding
            lv_obj_scroll_to_y(dialog_content, 0, LV_ANIM_ON); // Reset scroll position
        }
        
        // Remove click event from config_dialog (clean up event handler)
        if (config_dialog) {
            lv_obj_remove_event_cb_with_user_data(config_dialog, nullptr, nullptr);
        }
    }
}
