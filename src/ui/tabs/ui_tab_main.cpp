#include "ui/tabs/ui_tab_main.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/wcs_config.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "network/fluidnc_client.h"
#include "config.h"

// Static member initialization
lv_obj_t *UITabMain::lbl_state = nullptr;
lv_obj_t *UITabMain::lbl_message = nullptr;
lv_obj_t *UITabMain::ind_limit_x = nullptr;
lv_obj_t *UITabMain::lbl_wpos_x = nullptr;
lv_obj_t *UITabMain::lbl_mpos_x = nullptr;
float UITabMain::last_wpos_x = -9999.0f;
float UITabMain::last_mpos_x = -9999.0f;
char UITabMain::last_state[16] = "";
uint32_t UITabMain::last_limit_trigger_x_ms = 0;
lv_obj_t *UITabMain::btn_home_x = nullptr;
lv_obj_t *UITabMain::keyboard = nullptr;
lv_obj_t *UITabMain::active_textarea = nullptr;
char UITabMain::original_value[32] = "";

void UITabMain::create(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    // ========== MACHINE STATE (top left) ==========
    lv_obj_t *state_label = lv_label_create(tab);
    lv_label_set_text(state_label, "STATE");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(state_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(state_label, 10, 10);

    lbl_state = lv_label_create(tab);
    lv_label_set_text(lbl_state, "OFFLINE");
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    lv_obj_set_pos(lbl_state, 10, 30);

    // ========== WORK POSITION (editable X) ==========
    lv_obj_t *wpos_header = lv_label_create(tab);
    lv_label_set_text(wpos_header, "WORK POSITION");
    lv_obj_set_style_text_font(wpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(wpos_header, 10, 110);

    lv_obj_t *wpos_x_label = lv_label_create(tab);
    lv_label_set_text(wpos_x_label, "X");
    lv_obj_set_style_text_font(wpos_x_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_x_label, UITheme::AXIS_X, 0);
    lv_obj_set_pos(wpos_x_label, 10, 145);

    ind_limit_x = lv_obj_create(tab);
    lv_obj_set_size(ind_limit_x, 16, 16);
    lv_obj_set_pos(ind_limit_x, 10, 195);
    lv_obj_set_style_radius(ind_limit_x, 8, 0);
    lv_obj_set_style_bg_color(ind_limit_x, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_border_width(ind_limit_x, 1, 0);
    lv_obj_set_style_border_color(ind_limit_x, UITheme::BORDER_MEDIUM, 0);
    lv_obj_clear_flag(ind_limit_x, LV_OBJ_FLAG_SCROLLABLE);

    lbl_wpos_x = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_wpos_x, "----.---");
    lv_textarea_set_one_line(lbl_wpos_x, true);
    lv_textarea_set_max_length(lbl_wpos_x, 10);
    lv_obj_set_size(lbl_wpos_x, 300, 65);
    lv_obj_set_pos(lbl_wpos_x, 60, 135);
    lv_obj_clear_flag(lbl_wpos_x, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_wpos_x, 4, 0);
    lv_obj_set_style_pad_left(lbl_wpos_x, 10, 0);
    lv_obj_set_style_text_align(lbl_wpos_x, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_bg_color(lbl_wpos_x, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_wpos_x, 0, 0);
    lv_obj_set_style_border_width(lbl_wpos_x, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_wpos_x, UITheme::AXIS_X, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_wpos_x, (void*)"WX");
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

    // ========== MACHINE POSITION (editable X) ==========
    lv_obj_t *mpos_header = lv_label_create(tab);
    lv_label_set_text(mpos_header, "MACHINE POSITION");
    lv_obj_set_style_text_font(mpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(mpos_header, 10, 215);

    lv_obj_t *mpos_x_label = lv_label_create(tab);
    lv_label_set_text(mpos_x_label, "X");
    lv_obj_set_style_text_font(mpos_x_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_x_label, UITheme::POS_MACHINE, 0);
    lv_obj_set_pos(mpos_x_label, 10, 250);

    lbl_mpos_x = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_mpos_x, "----.---");
    lv_textarea_set_one_line(lbl_mpos_x, true);
    lv_textarea_set_max_length(lbl_mpos_x, 10);
    lv_obj_set_size(lbl_mpos_x, 300, 65);
    lv_obj_set_pos(lbl_mpos_x, 60, 240);
    lv_obj_clear_flag(lbl_mpos_x, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_mpos_x, 4, 0);
    lv_obj_set_style_pad_left(lbl_mpos_x, 10, 0);
    lv_obj_set_style_text_align(lbl_mpos_x, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::POS_MACHINE, 0);
    lv_obj_set_style_bg_color(lbl_mpos_x, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_mpos_x, 0, 0);
    lv_obj_set_style_border_width(lbl_mpos_x, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_mpos_x, UITheme::POS_MACHINE, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_mpos_x, (void*)"MX");
    lv_obj_add_event_cb(lbl_mpos_x, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_mpos_x, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

    // ========== ACTION BUTTONS (right column) ==========
    const int btn_x = 440;
    const int btn_width = 220;
    const int btn_height = 55;
    const int btn_spacing = 12;
    int btn_y = 110;

    btn_home_x = lv_button_create(tab);
    lv_obj_set_size(btn_home_x, btn_width, btn_height);
    lv_obj_set_pos(btn_home_x, btn_x, btn_y);
    lv_obj_set_style_bg_color(btn_home_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_home_x = lv_label_create(btn_home_x);
    lv_label_set_text(lbl_home_x, LV_SYMBOL_HOME " Home X");
    lv_obj_set_style_text_font(lbl_home_x, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_home_x);
    lv_obj_add_event_cb(btn_home_x, onHomeXClicked, LV_EVENT_CLICKED, nullptr);
    btn_y += btn_height + btn_spacing;

    lv_obj_t *btn_zero_x = lv_button_create(tab);
    lv_obj_set_size(btn_zero_x, btn_width, btn_height);
    lv_obj_set_pos(btn_zero_x, btn_x, btn_y);
    lv_obj_set_style_bg_color(btn_zero_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_zero_x = lv_label_create(btn_zero_x);
    lv_label_set_text(lbl_zero_x, LV_SYMBOL_GPS " Zero X");
    lv_obj_set_style_text_font(lbl_zero_x, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_zero_x);
    lv_obj_add_event_cb(btn_zero_x, onZeroXClicked, LV_EVENT_CLICKED, nullptr);
    btn_y += btn_height + btn_spacing;

    lv_obj_t *btn_unlock = lv_button_create(tab);
    lv_obj_set_size(btn_unlock, btn_width, btn_height);
    lv_obj_set_pos(btn_unlock, btn_x, btn_y);
    lv_obj_set_style_bg_color(btn_unlock, UITheme::ACCENT_PRIMARY, LV_PART_MAIN);
    lv_obj_t *lbl_unlock = lv_label_create(btn_unlock);
    lv_label_set_text(lbl_unlock, LV_SYMBOL_OK " Unlock");
    lv_obj_set_style_text_font(lbl_unlock, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_unlock);
    lv_obj_add_event_cb(btn_unlock, onUnlockClicked, LV_EVENT_CLICKED, nullptr);
    btn_y += btn_height + btn_spacing;

    lv_obj_t *btn_estop = lv_button_create(tab);
    lv_obj_set_size(btn_estop, btn_width, btn_height);
    lv_obj_set_pos(btn_estop, btn_x, btn_y);
    lv_obj_set_style_bg_color(btn_estop, UITheme::BTN_ESTOP, LV_PART_MAIN);
    lv_obj_t *lbl_estop = lv_label_create(btn_estop);
    lv_label_set_text(lbl_estop, LV_SYMBOL_STOP " STOP");
    lv_obj_set_style_text_font(lbl_estop, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_estop);
    lv_obj_add_event_cb(btn_estop, onQuickStopClicked, LV_EVENT_CLICKED, nullptr);

    // ========== MESSAGE ==========
    lv_obj_t *message_header = lv_label_create(tab);
    lv_label_set_text(message_header, "MESSAGE");
    lv_obj_set_style_text_font(message_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(message_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(message_header, 10, 320);

    lbl_message = lv_label_create(tab);
    lv_label_set_text(lbl_message, "No messages.");
    lv_obj_set_size(lbl_message, 650, 30);
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_message, 10, 340);
}

// ========== UPDATE METHODS ==========

void UITabMain::updateMessage(const char *message) {
    if (!lbl_message) return;
    lv_label_set_text(lbl_message, message);

    if (strncmp(message, "ERROR", 5) == 0 || strstr(message, "error") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::STATE_ALARM, 0);
    } else if (strncmp(message, "WARN", 4) == 0 || strstr(message, "warning") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_WARNING, 0);
    } else if (strncmp(message, "OK", 2) == 0 || strstr(message, "success") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_INFO, 0);
    }
}

void UITabMain::updateState(const char *state) {
    if (!lbl_state) return;
    if (strcmp(state, last_state) == 0) return;

    lv_label_set_text(lbl_state, state);
    strncpy(last_state, state, sizeof(last_state) - 1);
    last_state[sizeof(last_state) - 1] = '\0';

    if (strcmp(state, "IDLE") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_IDLE, 0);
    } else if (strcmp(state, "RUN") == 0 || strcmp(state, "JOG") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_RUN, 0);
    } else if (strcmp(state, "ALARM") == 0 || strcmp(state, "OFFLINE") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    } else {
        lv_obj_set_style_text_color(lbl_state, UITheme::UI_WARNING, 0);
    }
}

void UITabMain::updateWorkPosition(float x, float y, float z, float a) {
    if (!lbl_wpos_x || x == last_wpos_x) return;

    char buf[20];
    if (x <= -9999.0f) {
        lv_textarea_set_text(lbl_wpos_x, "----.---");
    } else {
        snprintf(buf, sizeof(buf), "%.3f", x);
        lv_textarea_set_text(lbl_wpos_x, buf);
    }
    last_wpos_x = x;
}

void UITabMain::updateMachinePosition(float x, float y, float z, float a) {
    if (!lbl_mpos_x || x == last_mpos_x) return;

    char buf[20];
    if (x <= -9999.0f) {
        lv_textarea_set_text(lbl_mpos_x, "----.---");
    } else {
        snprintf(buf, sizeof(buf), "%.3f", x);
        lv_textarea_set_text(lbl_mpos_x, buf);
    }
    last_mpos_x = x;
}

void UITabMain::updateLimitSwitches(bool x, bool y, bool z, bool a) {
    if (!ind_limit_x) return;

    uint32_t now = millis();
    if (x) last_limit_trigger_x_ms = now;
    bool vis_x = x || (now - last_limit_trigger_x_ms < LIMIT_SWITCH_HOLD_MS);

    lv_obj_set_style_bg_color(ind_limit_x, vis_x ? UITheme::STATE_ALARM : UITheme::BG_BUTTON, 0);
}

// ========== POSITION EDITING (Go To), same pattern as UITabStatus ==========

void UITabMain::position_field_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        const FluidNCStatus& status = FluidNCClient::getStatus();
        if (status.state != STATE_IDLE) {
            lv_obj_clear_state(textarea, LV_STATE_FOCUSED);
            return;
        }

        active_textarea = textarea;
        strncpy(original_value, lv_textarea_get_text(textarea), sizeof(original_value) - 1);
        original_value[sizeof(original_value) - 1] = '\0';

        if (!keyboard) {
            keyboard = lv_obj_create(lv_scr_act());
            lv_obj_set_size(keyboard, 325, 340);
            lv_obj_set_pos(keyboard, 465, 70);
            lv_obj_set_style_bg_color(keyboard, UITheme::BG_DARK, 0);
            lv_obj_set_style_border_color(keyboard, UITheme::BORDER_MEDIUM, 0);
            lv_obj_set_style_border_width(keyboard, 2, 0);
            lv_obj_set_style_pad_all(keyboard, 10, 0);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            const char* num_labels[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", ".", "0", "-"};
            int btn_width = 97;
            int btn_height = 48;
            int gap = 5;

            for (int i = 0; i < 12; i++) {
                int row = i / 3;
                int col = i % 3;

                lv_obj_t *btn = lv_button_create(keyboard);
                lv_obj_set_size(btn, btn_width, btn_height);
                lv_obj_set_pos(btn, col * (btn_width + gap), row * (btn_height + gap));
                lv_obj_set_style_bg_color(btn, UITheme::BG_MEDIUM, 0);
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

                lv_obj_t *label = lv_label_create(btn);
                lv_label_set_text(label, num_labels[i]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_center(label);

                lv_obj_set_user_data(btn, (void*)num_labels[i]);
                lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                        lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
                        const char* ch = (const char*)lv_obj_get_user_data(target);
                        lv_textarea_add_text(active_textarea, ch);
                    }
                }, LV_EVENT_CLICKED, NULL);
            }

            lv_obj_t *btn_clear = lv_button_create(keyboard);
            lv_obj_set_size(btn_clear, (btn_width * 2) + gap, btn_height);
            lv_obj_set_pos(btn_clear, 0, (btn_height * 4) + (gap * 4));
            lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xFF6600), 0);
            lv_obj_clear_flag(btn_clear, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_clear = lv_label_create(btn_clear);
            lv_label_set_text(lbl_clear, "CLEAR");
            lv_obj_set_style_text_font(lbl_clear, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_clear);

            lv_obj_add_event_cb(btn_clear, [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                    lv_textarea_set_text(active_textarea, "");
                }
            }, LV_EVENT_CLICKED, NULL);

            lv_obj_t *btn_back = lv_button_create(keyboard);
            lv_obj_set_size(btn_back, btn_width, btn_height);
            lv_obj_set_pos(btn_back, (btn_width * 2) + (gap * 2), (btn_height * 4) + (gap * 4));
            lv_obj_set_style_bg_color(btn_back, UITheme::BG_MEDIUM, 0);
            lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_back = lv_label_create(btn_back);
            lv_label_set_text(lbl_back, LV_SYMBOL_BACKSPACE);
            lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
            lv_obj_center(lbl_back);

            lv_obj_add_event_cb(btn_back, [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                    lv_textarea_delete_char(active_textarea);
                }
            }, LV_EVENT_CLICKED, NULL);

            lv_obj_t *btn_ok = lv_button_create(keyboard);
            lv_obj_set_size(btn_ok, (((btn_width * 3) + (gap * 2)) / 2) - 3, btn_height);
            lv_obj_set_pos(btn_ok, 0, (btn_height * 5) + (gap * 5));
            lv_obj_set_style_bg_color(btn_ok, UITheme::BTN_PLAY, 0);
            lv_obj_clear_flag(btn_ok, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_ok = lv_label_create(btn_ok);
            lv_label_set_text(lbl_ok, "OK");
            lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_ok);

            lv_obj_add_event_cb(btn_ok, keyboard_event_handler, LV_EVENT_CLICKED, NULL);
            lv_obj_set_user_data(btn_ok, (void*)"ok");

            lv_obj_t *btn_cancel = lv_button_create(keyboard);
            lv_obj_set_size(btn_cancel, (((btn_width * 3) + (gap * 2)) / 2) - 3, btn_height);
            lv_obj_set_pos(btn_cancel, (((btn_width * 3) + (gap * 2)) / 2) + 3, (btn_height * 5) + (gap * 5));
            lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x555555), 0);
            lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
            lv_label_set_text(lbl_cancel, "CANCEL");
            lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_cancel);

            lv_obj_add_event_cb(btn_cancel, keyboard_event_handler, LV_EVENT_CLICKED, NULL);
            lv_obj_set_user_data(btn_cancel, (void*)"cancel");
        }

        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    if (code == LV_EVENT_DEFOCUSED) {
        if (keyboard && !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN)) {
            bool switching_fields = (lv_obj_has_state(lbl_wpos_x, LV_STATE_FOCUSED) ||
                                     lv_obj_has_state(lbl_mpos_x, LV_STATE_FOCUSED));

            if (!switching_fields) {
                if (active_textarea) {
                    lv_textarea_set_text(active_textarea, original_value);
                }
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
                active_textarea = nullptr;
            }
        }
    }
}

