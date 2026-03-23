#include "ui/tabs/ui_tab_status.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/machine_config.h"
#include "ui/wcs_config.h"
#include "ui/fonts/fontawesome_icons_20.h"
#include "network/fluidnc_client.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include <stdio.h>

// Static member initialization
lv_obj_t *UITabStatus::lbl_message = nullptr;
lv_obj_t *UITabStatus::lbl_state = nullptr;
lv_obj_t *UITabStatus::btn_pause = nullptr;
lv_obj_t *UITabStatus::lbl_pause = nullptr;
lv_obj_t *UITabStatus::btn_stop = nullptr;
lv_obj_t *UITabStatus::btn_cancel_jog = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_x = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_y = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_z = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_a = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_x = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_y = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_z = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_a = nullptr;
lv_obj_t *UITabStatus::keyboard = nullptr;
lv_obj_t *UITabStatus::active_textarea = nullptr;
char UITabStatus::original_value[32] = "";
lv_obj_t *UITabStatus::lbl_feed_value = nullptr;
lv_obj_t *UITabStatus::lbl_feed_override = nullptr;
lv_obj_t *UITabStatus::lbl_feed_units = nullptr;
lv_obj_t *UITabStatus::lbl_rapid_override = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_value = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_override = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_units = nullptr;
lv_obj_t *UITabStatus::btn_modal_wcs = nullptr;
lv_obj_t *UITabStatus::lbl_modal_wcs_value = nullptr;
lv_obj_t *UITabStatus::lbl_modal_plane = nullptr;
lv_obj_t *UITabStatus::lbl_modal_dist = nullptr;
lv_obj_t *UITabStatus::lbl_modal_units = nullptr;
lv_obj_t *UITabStatus::lbl_modal_motion = nullptr;
lv_obj_t *UITabStatus::lbl_modal_feedrate = nullptr;
lv_obj_t *UITabStatus::lbl_modal_spindle = nullptr;
lv_obj_t *UITabStatus::lbl_modal_coolant = nullptr;
lv_obj_t *UITabStatus::lbl_modal_tool = nullptr;

// Cached values initialization (for delta checking)
float UITabStatus::last_wpos_x = -9999.0f;
float UITabStatus::last_wpos_y = -9999.0f;
float UITabStatus::last_wpos_z = -9999.0f;
float UITabStatus::last_wpos_a = -9999.0f;
float UITabStatus::last_mpos_x = -9999.0f;
float UITabStatus::last_mpos_y = -9999.0f;
float UITabStatus::last_mpos_z = -9999.0f;
float UITabStatus::last_mpos_a = -9999.0f;
float UITabStatus::last_feed_rate = -1.0f;
float UITabStatus::last_feed_override = -1.0f;
float UITabStatus::last_rapid_override = -1.0f;
float UITabStatus::last_spindle_speed = -1.0f;
float UITabStatus::last_spindle_override = -1.0f;
char UITabStatus::last_state[16] = "";
char UITabStatus::last_modal_wcs[8] = "";
char UITabStatus::last_modal_plane[8] = "";
char UITabStatus::last_modal_dist[8] = "";
char UITabStatus::last_modal_units[8] = "";
char UITabStatus::last_modal_motion[8] = "";
char UITabStatus::last_modal_feedrate[8] = "";
char UITabStatus::last_modal_spindle[8] = "";
char UITabStatus::last_modal_coolant[8] = "";
char UITabStatus::last_modal_tool[8] = "";

// WCS popup static members
lv_obj_t *UITabStatus::wcs_popup = nullptr;
lv_obj_t *UITabStatus::wcs_popup_content = nullptr;
lv_obj_t *UITabStatus::wcs_btn_set = nullptr;
lv_obj_t *UITabStatus::wcs_btn_cancel = nullptr;
lv_obj_t *UITabStatus::wcs_buttons[6] = {nullptr};  // Store button references
int UITabStatus::selected_wcs_index = -1;  // -1 = none selected
int UITabStatus::current_wcs_index = -1;  // -1 = none is current
char UITabStatus::wcs_offsets[10][64] = {{0}};  // G54-G59, G28, G30, G92, TLO

