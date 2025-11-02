#include "ui/tabs/control/ui_tab_control_actions.h"
#include "ui/ui_theme.h"
#include "network/fluidnc_client.h"

// Static member initialization
lv_obj_t *UITabControlActions::btn_pause = nullptr;
lv_obj_t *UITabControlActions::lbl_pause = nullptr;

void UITabControlActions::create(lv_obj_t *tab) {
    const int col_width = 180;
    const int col_spacing = 40;  // Increased from 20 to 40 (20px more per gap = 40px total)
    const int left_col_x = 10;
    const int middle_col_x = left_col_x + col_width + col_spacing;
    const int right_col_x = middle_col_x + col_width + col_spacing;
    const int btn_height = 55;  // Uniform button height
    const int spacing = 10;     // Vertical spacing between buttons
    
    // ========== LEFT COLUMN: Control Buttons ==========
    lv_obj_t *control_label = lv_label_create(tab);
    lv_label_set_text(control_label, "CONTROL");
    lv_obj_set_style_text_font(control_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(control_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(control_label, left_col_x, 10);
    
    int y_pos = 40;
    
    btn_pause = lv_button_create(tab);
    lv_obj_set_size(btn_pause, col_width, btn_height);
    lv_obj_set_pos(btn_pause, left_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_pause, UITheme::STATE_HOLD, LV_PART_MAIN);  // Dark yellow for pause
    lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, LV_SYMBOL_PAUSE " Pause");
    lv_obj_set_style_text_font(lbl_pause, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_pause);
    lv_obj_add_event_cb(btn_pause, onPauseResumeClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    lv_obj_t *btn_unlock = lv_button_create(tab);
    lv_obj_set_size(btn_unlock, col_width, btn_height);
    lv_obj_set_pos(btn_unlock, left_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_unlock, UITheme::ACCENT_PRIMARY, LV_PART_MAIN);  // Blue for unlock
    lv_obj_t *lbl_unlock = lv_label_create(btn_unlock);
    lv_label_set_text(lbl_unlock, LV_SYMBOL_OK " Unlock");
    lv_obj_set_style_text_font(lbl_unlock, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_unlock);
    lv_obj_add_event_cb(btn_unlock, onUnlockClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, col_width, btn_height);
    lv_obj_set_pos(btn_reset, left_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_reset, UITheme::UI_WARNING, LV_PART_MAIN);  // Orange for reset
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, LV_SYMBOL_REFRESH " Reset");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, onSoftResetClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Quick Stop at bottom of left column
    lv_obj_t *btn_estop = lv_button_create(tab);
    lv_obj_set_size(btn_estop, col_width, btn_height);
    lv_obj_set_pos(btn_estop, left_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_estop, UITheme::BTN_ESTOP, LV_PART_MAIN);
    lv_obj_t *lbl_estop = lv_label_create(btn_estop);
    lv_label_set_text(lbl_estop, LV_SYMBOL_STOP " STOP");
    lv_obj_set_style_text_font(lbl_estop, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_estop);
    lv_obj_add_event_cb(btn_estop, onQuickStopClicked, LV_EVENT_CLICKED, nullptr);
    
    // ========== MIDDLE COLUMN: Home Axis ==========
    lv_obj_t *home_label = lv_label_create(tab);
    lv_label_set_text(home_label, "HOME AXIS");
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(home_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(home_label, middle_col_x, 10);
    
    y_pos = 40;
    
    // Home X
    lv_obj_t *btn_home_x = lv_button_create(tab);
    lv_obj_set_size(btn_home_x, col_width, btn_height);
    lv_obj_set_pos(btn_home_x, middle_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_home_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_home_x = lv_label_create(btn_home_x);
    lv_label_set_text(lbl_home_x, LV_SYMBOL_HOME " X");
    lv_obj_set_style_text_font(lbl_home_x, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_home_x);
    lv_obj_add_event_cb(btn_home_x, onHomeXClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Home Y
    lv_obj_t *btn_home_y = lv_button_create(tab);
    lv_obj_set_size(btn_home_y, col_width, btn_height);
    lv_obj_set_pos(btn_home_y, middle_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_home_y, UITheme::AXIS_Y, LV_PART_MAIN);
    lv_obj_t *lbl_home_y = lv_label_create(btn_home_y);
    lv_label_set_text(lbl_home_y, LV_SYMBOL_HOME " Y");
    lv_obj_set_style_text_font(lbl_home_y, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_home_y);
    lv_obj_add_event_cb(btn_home_y, onHomeYClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Home Z
    lv_obj_t *btn_home_z = lv_button_create(tab);
    lv_obj_set_size(btn_home_z, col_width, btn_height);
    lv_obj_set_pos(btn_home_z, middle_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_home_z, UITheme::AXIS_Z, LV_PART_MAIN);
    lv_obj_t *lbl_home_z = lv_label_create(btn_home_z);
    lv_label_set_text(lbl_home_z, LV_SYMBOL_HOME " Z");
    lv_obj_set_style_text_font(lbl_home_z, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_home_z);
    lv_obj_add_event_cb(btn_home_z, onHomeZClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Home All
    lv_obj_t *btn_home_all = lv_button_create(tab);
    lv_obj_set_size(btn_home_all, col_width, btn_height);
    lv_obj_set_pos(btn_home_all, middle_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_home_all, UITheme::AXIS_XY, LV_PART_MAIN);
    lv_obj_t *lbl_home_all = lv_label_create(btn_home_all);
    lv_label_set_text(lbl_home_all, LV_SYMBOL_HOME " All");
    lv_obj_set_style_text_font(lbl_home_all, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_home_all);
    lv_obj_add_event_cb(btn_home_all, onHomeAllClicked, LV_EVENT_CLICKED, nullptr);
    
    // ========== RIGHT COLUMN: Zero Axis ==========
    lv_obj_t *zero_label = lv_label_create(tab);
    lv_label_set_text(zero_label, "ZERO AXIS");
    lv_obj_set_style_text_font(zero_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(zero_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(zero_label, right_col_x, 10);
    
    y_pos = 40;
    
    // Zero X
    lv_obj_t *btn_zero_x = lv_button_create(tab);
    lv_obj_set_size(btn_zero_x, col_width, btn_height);
    lv_obj_set_pos(btn_zero_x, right_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_zero_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_zero_x = lv_label_create(btn_zero_x);
    lv_label_set_text(lbl_zero_x, LV_SYMBOL_GPS " X");
    lv_obj_set_style_text_font(lbl_zero_x, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_zero_x);
    lv_obj_add_event_cb(btn_zero_x, onZeroXClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Zero Y
    lv_obj_t *btn_zero_y = lv_button_create(tab);
    lv_obj_set_size(btn_zero_y, col_width, btn_height);
    lv_obj_set_pos(btn_zero_y, right_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_zero_y, UITheme::AXIS_Y, LV_PART_MAIN);
    lv_obj_t *lbl_zero_y = lv_label_create(btn_zero_y);
    lv_label_set_text(lbl_zero_y, LV_SYMBOL_GPS " Y");
    lv_obj_set_style_text_font(lbl_zero_y, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_zero_y);
    lv_obj_add_event_cb(btn_zero_y, onZeroYClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Zero Z
    lv_obj_t *btn_zero_z = lv_button_create(tab);
    lv_obj_set_size(btn_zero_z, col_width, btn_height);
    lv_obj_set_pos(btn_zero_z, right_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_zero_z, UITheme::AXIS_Z, LV_PART_MAIN);
    lv_obj_t *lbl_zero_z = lv_label_create(btn_zero_z);
    lv_label_set_text(lbl_zero_z, LV_SYMBOL_GPS " Z");
    lv_obj_set_style_text_font(lbl_zero_z, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_zero_z);
    lv_obj_add_event_cb(btn_zero_z, onZeroZClicked, LV_EVENT_CLICKED, nullptr);
    
    y_pos += btn_height + spacing;
    
    // Zero All
    lv_obj_t *btn_zero_all = lv_button_create(tab);
    lv_obj_set_size(btn_zero_all, col_width, btn_height);
    lv_obj_set_pos(btn_zero_all, right_col_x, y_pos);
    lv_obj_set_style_bg_color(btn_zero_all, UITheme::AXIS_XY, LV_PART_MAIN);
    lv_obj_t *lbl_zero_all = lv_label_create(btn_zero_all);
    lv_label_set_text(lbl_zero_all, LV_SYMBOL_GPS " All");
    lv_obj_set_style_text_font(lbl_zero_all, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_zero_all);
    lv_obj_add_event_cb(btn_zero_all, onZeroAllClicked, LV_EVENT_CLICKED, nullptr);
}

// ========== EVENT HANDLERS ==========

void UITabControlActions::updatePauseButton(int machine_state) {
    if (!btn_pause || !lbl_pause) return;
    
    // Button shows what action it will perform NEXT
    if (machine_state == STATE_HOLD) {
        // Machine is paused - show Resume (green with play symbol)
        lv_obj_set_style_bg_color(btn_pause, UITheme::BTN_PLAY, LV_PART_MAIN);
        lv_label_set_text(lbl_pause, LV_SYMBOL_PLAY " Resume");
    } else {
        // Machine is running/idle - show Pause (dark yellow with pause symbol)
        lv_obj_set_style_bg_color(btn_pause, UITheme::STATE_HOLD, LV_PART_MAIN);
        lv_label_set_text(lbl_pause, LV_SYMBOL_PAUSE " Pause");
    }
}

void UITabControlActions::onPauseResumeClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    // Check actual machine state
    FluidNCStatus status = FluidNCClient::getStatus();
    
    if (status.state == STATE_HOLD) {
        // Machine is paused - send Resume command
        Serial.println("[Actions] Sending Resume command (~)");
        FluidNCClient::sendCommand("~");
    } else {
        // Machine is running/idle - send Pause command
        Serial.println("[Actions] Sending Pause command (!)");
        FluidNCClient::sendCommand("!");
    }
    
    // Note: Button appearance will be updated by the periodic updatePauseButton() call
    // once FluidNC confirms the state change
}

void UITabControlActions::onUnlockClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Unlock command ($X)");
    FluidNCClient::sendCommand("$X\n");
}

void UITabControlActions::onSoftResetClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Soft Reset command (Ctrl-X)");
    // Send Ctrl-X (0x18) as a single character
    char reset_cmd[2] = {0x18, 0x00};
    FluidNCClient::sendCommand(reset_cmd);
    // Button will sync with actual state from FluidNC status
    if (lbl_pause) {
        lv_label_set_text(lbl_pause, "Pause");
    }
}

void UITabControlActions::onHomeXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Home X command ($HX)");
    FluidNCClient::sendCommand("$HX\n");
}

void UITabControlActions::onHomeYClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Home Y command ($HY)");
    FluidNCClient::sendCommand("$HY\n");
}

void UITabControlActions::onHomeZClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Home Z command ($HZ)");
    FluidNCClient::sendCommand("$HZ\n");
}

void UITabControlActions::onHomeAllClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Home All command ($H)");
    FluidNCClient::sendCommand("$H\n");
}

void UITabControlActions::onZeroXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Zero X command (G10 L20 P0 X0)");
    FluidNCClient::sendCommand("G10 L20 P0 X0\n");
}

void UITabControlActions::onZeroYClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Zero Y command (G10 L20 P0 Y0)");
    FluidNCClient::sendCommand("G10 L20 P0 Y0\n");
}

void UITabControlActions::onZeroZClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Zero Z command (G10 L20 P0 Z0)");
    FluidNCClient::sendCommand("G10 L20 P0 Z0\n");
}

void UITabControlActions::onZeroAllClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] Sending Zero All command (G10 L20 P0 X0 Y0 Z0)");
    FluidNCClient::sendCommand("G10 L20 P0 X0 Y0 Z0\n");
}

void UITabControlActions::onQuickStopClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    Serial.println("[Actions] QUICK STOP - Sending Feed Hold + Soft Reset");
    // First send feed hold
    FluidNCClient::sendCommand("!");
    // Small delay then soft reset
    delay(100);
    char reset_cmd[2] = {0x18, 0x00};
    FluidNCClient::sendCommand(reset_cmd);
    // Button will sync with actual state from FluidNC status
    if (lbl_pause) {
        lv_label_set_text(lbl_pause, "Pause");
    }
}
