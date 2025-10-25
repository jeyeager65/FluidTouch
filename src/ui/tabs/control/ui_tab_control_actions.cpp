#include "ui/tabs/control/ui_tab_control_actions.h"
#include "ui/ui_theme.h"
#include "fluidnc_client.h"

// Static member initialization
bool UITabControlActions::is_paused = false;
lv_obj_t *UITabControlActions::btn_pause = nullptr;
lv_obj_t *UITabControlActions::lbl_pause = nullptr;

void UITabControlActions::create(lv_obj_t *tab) {
    const int col_width = 180;
    const int col_spacing = 20;
    const int left_col_x = 10;
    const int middle_col_x = left_col_x + col_width + col_spacing;
    const int right_col_x = middle_col_x + col_width + col_spacing;
    
    // ========== LEFT COLUMN: Control Buttons ==========
    lv_obj_t *control_label = lv_label_create(tab);
    lv_label_set_text(control_label, "Control:");
    lv_obj_set_style_text_font(control_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(control_label, left_col_x, 10);
    
    btn_pause = lv_button_create(tab);
    lv_obj_set_size(btn_pause, col_width, 60);
    lv_obj_set_pos(btn_pause, left_col_x, 45);
    lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, "Pause");
    lv_obj_set_style_text_font(lbl_pause, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_pause);
    lv_obj_add_event_cb(btn_pause, onPauseResumeClicked, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *btn_unlock = lv_button_create(tab);
    lv_obj_set_size(btn_unlock, col_width, 60);
    lv_obj_set_pos(btn_unlock, left_col_x, 115);
    lv_obj_t *lbl_unlock = lv_label_create(btn_unlock);
    lv_label_set_text(lbl_unlock, "Unlock");
    lv_obj_set_style_text_font(lbl_unlock, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_unlock);
    lv_obj_add_event_cb(btn_unlock, onUnlockClicked, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, col_width, 60);
    lv_obj_set_pos(btn_reset, left_col_x, 185);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Soft Reset");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, onSoftResetClicked, LV_EVENT_CLICKED, nullptr);
    
    // ========== MIDDLE COLUMN: Home Axis ==========
    lv_obj_t *home_label = lv_label_create(tab);
    lv_label_set_text(home_label, "Home Axis:");
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(home_label, middle_col_x, 10);
    
    // Home X
    lv_obj_t *btn_home_x = lv_button_create(tab);
    lv_obj_set_size(btn_home_x, col_width, 50);
    lv_obj_set_pos(btn_home_x, middle_col_x, 45);
    lv_obj_set_style_bg_color(btn_home_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_home_x = lv_label_create(btn_home_x);
    lv_label_set_text(lbl_home_x, "Home X");
    lv_obj_set_style_text_font(lbl_home_x, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_home_x);
    lv_obj_add_event_cb(btn_home_x, onHomeXClicked, LV_EVENT_CLICKED, nullptr);
    
    // Home Y
    lv_obj_t *btn_home_y = lv_button_create(tab);
    lv_obj_set_size(btn_home_y, col_width, 50);
    lv_obj_set_pos(btn_home_y, middle_col_x, 105);
    lv_obj_set_style_bg_color(btn_home_y, UITheme::AXIS_Y, LV_PART_MAIN);
    lv_obj_t *lbl_home_y = lv_label_create(btn_home_y);
    lv_label_set_text(lbl_home_y, "Home Y");
    lv_obj_set_style_text_font(lbl_home_y, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_home_y);
    lv_obj_add_event_cb(btn_home_y, onHomeYClicked, LV_EVENT_CLICKED, nullptr);
    
    // Home Z
    lv_obj_t *btn_home_z = lv_button_create(tab);
    lv_obj_set_size(btn_home_z, col_width, 50);
    lv_obj_set_pos(btn_home_z, middle_col_x, 165);
    lv_obj_set_style_bg_color(btn_home_z, UITheme::AXIS_Z, LV_PART_MAIN);
    lv_obj_t *lbl_home_z = lv_label_create(btn_home_z);
    lv_label_set_text(lbl_home_z, "Home Z");
    lv_obj_set_style_text_font(lbl_home_z, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_home_z);
    lv_obj_add_event_cb(btn_home_z, onHomeZClicked, LV_EVENT_CLICKED, nullptr);
    
    // Home All
    lv_obj_t *btn_home_all = lv_button_create(tab);
    lv_obj_set_size(btn_home_all, col_width, 50);
    lv_obj_set_pos(btn_home_all, middle_col_x, 225);
    lv_obj_set_style_bg_color(btn_home_all, UITheme::AXIS_XY, LV_PART_MAIN);
    lv_obj_t *lbl_home_all = lv_label_create(btn_home_all);
    lv_label_set_text(lbl_home_all, "Home All");
    lv_obj_set_style_text_font(lbl_home_all, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_home_all);
    lv_obj_add_event_cb(btn_home_all, onHomeAllClicked, LV_EVENT_CLICKED, nullptr);
    
    // ========== RIGHT COLUMN: Zero Axis ==========
    lv_obj_t *zero_label = lv_label_create(tab);
    lv_label_set_text(zero_label, "Zero Axis:");
    lv_obj_set_style_text_font(zero_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(zero_label, right_col_x, 10);
    
    // Zero X
    lv_obj_t *btn_zero_x = lv_button_create(tab);
    lv_obj_set_size(btn_zero_x, col_width, 50);
    lv_obj_set_pos(btn_zero_x, right_col_x, 45);
    lv_obj_set_style_bg_color(btn_zero_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_zero_x = lv_label_create(btn_zero_x);
    lv_label_set_text(lbl_zero_x, "Zero X");
    lv_obj_set_style_text_font(lbl_zero_x, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_zero_x);
    lv_obj_add_event_cb(btn_zero_x, onZeroXClicked, LV_EVENT_CLICKED, nullptr);
    
    // Zero Y
    lv_obj_t *btn_zero_y = lv_button_create(tab);
    lv_obj_set_size(btn_zero_y, col_width, 50);
    lv_obj_set_pos(btn_zero_y, right_col_x, 105);
    lv_obj_set_style_bg_color(btn_zero_y, UITheme::AXIS_Y, LV_PART_MAIN);
    lv_obj_t *lbl_zero_y = lv_label_create(btn_zero_y);
    lv_label_set_text(lbl_zero_y, "Zero Y");
    lv_obj_set_style_text_font(lbl_zero_y, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_zero_y);
    lv_obj_add_event_cb(btn_zero_y, onZeroYClicked, LV_EVENT_CLICKED, nullptr);
    
    // Zero Z
    lv_obj_t *btn_zero_z = lv_button_create(tab);
    lv_obj_set_size(btn_zero_z, col_width, 50);
    lv_obj_set_pos(btn_zero_z, right_col_x, 165);
    lv_obj_set_style_bg_color(btn_zero_z, UITheme::AXIS_Z, LV_PART_MAIN);
    lv_obj_t *lbl_zero_z = lv_label_create(btn_zero_z);
    lv_label_set_text(lbl_zero_z, "Zero Z");
    lv_obj_set_style_text_font(lbl_zero_z, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_zero_z);
    lv_obj_add_event_cb(btn_zero_z, onZeroZClicked, LV_EVENT_CLICKED, nullptr);
    
    // Zero All
    lv_obj_t *btn_zero_all = lv_button_create(tab);
    lv_obj_set_size(btn_zero_all, col_width, 50);
    lv_obj_set_pos(btn_zero_all, right_col_x, 225);
    lv_obj_set_style_bg_color(btn_zero_all, UITheme::AXIS_XY, LV_PART_MAIN);
    lv_obj_t *lbl_zero_all = lv_label_create(btn_zero_all);
    lv_label_set_text(lbl_zero_all, "Zero All");
    lv_obj_set_style_text_font(lbl_zero_all, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_zero_all);
    lv_obj_add_event_cb(btn_zero_all, onZeroAllClicked, LV_EVENT_CLICKED, nullptr);

    // ========== QUICK STOP: Full Width at Bottom ==========
    lv_obj_t *btn_estop = lv_button_create(tab);
    lv_obj_set_size(btn_estop, col_width * 3 + col_spacing * 2, 60);
    lv_obj_set_pos(btn_estop, left_col_x, 290);
    lv_obj_set_style_bg_color(btn_estop, UITheme::BTN_ESTOP, LV_PART_MAIN);
    lv_obj_t *lbl_estop = lv_label_create(btn_estop);
    lv_label_set_text(lbl_estop, "QUICK STOP");
    lv_obj_set_style_text_font(lbl_estop, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_estop);
    lv_obj_add_event_cb(btn_estop, onQuickStopClicked, LV_EVENT_CLICKED, nullptr);
}

// ========== EVENT HANDLERS ==========

void UITabControlActions::onPauseResumeClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Actions] Not connected to FluidNC");
        return;
    }
    
    if (is_paused) {
        // Resume (cycle start)
        Serial.println("[Actions] Sending Resume command (~)");
        FluidNCClient::sendCommand("~");
        is_paused = false;
        if (lbl_pause) {
            lv_label_set_text(lbl_pause, "Pause");
        }
    } else {
        // Pause (feed hold)
        Serial.println("[Actions] Sending Pause command (!)");
        FluidNCClient::sendCommand("!");
        is_paused = true;
        if (lbl_pause) {
            lv_label_set_text(lbl_pause, "Resume");
        }
    }
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
    is_paused = false;  // Reset pause state after soft reset
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
    is_paused = false;  // Reset pause state
    if (lbl_pause) {
        lv_label_set_text(lbl_pause, "Pause");
    }
}