void UITabStatus::create(lv_obj_t *tab) {
    // Set 5px margins by using padding
    lv_obj_set_style_pad_all(tab, 10, 0);
    
    // Industrial Electronics Style Status Display
    lv_obj_set_style_bg_color(tab, UITheme::BG_BLACK, LV_PART_MAIN);
    
    // MACHINE STATE - Industrial header
    lv_obj_t *state_label = lv_label_create(tab);
    lv_label_set_text(state_label, "STATE");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(state_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(state_label, 0, 0);

    lbl_state = lv_label_create(tab);
    lv_label_set_text(lbl_state, "OFFLINE");
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    lv_obj_set_pos(lbl_state, 0, 20);

    // CONTROL BUTTONS - Appears in place of job progress (shown during job run or jog)
    // Pause/Resume button (hidden by default, shown when running job)
    btn_pause = lv_button_create(tab);
    lv_obj_set_size(btn_pause, 180, 50);
    lv_obj_set_pos(btn_pause, 230, 0);
    lv_obj_set_style_bg_color(btn_pause, UITheme::STATE_HOLD, LV_PART_MAIN);
    lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, LV_SYMBOL_PAUSE " Pause");
    lv_obj_set_style_text_font(lbl_pause, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_pause);
    lv_obj_add_event_cb(btn_pause, onPauseResumeClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_pause, LV_OBJ_FLAG_HIDDEN);
    
    // Stop button (hidden by default, shown when running job)
    btn_stop = lv_button_create(tab);
    lv_obj_set_size(btn_stop, 180, 50);
    lv_obj_set_pos(btn_stop, 420, 0);
    lv_obj_set_style_bg_color(btn_stop, UITheme::BTN_ESTOP, LV_PART_MAIN);
    lv_obj_t *lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, LV_SYMBOL_STOP " STOP");
    lv_obj_set_style_text_font(lbl_stop, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_stop);
    lv_obj_add_event_cb(btn_stop, onStopClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
    
    // Cancel Jog button (hidden by default, shown only when jogging)
    btn_cancel_jog = lv_button_create(tab);
    lv_obj_set_size(btn_cancel_jog, 370, 50);
    lv_obj_set_pos(btn_cancel_jog, 230, 0);
    lv_obj_set_style_bg_color(btn_cancel_jog, UITheme::UI_WARNING, LV_PART_MAIN);
    lv_obj_t *lbl_cancel_jog = lv_label_create(btn_cancel_jog);
    lv_label_set_text(lbl_cancel_jog, LV_SYMBOL_STOP " Cancel Jog");
    lv_obj_set_style_text_font(lbl_cancel_jog, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_cancel_jog);
    lv_obj_add_event_cb(btn_cancel_jog, onCancelJogClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_cancel_jog, LV_OBJ_FLAG_HIDDEN);

    // Separator line
    lv_obj_t *line1 = lv_obj_create(tab);
    lv_obj_set_size(line1, 780, 2);
    lv_obj_set_pos(line1, 0, 60);
    lv_obj_set_style_bg_color(line1, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_border_width(line1, 0, 0);

    // WCS BUTTON - Top right corner
    lv_obj_t *wcs_header = lv_label_create(tab);
    lv_label_set_text(wcs_header, "WCS");
    lv_obj_set_style_text_font(wcs_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wcs_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(wcs_header, 615, 0);
    
    btn_modal_wcs = lv_button_create(tab);
    lv_obj_set_size(btn_modal_wcs, 115, 50);
    lv_obj_set_pos(btn_modal_wcs, 665, 0);
    lv_obj_set_style_bg_color(btn_modal_wcs, UITheme::BG_BLACK, LV_PART_MAIN);
    
    lbl_modal_wcs_value = lv_label_create(btn_modal_wcs);
    lv_label_set_text(lbl_modal_wcs_value, "---");
    lv_obj_set_style_text_font(lbl_modal_wcs_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_modal_wcs_value, UITheme::POS_MODAL, 0);
    lv_obj_center(lbl_modal_wcs_value);
    
    lv_obj_add_event_cb(btn_modal_wcs, onWCSButtonClicked, LV_EVENT_CLICKED, nullptr);

    // WORK POSITION - Left column
    lv_obj_t *wpos_header = lv_label_create(tab);
    lv_label_set_text(wpos_header, "WORK POSITION");
    lv_obj_set_style_text_font(wpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(wpos_header, 0, 70);

    // Work Position - Editable text areas with axis labels
    lv_obj_t *wpos_x_label = lv_label_create(tab);
    lv_label_set_text(wpos_x_label, "X");
    lv_obj_set_style_text_font(wpos_x_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_x_label, UITheme::AXIS_X, 0);
    lv_obj_set_pos(wpos_x_label, 0, 95);

    lbl_wpos_x = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_wpos_x, "----.---");
    lv_textarea_set_one_line(lbl_wpos_x, true);
    lv_textarea_set_max_length(lbl_wpos_x, 10);
    lv_obj_set_size(lbl_wpos_x, 180, 40);
    lv_obj_set_pos(lbl_wpos_x, 35, 95);
    lv_obj_clear_flag(lbl_wpos_x, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_wpos_x, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_wpos_x, 2, 0);
    lv_obj_set_style_pad_left(lbl_wpos_x, 5, 0);
    lv_obj_set_style_text_align(lbl_wpos_x, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_bg_color(lbl_wpos_x, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_wpos_x, 0, 0);
    lv_obj_set_style_border_width(lbl_wpos_x, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_wpos_x, UITheme::AXIS_X, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_wpos_x, (void*)"WX");
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);
    
    lv_obj_t *wpos_y_label = lv_label_create(tab);
    lv_label_set_text(wpos_y_label, "Y");
    lv_obj_set_style_text_font(wpos_y_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_y_label, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(wpos_y_label, 0, 140);

    lbl_wpos_y = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_wpos_y, "----.---");
    lv_textarea_set_one_line(lbl_wpos_y, true);
    lv_textarea_set_max_length(lbl_wpos_y, 10);
    lv_obj_set_size(lbl_wpos_y, 180, 40);
    lv_obj_set_pos(lbl_wpos_y, 35, 140);
    lv_obj_clear_flag(lbl_wpos_y, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_wpos_y, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_wpos_y, 2, 0);
    lv_obj_set_style_pad_left(lbl_wpos_y, 5, 0);
    lv_obj_set_style_text_align(lbl_wpos_y, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_style_bg_color(lbl_wpos_y, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_wpos_y, 0, 0);
    lv_obj_set_style_border_width(lbl_wpos_y, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_wpos_y, UITheme::AXIS_Y, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_wpos_y, (void*)"WY");
    lv_obj_add_event_cb(lbl_wpos_y, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_wpos_y, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);
    
    lv_obj_t *wpos_z_label = lv_label_create(tab);
    lv_label_set_text(wpos_z_label, "Z");
    lv_obj_set_style_text_font(wpos_z_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_z_label, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(wpos_z_label, 0, 185);

    lbl_wpos_z = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_wpos_z, "----.---");
    lv_textarea_set_one_line(lbl_wpos_z, true);
    lv_textarea_set_max_length(lbl_wpos_z, 10);
    lv_obj_set_size(lbl_wpos_z, 180, 40);
    lv_obj_set_pos(lbl_wpos_z, 35, 185);
    lv_obj_clear_flag(lbl_wpos_z, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_wpos_z, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_wpos_z, 2, 0);
    lv_obj_set_style_pad_left(lbl_wpos_z, 5, 0);
    lv_obj_set_style_text_align(lbl_wpos_z, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_style_bg_color(lbl_wpos_z, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_wpos_z, 0, 0);
    lv_obj_set_style_border_width(lbl_wpos_z, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_wpos_z, UITheme::AXIS_Z, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_wpos_z, (void*)"WZ");
    lv_obj_add_event_cb(lbl_wpos_z, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_wpos_z, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

    // MACHINE POSITION - Center column (moved right for wider values)
    lv_obj_t *mpos_header = lv_label_create(tab);
    lv_label_set_text(mpos_header, "MACHINE POSITION");
    lv_obj_set_style_text_font(mpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(mpos_header, 225, 70);

    // Machine Position - Editable text areas with axis labels
    lv_obj_t *mpos_x_label = lv_label_create(tab);
    lv_label_set_text(mpos_x_label, "X");
    lv_obj_set_style_text_font(mpos_x_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_x_label, UITheme::AXIS_X, 0);
    lv_obj_set_pos(mpos_x_label, 225, 95);

    lbl_mpos_x = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_mpos_x, "----.---");
    lv_textarea_set_one_line(lbl_mpos_x, true);
    lv_textarea_set_max_length(lbl_mpos_x, 10);
    lv_obj_set_size(lbl_mpos_x, 180, 40);
    lv_obj_set_pos(lbl_mpos_x, 260, 95);
    lv_obj_clear_flag(lbl_mpos_x, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_mpos_x, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_mpos_x, 2, 0);
    lv_obj_set_style_pad_left(lbl_mpos_x, 5, 0);
    lv_obj_set_style_text_align(lbl_mpos_x, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_bg_color(lbl_mpos_x, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_mpos_x, 0, 0);
    lv_obj_set_style_border_width(lbl_mpos_x, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_mpos_x, UITheme::AXIS_X, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_mpos_x, (void*)"MX");
    lv_obj_add_event_cb(lbl_mpos_x, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_mpos_x, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);
    
    lv_obj_t *mpos_y_label = lv_label_create(tab);
    lv_label_set_text(mpos_y_label, "Y");
    lv_obj_set_style_text_font(mpos_y_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_y_label, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(mpos_y_label, 225, 140);

    lbl_mpos_y = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_mpos_y, "----.---");
    lv_textarea_set_one_line(lbl_mpos_y, true);
    lv_textarea_set_max_length(lbl_mpos_y, 10);
    lv_obj_set_size(lbl_mpos_y, 180, 40);
    lv_obj_set_pos(lbl_mpos_y, 260, 140);
    lv_obj_clear_flag(lbl_mpos_y, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_mpos_y, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_mpos_y, 2, 0);
    lv_obj_set_style_pad_left(lbl_mpos_y, 5, 0);
    lv_obj_set_style_text_align(lbl_mpos_y, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_style_bg_color(lbl_mpos_y, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_mpos_y, 0, 0);
    lv_obj_set_style_border_width(lbl_mpos_y, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_mpos_y, UITheme::AXIS_Y, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_mpos_y, (void*)"MY");
    lv_obj_add_event_cb(lbl_mpos_y, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_mpos_y, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);
    
    lv_obj_t *mpos_z_label = lv_label_create(tab);
    lv_label_set_text(mpos_z_label, "Z");
    lv_obj_set_style_text_font(mpos_z_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_z_label, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(mpos_z_label, 225, 185);

    lbl_mpos_z = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_mpos_z, "----.---");
    lv_textarea_set_one_line(lbl_mpos_z, true);
    lv_textarea_set_max_length(lbl_mpos_z, 10);
    lv_obj_set_size(lbl_mpos_z, 180, 40);
    lv_obj_set_pos(lbl_mpos_z, 260, 185);
    lv_obj_clear_flag(lbl_mpos_z, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(lbl_mpos_z, 2, 0);
    lv_obj_set_style_pad_bottom(lbl_mpos_z, 2, 0);
    lv_obj_set_style_pad_left(lbl_mpos_z, 5, 0);
    lv_obj_set_style_text_align(lbl_mpos_z, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_style_bg_color(lbl_mpos_z, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_mpos_z, 0, 0);
    lv_obj_set_style_border_width(lbl_mpos_z, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(lbl_mpos_z, UITheme::AXIS_Z, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_mpos_z, (void*)"MZ");
    lv_obj_add_event_cb(lbl_mpos_z, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_mpos_z, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

    // A-AXIS POSITIONS (Conditional, only if A-axis enabled)
    if (UICommon::isAAxisEnabled()) {
        // Work Position A
        lv_obj_t *wpos_a_label = lv_label_create(tab);
        lv_label_set_text(wpos_a_label, "A");
        lv_obj_set_style_text_font(wpos_a_label, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(wpos_a_label, UITheme::AXIS_A, 0);
        lv_obj_set_pos(wpos_a_label, 0, 230);

        lbl_wpos_a = lv_textarea_create(tab);
        lv_textarea_set_text(lbl_wpos_a, "----.---");
        lv_textarea_set_one_line(lbl_wpos_a, true);
        lv_textarea_set_max_length(lbl_wpos_a, 10);
        lv_obj_set_size(lbl_wpos_a, 180, 40);
        lv_obj_set_pos(lbl_wpos_a, 35, 230);
        lv_obj_clear_flag(lbl_wpos_a, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_text_font(lbl_wpos_a, &lv_font_montserrat_32, 0);
        lv_obj_set_style_pad_top(lbl_wpos_a, 2, 0);
        lv_obj_set_style_pad_bottom(lbl_wpos_a, 2, 0);
        lv_obj_set_style_pad_left(lbl_wpos_a, 5, 0);
        lv_obj_set_style_text_align(lbl_wpos_a, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(lbl_wpos_a, UITheme::AXIS_A, 0);
        lv_obj_set_style_bg_color(lbl_wpos_a, UITheme::BG_BLACK, 0);
        lv_obj_set_style_border_width(lbl_wpos_a, 0, 0);
        lv_obj_set_style_border_width(lbl_wpos_a, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(lbl_wpos_a, UITheme::AXIS_A, LV_STATE_FOCUSED);
        lv_obj_set_user_data(lbl_wpos_a, (void*)"WA");
        lv_obj_add_event_cb(lbl_wpos_a, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(lbl_wpos_a, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

        // Machine Position A
        lv_obj_t *mpos_a_label = lv_label_create(tab);
        lv_label_set_text(mpos_a_label, "A");
        lv_obj_set_style_text_font(mpos_a_label, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(mpos_a_label, UITheme::AXIS_A, 0);
        lv_obj_set_pos(mpos_a_label, 225, 230);

        lbl_mpos_a = lv_textarea_create(tab);
        lv_textarea_set_text(lbl_mpos_a, "----.---");
        lv_textarea_set_one_line(lbl_mpos_a, true);
        lv_textarea_set_max_length(lbl_mpos_a, 10);
        lv_obj_set_size(lbl_mpos_a, 180, 40);
        lv_obj_set_pos(lbl_mpos_a, 260, 230);
        lv_obj_clear_flag(lbl_mpos_a, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_text_font(lbl_mpos_a, &lv_font_montserrat_32, 0);
        lv_obj_set_style_pad_top(lbl_mpos_a, 2, 0);
        lv_obj_set_style_pad_bottom(lbl_mpos_a, 2, 0);
        lv_obj_set_style_pad_left(lbl_mpos_a, 5, 0);
        lv_obj_set_style_text_align(lbl_mpos_a, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_color(lbl_mpos_a, UITheme::AXIS_A, 0);
        lv_obj_set_style_bg_color(lbl_mpos_a, UITheme::BG_BLACK, 0);
        lv_obj_set_style_border_width(lbl_mpos_a, 0, 0);
        lv_obj_set_style_border_width(lbl_mpos_a, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(lbl_mpos_a, UITheme::AXIS_A, LV_STATE_FOCUSED);
        lv_obj_set_user_data(lbl_mpos_a, (void*)"MA");
        lv_obj_add_event_cb(lbl_mpos_a, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(lbl_mpos_a, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);
    }

    // MODAL STATES - Right column (labels at x=615, values at x=735)
    lv_obj_t *modal_header = lv_label_create(tab);
    lv_label_set_text(modal_header, "MODAL");
    lv_obj_set_style_text_font(modal_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(modal_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(modal_header, 615, 70);


    int modalStart = 99;
    int modalSpacing = 31;
    int modalPosition = 0;
    // PLANE
    lv_obj_t *status_plane_label = lv_label_create(tab);
    lv_label_set_text(status_plane_label, "PLANE");
    lv_obj_set_style_text_font(status_plane_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_plane_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_plane_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_plane = lv_label_create(tab);
    lv_label_set_text(lbl_modal_plane, "---");
    lv_obj_set_style_text_font(lbl_modal_plane, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_plane, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(lbl_modal_plane, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // DIST
    lv_obj_t *status_dist_label = lv_label_create(tab);
    lv_label_set_text(status_dist_label, "DIST");
    lv_obj_set_style_text_font(status_dist_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_dist_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_dist_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_dist = lv_label_create(tab);
    lv_label_set_text(lbl_modal_dist, "---");
    lv_obj_set_style_text_font(lbl_modal_dist, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_dist, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(lbl_modal_dist, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // UNITS
    lv_obj_t *status_units_label = lv_label_create(tab);
    lv_label_set_text(status_units_label, "UNITS");
    lv_obj_set_style_text_font(status_units_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_units_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_units_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_units = lv_label_create(tab);
    lv_label_set_text(lbl_modal_units, "---");
    lv_obj_set_style_text_font(lbl_modal_units, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_units, UITheme::POS_WORK, 0);
    lv_obj_set_pos(lbl_modal_units, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // MOTION
    lv_obj_t *status_motion_label = lv_label_create(tab);
    lv_label_set_text(status_motion_label, "MOTION");
    lv_obj_set_style_text_font(status_motion_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_motion_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_motion_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_motion = lv_label_create(tab);
    lv_label_set_text(lbl_modal_motion, "---");
    lv_obj_set_style_text_font(lbl_modal_motion, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_motion, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_motion, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // FEED
    lv_obj_t *status_feedrate_label = lv_label_create(tab);
    lv_label_set_text(status_feedrate_label, "FEED");
    lv_obj_set_style_text_font(status_feedrate_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_feedrate_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_feedrate_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_feedrate = lv_label_create(tab);
    lv_label_set_text(lbl_modal_feedrate, "---");
    lv_obj_set_style_text_font(lbl_modal_feedrate, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_feedrate, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_feedrate, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // SPINDLE
    lv_obj_t *status_spindle_label = lv_label_create(tab);
    lv_label_set_text(status_spindle_label, "SPINDLE");
    lv_obj_set_style_text_font(status_spindle_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_spindle_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_spindle_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_spindle = lv_label_create(tab);
    lv_label_set_text(lbl_modal_spindle, "---");
    lv_obj_set_style_text_font(lbl_modal_spindle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_spindle, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_spindle, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // COOLANT
    lv_obj_t *status_coolant_label = lv_label_create(tab);
    lv_label_set_text(status_coolant_label, "COOLANT");
    lv_obj_set_style_text_font(status_coolant_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_coolant_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_coolant_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_coolant = lv_label_create(tab);
    lv_label_set_text(lbl_modal_coolant, "---");
    lv_obj_set_style_text_font(lbl_modal_coolant, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_coolant, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_coolant, 735, modalStart + (modalSpacing * modalPosition));

    modalPosition++;

    // TOOL
    lv_obj_t *status_tool_label = lv_label_create(tab);
    lv_label_set_text(status_tool_label, "TOOL");
    lv_obj_set_style_text_font(status_tool_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_tool_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_tool_label, 615, modalStart + (modalSpacing * modalPosition));
    
    lbl_modal_tool = lv_label_create(tab);
    lv_label_set_text(lbl_modal_tool, "---");
    lv_obj_set_style_text_font(lbl_modal_tool, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_tool, UITheme::UI_WARNING, 0);
    lv_obj_set_pos(lbl_modal_tool, 735, modalStart + (modalSpacing * modalPosition));

    // 315

    // FEED RATE & SPINDLE - Third column (moved 20px left: 475→455)
    lv_obj_t *status_feed_header = lv_label_create(tab);
    lv_label_set_text(status_feed_header, "FEED RATE");
    lv_obj_set_style_text_font(status_feed_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_feed_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_feed_header, 455, 70);

    lbl_feed_value = lv_label_create(tab);
    lv_label_set_text(lbl_feed_value, "---");
    lv_obj_set_style_text_font(lbl_feed_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_feed_value, lv_color_white(), 0);
    lv_obj_set_pos(lbl_feed_value, 455, 95);
    
    lbl_feed_override = lv_label_create(tab);
    lv_label_set_text(lbl_feed_override, "---%");
    lv_obj_set_style_text_font(lbl_feed_override, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_feed_override, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_feed_override, 555, 70);  // Right next to value
    
    lbl_feed_units = lv_label_create(tab);
    lv_label_set_text(lbl_feed_units, "mm/min");
    lv_obj_set_style_text_font(lbl_feed_units, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_feed_units, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_feed_units, 455, 129);  // Moved up 2px from 131

    // RAPID OVERRIDE - Between feed and spindle
    lv_obj_t *status_rapid_label = lv_label_create(tab);
    lv_label_set_text(status_rapid_label, "RAPID");
    lv_obj_set_style_text_font(status_rapid_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_rapid_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_rapid_label, 455, 147);
    
    lbl_rapid_override = lv_label_create(tab);
    lv_label_set_text(lbl_rapid_override, "---%");
    lv_obj_set_style_text_font(lbl_rapid_override, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_rapid_override, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_rapid_override, 555, 147);  // Right aligned with percentage

    lv_obj_t *status_speed_header = lv_label_create(tab);
    lv_label_set_text(status_speed_header, "SPINDLE");
    lv_obj_set_style_text_font(status_speed_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_speed_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_speed_header, 455, 164);

    lbl_spindle_value = lv_label_create(tab);
    lv_label_set_text(lbl_spindle_value, "---");
    lv_obj_set_style_text_font(lbl_spindle_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_spindle_value, lv_color_white(), 0);
    lv_obj_set_pos(lbl_spindle_value, 455, 185);
    
    lbl_spindle_override = lv_label_create(tab);
    lv_label_set_text(lbl_spindle_override, "---%");
    lv_obj_set_style_text_font(lbl_spindle_override, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_spindle_override, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_spindle_override, 555, 164);  // Right next to value
    
    lbl_spindle_units = lv_label_create(tab);
    lv_label_set_text(lbl_spindle_units, "RPM");
    lv_obj_set_style_text_font(lbl_spindle_units, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_spindle_units, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_spindle_units, 455, 222);  // Second line

    // MESSAGE - Bottom section spanning columns 1-3
    // Position depends on whether A-axis is enabled
    int message_label_y = UICommon::isAAxisEnabled() ? 255 : 235;  // Label position
    int message_y = UICommon::isAAxisEnabled() ? 280 : 260;  // Field position
    int message_height = UICommon::isAAxisEnabled() ? 60 : 80;

    lv_obj_t *message_header = lv_label_create(tab);
    lv_label_set_text(message_header, "MESSAGE");
    lv_obj_set_style_text_font(message_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(message_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(message_header, 0, message_label_y);

    // Hide MESSAGE label when A-axis is enabled to avoid interference with A position fields
    if (UICommon::isAAxisEnabled()) {
        lv_obj_add_flag(message_header, LV_OBJ_FLAG_HIDDEN);
    }

    lbl_message = lv_label_create(tab);
    lv_label_set_text(lbl_message, "No messages.");
    lv_obj_set_size(lbl_message, 600, message_height);
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_bg_color(lbl_message, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_bg_opa(lbl_message, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lbl_message, 1, 0);
    lv_obj_set_style_border_color(lbl_message, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_radius(lbl_message, 5, 0);
    lv_obj_set_style_pad_all(lbl_message, 5, 0);
    lv_obj_set_pos(lbl_message, 0, message_y);
}

void UITabStatus::updateMessage(const char *message) {
    if (lbl_message) {
        lv_label_set_text(lbl_message, message);
        
        // Color code based on message prefix or keywords
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
}

void UITabStatus::updateState(const char *state) {
    if (!lbl_state) return;
    
    // Only update if state changed
    if (strcmp(state, last_state) == 0) {
        return;
    }
    
    lv_label_set_text(lbl_state, state);
    strncpy(last_state, state, sizeof(last_state) - 1);
    last_state[sizeof(last_state) - 1] = '\0';
    
    // Update color based on state
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

void UITabStatus::updateWorkPosition(float x, float y, float z, float a) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_wpos_x && y == last_wpos_y && z == last_wpos_z && a == last_wpos_a) {
        return;
    }

    char buf[20];

    if (lbl_wpos_x && x != last_wpos_x) {
        if (x <= -9999.0f) {
            lv_textarea_set_text(lbl_wpos_x, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", x);
            lv_textarea_set_text(lbl_wpos_x, buf);
        }
        last_wpos_x = x;
    }

    if (lbl_wpos_y && y != last_wpos_y) {
        if (y <= -9999.0f) {
            lv_textarea_set_text(lbl_wpos_y, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", y);
            lv_textarea_set_text(lbl_wpos_y, buf);
        }
        last_wpos_y = y;
    }

    if (lbl_wpos_z && z != last_wpos_z) {
        if (z <= -9999.0f) {
            lv_textarea_set_text(lbl_wpos_z, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", z);
            lv_textarea_set_text(lbl_wpos_z, buf);
        }
        last_wpos_z = z;
    }

    // Update A-axis if enabled and value provided
    if (lbl_wpos_a && a != last_wpos_a && a > -9999.0f) {
        if (a <= -9998.0f) {
            lv_textarea_set_text(lbl_wpos_a, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", a);
            lv_textarea_set_text(lbl_wpos_a, buf);
        }
        last_wpos_a = a;
    }
}

void UITabStatus::updateMachinePosition(float x, float y, float z, float a) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_mpos_x && y == last_mpos_y && z == last_mpos_z && a == last_mpos_a) {
        return;
    }

    char buf[20];

    if (lbl_mpos_x && x != last_mpos_x) {
        if (x <= -9999.0f) {
            lv_textarea_set_text(lbl_mpos_x, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", x);
            lv_textarea_set_text(lbl_mpos_x, buf);
        }
        last_mpos_x = x;
    }

    if (lbl_mpos_y && y != last_mpos_y) {
        if (y <= -9999.0f) {
            lv_textarea_set_text(lbl_mpos_y, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", y);
            lv_textarea_set_text(lbl_mpos_y, buf);
        }
        last_mpos_y = y;
    }

    if (lbl_mpos_z && z != last_mpos_z) {
        if (z <= -9999.0f) {
            lv_textarea_set_text(lbl_mpos_z, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", z);
            lv_textarea_set_text(lbl_mpos_z, buf);
        }
        last_mpos_z = z;
    }

    // Update A-axis if enabled and value provided
    if (lbl_mpos_a && a != last_mpos_a && a > -9999.0f) {
        if (a <= -9998.0f) {
            lv_textarea_set_text(lbl_mpos_a, "----.---");
        } else {
            snprintf(buf, sizeof(buf), "%.3f", a);
            lv_textarea_set_text(lbl_mpos_a, buf);
        }
        last_mpos_a = a;
    }
}

void UITabStatus::updateFeedRate(float rate, float override_pct) {
    // Only update if values changed
    if (rate == last_feed_rate && override_pct == last_feed_override) {
        return;
    }
    
    char buf[32];
    
    if (lbl_feed_value && rate != last_feed_rate) {
        if (rate <= -9999.0f) {
            lv_label_set_text(lbl_feed_value, "---");
        } else {
            snprintf(buf, sizeof(buf), "%.0f", rate);
            lv_label_set_text(lbl_feed_value, buf);
        }
        last_feed_rate = rate;
    }
    
    if (lbl_feed_override && override_pct != last_feed_override) {
        if (override_pct <= -9999.0f) {
            lv_label_set_text(lbl_feed_override, "---%");
        } else {
            snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
            lv_label_set_text(lbl_feed_override, buf);
        }
        last_feed_override = override_pct;
    }
}

void UITabStatus::updateRapidOverride(float override_pct) {
    // Only update if value changed
    if (override_pct == last_rapid_override) {
        return;
    }
    
    if (lbl_rapid_override) {
        char buf[32];
        if (override_pct <= -9999.0f) {
            lv_label_set_text(lbl_rapid_override, "---%");
        } else {
            snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
            lv_label_set_text(lbl_rapid_override, buf);
        }
        last_rapid_override = override_pct;
    }
}

void UITabStatus::updateSpindle(float speed, float override_pct) {
    // Only update if values changed
    if (speed == last_spindle_speed && override_pct == last_spindle_override) {
        return;
    }
    
    char buf[32];
    
    if (lbl_spindle_value && speed != last_spindle_speed) {
        if (speed <= -9999.0f) {
            lv_label_set_text(lbl_spindle_value, "---");
        } else {
            snprintf(buf, sizeof(buf), "%.0f", speed);
            lv_label_set_text(lbl_spindle_value, buf);
        }
        last_spindle_speed = speed;
    }
    
    if (lbl_spindle_override && override_pct != last_spindle_override) {
        if (override_pct <= -9999.0f) {
            lv_label_set_text(lbl_spindle_override, "---%");
        } else {
            snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
            lv_label_set_text(lbl_spindle_override, buf);
        }
        last_spindle_override = override_pct;
    }
}

void UITabStatus::updateModalStates(const char *wcs, const char *plane, const char *dist, 
                                    const char *units, const char *motion, const char *feedrate,
                                    const char *spindle, const char *coolant, const char *tool) {
    if (lbl_modal_wcs_value && strcmp(wcs, last_modal_wcs) != 0) {
        lv_label_set_text(lbl_modal_wcs_value, wcs);
        strncpy(last_modal_wcs, wcs, sizeof(last_modal_wcs) - 1);
        last_modal_wcs[sizeof(last_modal_wcs) - 1] = '\0';
    }
    
    if (lbl_modal_plane && strcmp(plane, last_modal_plane) != 0) {
        lv_label_set_text(lbl_modal_plane, plane);
        strncpy(last_modal_plane, plane, sizeof(last_modal_plane) - 1);
        last_modal_plane[sizeof(last_modal_plane) - 1] = '\0';
    }
    
    if (lbl_modal_dist && strcmp(dist, last_modal_dist) != 0) {
        lv_label_set_text(lbl_modal_dist, dist);
        strncpy(last_modal_dist, dist, sizeof(last_modal_dist) - 1);
        last_modal_dist[sizeof(last_modal_dist) - 1] = '\0';
    }
    
    if (lbl_modal_units && strcmp(units, last_modal_units) != 0) {
        lv_label_set_text(lbl_modal_units, units);
        strncpy(last_modal_units, units, sizeof(last_modal_units) - 1);
        last_modal_units[sizeof(last_modal_units) - 1] = '\0';
    }
    
    if (lbl_modal_motion && strcmp(motion, last_modal_motion) != 0) {
        lv_label_set_text(lbl_modal_motion, motion);
        strncpy(last_modal_motion, motion, sizeof(last_modal_motion) - 1);
        last_modal_motion[sizeof(last_modal_motion) - 1] = '\0';
    }
    
    if (lbl_modal_feedrate && strcmp(feedrate, last_modal_feedrate) != 0) {
        lv_label_set_text(lbl_modal_feedrate, feedrate);
        strncpy(last_modal_feedrate, feedrate, sizeof(last_modal_feedrate) - 1);
        last_modal_feedrate[sizeof(last_modal_feedrate) - 1] = '\0';
    }
    
    if (lbl_modal_spindle && strcmp(spindle, last_modal_spindle) != 0) {
        lv_label_set_text(lbl_modal_spindle, spindle);
        strncpy(last_modal_spindle, spindle, sizeof(last_modal_spindle) - 1);
        last_modal_spindle[sizeof(last_modal_spindle) - 1] = '\0';
    }
    
    if (lbl_modal_coolant && strcmp(coolant, last_modal_coolant) != 0) {
        lv_label_set_text(lbl_modal_coolant, coolant);
        strncpy(last_modal_coolant, coolant, sizeof(last_modal_coolant) - 1);
        last_modal_coolant[sizeof(last_modal_coolant) - 1] = '\0';
    }
    
    if (lbl_modal_tool && strcmp(tool, last_modal_tool) != 0) {
        lv_label_set_text(lbl_modal_tool, tool);
        strncpy(last_modal_tool, tool, sizeof(last_modal_tool) - 1);
        last_modal_tool[sizeof(last_modal_tool) - 1] = '\0';
    }
}

void UITabStatus::updateControlButtons(int machine_state) {
    if (!btn_pause || !btn_stop || !btn_cancel_jog) return;
    
    if (machine_state == STATE_JOG) {
        // Jogging - show cancel jog button, hide pause/stop
        lv_obj_clear_flag(btn_cancel_jog, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_pause, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
    } else if (machine_state == STATE_RUN) {
        // Running job - show pause/stop buttons, hide cancel jog
        lv_obj_clear_flag(btn_pause, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_cancel_jog, LV_OBJ_FLAG_HIDDEN);
        
        // Update pause button appearance (same logic as Actions tab)
        if (lbl_pause) {
            lv_obj_set_style_bg_color(btn_pause, UITheme::STATE_HOLD, LV_PART_MAIN);
            lv_label_set_text(lbl_pause, LV_SYMBOL_PAUSE " Pause");
        }
    } else if (machine_state == STATE_HOLD || machine_state == STATE_DOOR) {
        // Paused or Door Hold - show pause (as resume) and stop buttons
        lv_obj_clear_flag(btn_pause, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_cancel_jog, LV_OBJ_FLAG_HIDDEN);
        
        // Update pause button to show resume
        if (lbl_pause) {
            lv_obj_set_style_bg_color(btn_pause, UITheme::BTN_PLAY, LV_PART_MAIN);
            lv_label_set_text(lbl_pause, LV_SYMBOL_PLAY " Resume");
        }
    } else {
        // All other states - hide all control buttons
        lv_obj_add_flag(btn_pause, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_cancel_jog, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabStatus::onPauseResumeClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) return;
    
    FluidNCStatus status = FluidNCClient::getStatus();
    
    if (status.state == STATE_HOLD || status.state == STATE_DOOR) {
        // Machine is paused or door hold - send Resume command
        Serial.println("[Status Tab] Sending Resume command (~)");
        FluidNCClient::sendCommand("~");
    } else {
        // Machine is running - send Pause command
        Serial.println("[Status Tab] Sending Pause command (!)");
        FluidNCClient::sendCommand("!");
    }
}

void UITabStatus::onStopClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) return;
    
    Serial.println("[Status Tab] STOP - Sending Door Hold (0x84)");
    char door_hold_cmd[2] = {0x84, 0x00};
    FluidNCClient::sendCommand(door_hold_cmd);
}

void UITabStatus::onCancelJogClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) return;
    
    Serial.println("[Status Tab] Cancelling jog (0x85)");
    char cancel_cmd[2] = {0x85, 0x00};
    FluidNCClient::sendCommand(cancel_cmd);
}

// Event handler for position field focus (show keyboard)
void UITabStatus::position_field_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Only allow position editing when machine is IDLE
        const FluidNCStatus& status = FluidNCClient::getStatus();
        if (status.state != STATE_IDLE) {
            // Clear focus state from the textarea
            lv_obj_clear_state(textarea, LV_STATE_FOCUSED);
            return;
        }
        
        active_textarea = textarea;
        
        // Store original value for cancel restoration
        strncpy(original_value, lv_textarea_get_text(textarea), sizeof(original_value) - 1);
        original_value[sizeof(original_value) - 1] = '\0';
        
        // Create custom keyboard if it doesn't exist
        if (!keyboard) {
            // Create keyboard container on right side of screen
            keyboard = lv_obj_create(lv_scr_act());
            lv_obj_set_size(keyboard, 325, 340);  // Wider to cover column 3, 20px shorter
            lv_obj_set_pos(keyboard, 465, 70);  // Moved up 10px from 80
            lv_obj_set_style_bg_color(keyboard, UITheme::BG_DARK, 0);
            lv_obj_set_style_border_color(keyboard, UITheme::BORDER_MEDIUM, 0);
            lv_obj_set_style_border_width(keyboard, 2, 0);
            lv_obj_set_style_pad_all(keyboard, 10, 0);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_CLICK_FOCUSABLE);  // Prevent keyboard from stealing focus
            
            // Create number pad buttons (3x4 grid)
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
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);  // Prevent buttons from stealing focus
                
                lv_obj_t *label = lv_label_create(btn);
                lv_label_set_text(label, num_labels[i]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_center(label);
                
                // Store character in user data
                lv_obj_set_user_data(btn, (void*)num_labels[i]);
                lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                        lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
                        const char* ch = (const char*)lv_obj_get_user_data(target);
                        lv_textarea_add_text(active_textarea, ch);
                    }
                }, LV_EVENT_CLICKED, NULL);
            }
            
            // Clear button
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
            
            // Backspace button
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
            
            // OK button (now on left)
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
            
            // Cancel button (now on right)
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
        
        Serial.printf("Position field focused: %s\n", (const char*)lv_obj_get_user_data(textarea));
    }
    
    if (code == LV_EVENT_DEFOCUSED) {
        // Textarea lost focus - check if we're switching to another textarea or clicking elsewhere
        if (keyboard && !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);
            
            // Check if one of the other textareas is getting focus
            // If so, don't close keyboard - let the FOCUSED event handler deal with it
            bool switching_fields = (lv_obj_has_state(lbl_wpos_x, LV_STATE_FOCUSED) ||
                                    lv_obj_has_state(lbl_wpos_y, LV_STATE_FOCUSED) ||
                                    lv_obj_has_state(lbl_wpos_z, LV_STATE_FOCUSED) ||
                                    (lbl_wpos_a && lv_obj_has_state(lbl_wpos_a, LV_STATE_FOCUSED)) ||
                                    lv_obj_has_state(lbl_mpos_x, LV_STATE_FOCUSED) ||
                                    lv_obj_has_state(lbl_mpos_y, LV_STATE_FOCUSED) ||
                                    lv_obj_has_state(lbl_mpos_z, LV_STATE_FOCUSED) ||
                                    (lbl_mpos_a && lv_obj_has_state(lbl_mpos_a, LV_STATE_FOCUSED)));
            
            // Only close keyboard if we're not switching to another position field
            if (!switching_fields) {
                if (active_textarea) {
                    lv_textarea_set_text(active_textarea, original_value);
                }
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
                active_textarea = nullptr;
                Serial.println("Keyboard closed by defocus");
            }
        }
    }
}

// Show validation error dialog
void UITabStatus::showValidationError(const char* message) {
    lv_obj_t *dialog = lv_obj_create(lv_screen_active());
    lv_obj_set_size(dialog, 400, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    
    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "VALIDATION ERROR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Message
    lv_obj_t *msg = lv_label_create(dialog);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
    
    // OK button
    lv_obj_t *btn = lv_btn_create(dialog);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, UITheme::BTN_CONNECT, 0);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
    lv_obj_center(btn_label);
    
    // Close dialog on button click
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        lv_obj_t *dialog = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(dialog);
    }, LV_EVENT_CLICKED, dialog);
}

// Event handler for keyboard ready/cancel (send jog command or close)
void UITabStatus::keyboard_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        const char* action = (const char*)lv_obj_get_user_data(btn);
        
        if (strcmp(action, "ok") == 0) {
            // User pressed OK - validate and send jog command
            if (active_textarea) {
                const char *axis_id = (const char*)lv_obj_get_user_data(active_textarea);
                const char *value_str = lv_textarea_get_text(active_textarea);
                
                // Validate input is a number
                char *endptr;
                float value = strtof(value_str, &endptr);
                if (endptr == value_str || *endptr != '\0') {
                    // Invalid number
                    showValidationError("Invalid number entered");
                    return;
                }
                
                char axis = axis_id[1];  // X, Y, Z, or A

                // Note: Position bounds validation would require checking if machine is homed
                // and querying $/axes/{axis}/max_travel_mm. For simplicity, we rely on
                // FluidNC's soft limits to reject invalid moves and provide feedback.

                char command[64];

                // Get appropriate feed rate based on axis
                int feed_rate;
                if (axis == 'Z') {
                    feed_rate = UITabSettingsJog::getDefaultZFeed();
                } else if (axis == 'A') {
                    feed_rate = UITabSettingsJog::getDefaultAFeed();
                } else {
                    feed_rate = UITabSettingsJog::getDefaultXYFeed();
                }
                
                // Determine if work or machine coordinates and which axis
                if (axis_id[0] == 'W') {
                    // Work coordinates - use $J jogging command with work coordinate system
                    snprintf(command, sizeof(command), "$J=%c%.3f F%d\n", axis, value, feed_rate);
                    Serial.printf("Jogging to work position: %s", command);
                } else if (axis_id[0] == 'M') {
                    // Machine coordinates - use $J with G53 (move in machine coordinate system)
                    snprintf(command, sizeof(command), "$J=G53 %c%.3f F%d\n", axis, value, feed_rate);
                    Serial.printf("Jogging to machine position: %s", command);
                }
                
                FluidNCClient::sendCommand(command);
            }
            
            // Hide keyboard
            if (keyboard) {
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
            active_textarea = nullptr;
            
        } else if (strcmp(action, "cancel") == 0) {
            // User cancelled - restore original value and refresh from FluidNC
            if (active_textarea) {
                lv_textarea_set_text(active_textarea, original_value);
                
                // Send status query to FluidNC to refresh current position
                FluidNCClient::sendCommand("?");
                Serial.println("Position edit cancelled, restored original value and requested status update");
            }
            
            if (keyboard) {
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
            active_textarea = nullptr;
        }
    }
}

// WCS Button Click Handler
void UITabStatus::onWCSButtonClicked(lv_event_t *e) {
    Serial.println("WCS button clicked, requesting coordinate offsets");
    
    // Set callback to capture the $# response
    FluidNCClient::setMessageCallback([](const char* message) {
        parseWCSOffsetsResponse(message);
    });
    
    // Request coordinate offsets from FluidNC
    // Popup will be shown automatically when G59 is parsed
    FluidNCClient::sendCommand("$#\n");
}

// Parse WCS offsets response from $# command
void UITabStatus::parseWCSOffsetsResponse(const char* response) {
    // FluidNC sends each coordinate system as a separate message
    // Example: [G54:90.000,0.000,0.000]
    
    // Map of WCS names to array indices
    const char* wcs_names[] = {"G54", "G55", "G56", "G57", "G58", "G59", "G28", "G30", "G92", "TLO"};
    
    // Check if this message contains a WCS offset
    if (response[0] != '[') return;
    
    // Find which WCS this is
    for (int i = 0; i < 10; i++) {
        char search[8];
        snprintf(search, sizeof(search), "[%s:", wcs_names[i]);
        
        if (strncmp(response, search, strlen(search)) == 0) {
            // Found matching WCS - extract coordinates
            const char* start = response + strlen(search);
            const char* end = strchr(start, ']');
            
            if (end) {
                int len = end - start;
                if (len < 64) {
                    strncpy(wcs_offsets[i], start, len);
                    wcs_offsets[i][len] = '\0';
                    Serial.printf("Parsed %s: %s\n", wcs_names[i], wcs_offsets[i]);
                    
                    // Show popup after parsing G59 (last coordinate system we display)
                    if (i == 5) {  // G59 is at index 5
                        Serial.println("G59 parsed, showing WCS popup");
                        showWCSPopup();
                    }
                }
            }
            break;
        }
    }
}

// Show WCS Selection Popup
void UITabStatus::showWCSPopup() {
    if (wcs_popup) {
        lv_obj_del(wcs_popup);
    }
    
    // Create modal backdrop
    wcs_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(wcs_popup, 800, 480);
    lv_obj_set_pos(wcs_popup, 0, 0);
    lv_obj_set_style_bg_color(wcs_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(wcs_popup, LV_OPA_70, 0);
    lv_obj_set_style_border_width(wcs_popup, 0, 0);
    lv_obj_clear_flag(wcs_popup, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create content panel
    wcs_popup_content = lv_obj_create(wcs_popup);
    lv_obj_set_size(wcs_popup_content, 700, 400);
    lv_obj_center(wcs_popup_content);
    lv_obj_set_style_bg_color(wcs_popup_content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(wcs_popup_content, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_border_width(wcs_popup_content, 3, 0);
    lv_obj_set_style_pad_all(wcs_popup_content, 15, 0);
    lv_obj_clear_flag(wcs_popup_content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(wcs_popup_content);
    lv_label_set_text(title, "SELECT WORK COORDINATE SYSTEM");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(title, 0, 0);
    
    // Create WCS buttons in 2 columns (no scrolling)
    const char* wcs_names[] = {"G54", "G55", "G56", "G57", "G58", "G59"};
    current_wcs_index = -1;  // Reset current WCS tracking
    
    // Load WCS configurations from Preferences
    char wcs_custom_names[6][32] = {{0}};  // Custom names for G54-G59
    bool wcs_locked[6] = {false};          // Lock status for G54-G59
    
    int machine_index = MachineConfigManager::getSelectedMachineIndex();
    if (machine_index >= 0) {
        WCSConfig::loadWCSConfig(machine_index, wcs_custom_names, wcs_locked);
    }
    
    for (int i = 0; i < 6; i++) {
        // Calculate position: column 0 (left) for G54-G56, column 1 (right) for G57-G59
        int col = i / 3;  // 0 for indices 0-2, 1 for indices 3-5
        int row = i % 3;  // Row within column (0, 1, 2)
        int x = col * 335;  // Left column at x=0, right column at x=335
        int y = 40 + (row * 85);  // Start at y=40, space buttons 85px apart
        
        lv_obj_t *btn = lv_button_create(wcs_popup_content);
        lv_obj_set_size(btn, 325, 75);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_bg_color(btn, UITheme::BG_BUTTON, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);
        
        // Add transparent border by default to prevent layout shift when selected
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, UITheme::BG_BUTTON, LV_PART_MAIN);  // Match background
        
        // Check if this is the current WCS
        bool is_current = (strcmp(wcs_names[i], lv_label_get_text(lbl_modal_wcs_value)) == 0);
        if (is_current) {
            current_wcs_index = i;  // Store which button is current
            lv_obj_set_style_border_color(btn, UITheme::ACCENT_SECONDARY, LV_PART_MAIN);
        }
        
        // WCS code label (large, yellow)
        lv_obj_t *lbl_wcs_code = lv_label_create(btn);
        lv_label_set_text(lbl_wcs_code, wcs_names[i]);
        lv_obj_set_style_text_font(lbl_wcs_code, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_wcs_code, UITheme::POS_MODAL, 0);
        lv_obj_align(lbl_wcs_code, LV_ALIGN_TOP_LEFT, 0, 0);
        
        // Custom name label (if exists) - orange color, to the right of WCS code
        if (wcs_custom_names[i][0] != '\0') {
            lv_obj_t *lbl_custom = lv_label_create(btn);
            lv_label_set_text(lbl_custom, wcs_custom_names[i]);
            lv_obj_set_style_text_font(lbl_custom, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(lbl_custom, UITheme::POS_WORK, 0);
            lv_label_set_long_mode(lbl_custom, LV_LABEL_LONG_DOT);
            lv_obj_set_width(lbl_custom, wcs_locked[i] ? 215 : 245);  // Leave room for lock icon if present
            lv_obj_align(lbl_custom, LV_ALIGN_TOP_LEFT, 70, 3);  // To the right of WCS code, slight vertical adjustment
        }
        
        // Lock icon (bottom right) if locked
        if (wcs_locked[i]) {
            lv_obj_t *lbl_lock = lv_label_create(btn);
            lv_label_set_text(lbl_lock, FA_ICON_LOCK);
            lv_obj_set_style_text_font(lbl_lock, &fontawesome_icons_20, 0);
            lv_obj_set_style_text_color(lbl_lock, UITheme::ACCENT_SECONDARY, 0);
            lv_obj_align(lbl_lock, LV_ALIGN_BOTTOM_RIGHT, 0, -18);
        }
        
        // Coordinates label with axis colors (axis labels AND values colored)
        lv_obj_t *lbl_coords = lv_label_create(btn);
        char text[120];
        char* coords = wcs_offsets[i];
        
        // Parse the three coordinate values
        float x_val = 0, y_val = 0, z_val = 0;
        sscanf(coords, "%f,%f,%f", &x_val, &y_val, &z_val);
        
        // Format with axis color codes (LVGL recolor feature)
        // Extract RGB components from theme colors (convert from RGB565 to RGB888)
        uint32_t x_color = lv_color_to_u32(UITheme::AXIS_X);
        uint32_t y_color = lv_color_to_u32(UITheme::AXIS_Y);
        uint32_t z_color = lv_color_to_u32(UITheme::AXIS_Z);
        
        snprintf(text, sizeof(text), 
                 "#%06X X: %.3f#  #%06X Y: %.3f#  #%06X Z: %.3f#",
                 x_color & 0xFFFFFF, x_val,
                 y_color & 0xFFFFFF, y_val,
                 z_color & 0xFFFFFF, z_val);
        
        lv_label_set_text(lbl_coords, text);
        lv_obj_set_style_text_font(lbl_coords, &lv_font_montserrat_16, 0);
        lv_label_set_recolor(lbl_coords, true);
        lv_obj_align(lbl_coords, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        
        // Store button reference for highlighting
        wcs_buttons[i] = btn;
        
        // Store WCS index in user data
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, onWCSSelected, LV_EVENT_CLICKED, nullptr);
    }
    
    // Set button (left side)
    wcs_btn_set = lv_button_create(wcs_popup_content);
    lv_obj_set_size(wcs_btn_set, 150, 50);
    lv_obj_set_pos(wcs_btn_set, 175, 305);
    lv_obj_set_style_bg_color(wcs_btn_set, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_add_flag(wcs_btn_set, LV_OBJ_FLAG_HIDDEN);  // Hidden until WCS selected
    
    lv_obj_t *lbl_set = lv_label_create(wcs_btn_set);
    lv_label_set_text(lbl_set, "Set");
    lv_obj_set_style_text_font(lbl_set, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_set);
    
    lv_obj_add_event_cb(wcs_btn_set, onWCSSetClicked, LV_EVENT_CLICKED, nullptr);
    
    // Cancel button (right side)
    wcs_btn_cancel = lv_button_create(wcs_popup_content);
    lv_obj_set_size(wcs_btn_cancel, 150, 50);
    lv_obj_set_pos(wcs_btn_cancel, 375, 305);
    lv_obj_set_style_bg_color(wcs_btn_cancel, UITheme::BG_BUTTON, LV_PART_MAIN);
    
    lv_obj_t *lbl_cancel = lv_label_create(wcs_btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
    
    lv_obj_add_event_cb(wcs_btn_cancel, onWCSCancelClicked, LV_EVENT_CLICKED, nullptr);
    
    // Initialize selection state
    selected_wcs_index = -1;
}

// Hide WCS Popup
void UITabStatus::hideWCSPopup() {
    if (wcs_popup) {
        lv_obj_del(wcs_popup);
        wcs_popup = nullptr;
        wcs_popup_content = nullptr;
        wcs_btn_set = nullptr;
        wcs_btn_cancel = nullptr;
        for (int i = 0; i < 6; i++) {
            wcs_buttons[i] = nullptr;
        }
        selected_wcs_index = -1;
        current_wcs_index = -1;
    }
    
    // Clear message callback
    FluidNCClient::clearMessageCallback();
}

// WCS Selected Handler (highlights selection, doesn't execute)
void UITabStatus::onWCSSelected(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    Serial.printf("WCS button clicked: index %d\n", index);
    
    // Clear previous selection highlighting
    if (selected_wcs_index >= 0 && selected_wcs_index < 6) {
        // If previous selection was the current WCS, restore its 2px teal border
        if (selected_wcs_index == current_wcs_index) {
            lv_obj_set_style_border_color(wcs_buttons[selected_wcs_index], UITheme::ACCENT_SECONDARY, LV_PART_MAIN);
            lv_obj_set_style_border_width(wcs_buttons[selected_wcs_index], 2, LV_PART_MAIN);
        } else {
            // Otherwise, restore transparent 2px border (matches background)
            lv_obj_set_style_border_color(wcs_buttons[selected_wcs_index], UITheme::BG_BUTTON, LV_PART_MAIN);
            lv_obj_set_style_border_width(wcs_buttons[selected_wcs_index], 2, LV_PART_MAIN);
        }
    }
    
    // Highlight new selection with blue 3px border
    selected_wcs_index = index;
    lv_obj_set_style_border_color(btn, UITheme::ACCENT_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN);
    
    // Show Set button
    if (wcs_btn_set) {
        lv_obj_clear_flag(wcs_btn_set, LV_OBJ_FLAG_HIDDEN);
    }
}

// WCS Set Button Handler (executes the command)
void UITabStatus::onWCSSetClicked(lv_event_t *e) {
    if (selected_wcs_index < 0 || selected_wcs_index >= 6) {
        Serial.println("No WCS selected");
        return;
    }
    
    const char* wcs_names[] = {"G54", "G55", "G56", "G57", "G58", "G59"};
    const char* wcs = wcs_names[selected_wcs_index];
    
    Serial.printf("Setting WCS to: %s\n", wcs);
    
    // Send command to FluidNC to switch WCS
    char command[8];
    snprintf(command, sizeof(command), "%s\n", wcs);
    FluidNCClient::sendCommand(command);
    
    // Close popup
    hideWCSPopup();
}

// WCS Cancel Button Handler
void UITabStatus::onWCSCancelClicked(lv_event_t *e) {
    hideWCSPopup();
}