void UITabMain::showValidationError(const char* message) {
    lv_obj_t *dialog = lv_obj_create(lv_screen_active());
    lv_obj_set_size(dialog, 400, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);

    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "VALIDATION ERROR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *msg = lv_label_create(dialog);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn = lv_btn_create(dialog);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, UITheme::BTN_CONNECT, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        lv_obj_t *dialog = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(dialog);
    }, LV_EVENT_CLICKED, dialog);
}

void UITabMain::keyboard_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED) return;

    const char* action = (const char*)lv_obj_get_user_data(btn);

    if (strcmp(action, "ok") == 0) {
        if (active_textarea) {
            const char *axis_id = (const char*)lv_obj_get_user_data(active_textarea);
            const char *value_str = lv_textarea_get_text(active_textarea);

            char *endptr;
            float value = strtof(value_str, &endptr);
            if (endptr == value_str || *endptr != '\0') {
                showValidationError("Invalid number entered");
                return;
            }

            char command[64];
            int feed_rate = UITabSettingsJog::getDefaultXYFeed();

            if (axis_id[0] == 'W') {
                snprintf(command, sizeof(command), "$J=X%.3f F%d\n", value, feed_rate);
                Serial.printf("[Main] Jogging to work X position: %s", command);
            } else if (axis_id[0] == 'M') {
                snprintf(command, sizeof(command), "$J=G53 X%.3f F%d\n", value, feed_rate);
                Serial.printf("[Main] Jogging to machine X position: %s", command);
            }

            FluidNCClient::sendCommand(command);
        }

        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        active_textarea = nullptr;

    } else if (strcmp(action, "cancel") == 0) {
        if (active_textarea) {
            lv_textarea_set_text(active_textarea, original_value);
            FluidNCClient::sendCommand("?");
        }

        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        active_textarea = nullptr;
    }
}

// ========== ACTION BUTTON HANDLERS ==========

void UITabMain::onHomeXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Home X command ($HX)");
    FluidNCClient::sendCommand("$HX\n");
}

void UITabMain::onZeroXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }

    if (WCSConfig::isCurrentWCSLocked()) {
        const FluidNCStatus& status = FluidNCClient::getStatus();
        char wcs_name[32];
        WCSConfig::getCurrentWCSName(wcs_name, sizeof(wcs_name));

        Serial.printf("[Main] WCS %s is locked, showing confirmation\n", status.modal_wcs);
        UICommon::showWCSLockDialog(status.modal_wcs, wcs_name, [](lv_event_t *e) {
            Serial.println("[Main] Confirmed Zero X on locked WCS");
            FluidNCClient::sendCommand("G10 L20 P0 X0\n");
        });
        return;
    }

    Serial.println("[Main] Sending Zero X command (G10 L20 P0 X0)");
    FluidNCClient::sendCommand("G10 L20 P0 X0\n");
}

void UITabMain::onUnlockClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Unlock command ($X)");
    FluidNCClient::sendCommand("$X\n");
}

void UITabMain::onQuickStopClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Quick Stop (feed hold !)");
    FluidNCClient::sendCommand("!");
}
