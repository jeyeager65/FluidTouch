#include "ui/tabs/ui_tab_status.h"
#include "ui/ui_theme.h"
#include <Arduino.h>
#include <cstring>
#include <stdio.h>

// Static member initialization
lv_obj_t *UITabStatus::lbl_message = nullptr;
lv_obj_t *UITabStatus::lbl_state = nullptr;
lv_obj_t *UITabStatus::lbl_file_progress_container = nullptr;
lv_obj_t *UITabStatus::lbl_filename = nullptr;
lv_obj_t *UITabStatus::bar_progress = nullptr;
lv_obj_t *UITabStatus::lbl_percent = nullptr;
lv_obj_t *UITabStatus::lbl_elapsed_time = nullptr;
lv_obj_t *UITabStatus::lbl_elapsed_unit = nullptr;
lv_obj_t *UITabStatus::lbl_estimated_time = nullptr;
lv_obj_t *UITabStatus::lbl_estimated_unit = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_x = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_y = nullptr;
lv_obj_t *UITabStatus::lbl_wpos_z = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_x = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_y = nullptr;
lv_obj_t *UITabStatus::lbl_mpos_z = nullptr;
lv_obj_t *UITabStatus::lbl_feed_value = nullptr;
lv_obj_t *UITabStatus::lbl_feed_override = nullptr;
lv_obj_t *UITabStatus::lbl_feed_units = nullptr;
lv_obj_t *UITabStatus::lbl_rapid_override = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_value = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_override = nullptr;
lv_obj_t *UITabStatus::lbl_spindle_units = nullptr;
lv_obj_t *UITabStatus::lbl_modal_wcs = nullptr;
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
float UITabStatus::last_mpos_x = -9999.0f;
float UITabStatus::last_mpos_y = -9999.0f;
float UITabStatus::last_mpos_z = -9999.0f;
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

    // FILE PROGRESS - Spans columns 2-4 (appears only when file is running)
    lbl_file_progress_container = lv_obj_create(tab);
    lv_obj_set_size(lbl_file_progress_container, 550, 50);  // Span from col 2 to col 4
    lv_obj_set_pos(lbl_file_progress_container, 230, 0);
    lv_obj_set_style_bg_color(lbl_file_progress_container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(lbl_file_progress_container, 1, 0);
    lv_obj_set_style_border_color(lbl_file_progress_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(lbl_file_progress_container, 5, 0);
    lv_obj_clear_flag(lbl_file_progress_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    lv_obj_add_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Filename label
    lbl_filename = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_filename, "filename.gcode");
    lv_obj_set_style_text_font(lbl_filename, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_filename, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_filename, 0, 0);
    lv_label_set_long_mode(lbl_filename, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_filename, 350);  // Truncate long filenames
    
    // Progress bar (expanded 80px wider: 250→330, 2px taller: 13→15, black background)
    bar_progress = lv_bar_create(lbl_file_progress_container);
    lv_obj_set_size(bar_progress, 330, 15);  // 2px taller: 13→15
    lv_obj_set_pos(bar_progress, 0, 20);
    lv_obj_set_style_bg_color(bar_progress, UITheme::BG_BLACK, LV_PART_MAIN);  // Black for incomplete
    lv_obj_set_style_bg_color(bar_progress, UITheme::UI_SUCCESS, LV_PART_INDICATOR);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    
    // Percentage label (next to progress bar)
    lbl_percent = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_percent, "0.0%");
    lv_obj_set_style_text_font(lbl_percent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_percent, UITheme::UI_SUCCESS, 0);
    lv_obj_set_pos(lbl_percent, 340, 18);
    
    // Elapsed time - split into label, value, unit (moved 85px right for 5px more space)
    lv_obj_t *elapsed_label = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(elapsed_label, "Elapsed");
    lv_obj_set_style_text_font(elapsed_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(elapsed_label, UITheme::UI_INFO, 0);  // Colored label
    lv_obj_set_style_text_align(elapsed_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(elapsed_label, 385, 2);  // 5px more space (390→385)
    lv_obj_set_width(elapsed_label, 65);  // 5px wider (60→65)
    
    lbl_elapsed_time = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_elapsed_time, "00:00");
    lv_obj_set_style_text_font(lbl_elapsed_time, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_elapsed_time, UITheme::TEXT_LIGHT, 0);  // White value
    lv_obj_set_pos(lbl_elapsed_time, 455, 2);
    
    lbl_elapsed_unit = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_elapsed_unit, "min:sec");  // Changed from mm:ss
    lv_obj_set_style_text_font(lbl_elapsed_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_elapsed_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_elapsed_unit, 495, 2);
    
    // Estimated time - split into label, value, unit
    lv_obj_t *estimated_label = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(estimated_label, "Estimated");
    lv_obj_set_style_text_font(estimated_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(estimated_label, UITheme::UI_WARNING, 0);  // Colored label
    lv_obj_set_style_text_align(estimated_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(estimated_label, 385, 22);  // 5px more space (390→385)
    lv_obj_set_width(estimated_label, 65);  // 5px wider (60→65)
    
    lbl_estimated_time = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_estimated_time, "00:00");
    lv_obj_set_style_text_font(lbl_estimated_time, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_estimated_time, UITheme::TEXT_LIGHT, 0);  // White value
    lv_obj_set_pos(lbl_estimated_time, 455, 22);
    
    lbl_estimated_unit = lv_label_create(lbl_file_progress_container);
    lv_label_set_text(lbl_estimated_unit, "min:sec");  // Changed from mm:ss
    lv_obj_set_style_text_font(lbl_estimated_unit, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_estimated_unit, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_estimated_unit, 495, 22);

    // Separator line
    lv_obj_t *line1 = lv_obj_create(tab);
    lv_obj_set_size(line1, 780, 2);
    lv_obj_set_pos(line1, 0, 60);
    lv_obj_set_style_bg_color(line1, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_border_width(line1, 0, 0);

    // WORK POSITION - Left column
    lv_obj_t *wpos_header = lv_label_create(tab);
    lv_label_set_text(wpos_header, "WORK POSITION");
    lv_obj_set_style_text_font(wpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(wpos_header, 0, 70);

    lbl_wpos_x = lv_label_create(tab);
    lv_label_set_text(lbl_wpos_x, "X  ----.---");
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_text_letter_space(lbl_wpos_x, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_wpos_x, 0, 95);

    lbl_wpos_y = lv_label_create(tab);
    lv_label_set_text(lbl_wpos_y, "Y  ----.---");
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_style_text_letter_space(lbl_wpos_y, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_wpos_y, 0, 140);

    lbl_wpos_z = lv_label_create(tab);
    lv_label_set_text(lbl_wpos_z, "Z  ----.---");
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_style_text_letter_space(lbl_wpos_z, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_wpos_z, 0, 185);

    // MACHINE POSITION - Center column (moved right for wider values)
    lv_obj_t *mpos_header = lv_label_create(tab);
    lv_label_set_text(mpos_header, "MACHINE POSITION");
    lv_obj_set_style_text_font(mpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(mpos_header, 225, 70);

    lbl_mpos_x = lv_label_create(tab);
    lv_label_set_text(lbl_mpos_x, "X  ----.---");
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_text_letter_space(lbl_mpos_x, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_mpos_x, 225, 95);

    lbl_mpos_y = lv_label_create(tab);
    lv_label_set_text(lbl_mpos_y, "Y  ----.---");
    lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_style_text_letter_space(lbl_mpos_y, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_mpos_y, 225, 140);

    lbl_mpos_z = lv_label_create(tab);
    lv_label_set_text(lbl_mpos_z, "Z  ----.---");
    lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_style_text_letter_space(lbl_mpos_z, 1, 0);  // Tighter spacing for monospace feel
    lv_obj_set_pos(lbl_mpos_z, 225, 185);

    // MODAL STATES - Right column (labels at x=615, values at x=735)
    lv_obj_t *modal_header = lv_label_create(tab);
    lv_label_set_text(modal_header, "MODAL");
    lv_obj_set_style_text_font(modal_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(modal_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(modal_header, 615, 70);

    // WCS
    lv_obj_t *status_coord_label = lv_label_create(tab);
    lv_label_set_text(status_coord_label, "WCS");
    lv_obj_set_style_text_font(status_coord_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_coord_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_coord_label, 615, 99);
    
    lbl_modal_wcs = lv_label_create(tab);
    lv_label_set_text(lbl_modal_wcs, "---");
    lv_obj_set_style_text_font(lbl_modal_wcs, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_wcs, UITheme::POS_MODAL, 0);
    lv_obj_set_pos(lbl_modal_wcs, 735, 99);

    // PLANE
    lv_obj_t *status_plane_label = lv_label_create(tab);
    lv_label_set_text(status_plane_label, "PLANE");
    lv_obj_set_style_text_font(status_plane_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_plane_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_plane_label, 615, 126);
    
    lbl_modal_plane = lv_label_create(tab);
    lv_label_set_text(lbl_modal_plane, "---");
    lv_obj_set_style_text_font(lbl_modal_plane, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_plane, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(lbl_modal_plane, 735, 126);

    // DIST
    lv_obj_t *status_dist_label = lv_label_create(tab);
    lv_label_set_text(status_dist_label, "DIST");
    lv_obj_set_style_text_font(status_dist_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_dist_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_dist_label, 615, 153);
    
    lbl_modal_dist = lv_label_create(tab);
    lv_label_set_text(lbl_modal_dist, "---");
    lv_obj_set_style_text_font(lbl_modal_dist, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_dist, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(lbl_modal_dist, 735, 153);

    // UNITS
    lv_obj_t *status_units_label = lv_label_create(tab);
    lv_label_set_text(status_units_label, "UNITS");
    lv_obj_set_style_text_font(status_units_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_units_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_units_label, 615, 180);
    
    lbl_modal_units = lv_label_create(tab);
    lv_label_set_text(lbl_modal_units, "---");
    lv_obj_set_style_text_font(lbl_modal_units, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_units, UITheme::POS_WORK, 0);
    lv_obj_set_pos(lbl_modal_units, 735, 180);

    // MOTION
    lv_obj_t *status_motion_label = lv_label_create(tab);
    lv_label_set_text(status_motion_label, "MOTION");
    lv_obj_set_style_text_font(status_motion_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_motion_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_motion_label, 615, 207);
    
    lbl_modal_motion = lv_label_create(tab);
    lv_label_set_text(lbl_modal_motion, "---");
    lv_obj_set_style_text_font(lbl_modal_motion, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_motion, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_motion, 735, 207);

    // FEED
    lv_obj_t *status_feedrate_label = lv_label_create(tab);
    lv_label_set_text(status_feedrate_label, "FEED");
    lv_obj_set_style_text_font(status_feedrate_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_feedrate_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_feedrate_label, 615, 234);
    
    lbl_modal_feedrate = lv_label_create(tab);
    lv_label_set_text(lbl_modal_feedrate, "---");
    lv_obj_set_style_text_font(lbl_modal_feedrate, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_feedrate, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_feedrate, 735, 234);

    // SPINDLE
    lv_obj_t *status_spindle_label = lv_label_create(tab);
    lv_label_set_text(status_spindle_label, "SPINDLE");
    lv_obj_set_style_text_font(status_spindle_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_spindle_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_spindle_label, 615, 261);
    
    lbl_modal_spindle = lv_label_create(tab);
    lv_label_set_text(lbl_modal_spindle, "---");
    lv_obj_set_style_text_font(lbl_modal_spindle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_spindle, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_spindle, 735, 261);

    // COOLANT
    lv_obj_t *status_coolant_label = lv_label_create(tab);
    lv_label_set_text(status_coolant_label, "COOLANT");
    lv_obj_set_style_text_font(status_coolant_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_coolant_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_coolant_label, 615, 288);
    
    lbl_modal_coolant = lv_label_create(tab);
    lv_label_set_text(lbl_modal_coolant, "---");
    lv_obj_set_style_text_font(lbl_modal_coolant, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_coolant, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_modal_coolant, 735, 288);

    // TOOL
    lv_obj_t *status_tool_label = lv_label_create(tab);
    lv_label_set_text(status_tool_label, "TOOL");
    lv_obj_set_style_text_font(status_tool_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_tool_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(status_tool_label, 615, 315);
    
    lbl_modal_tool = lv_label_create(tab);
    lv_label_set_text(lbl_modal_tool, "---");
    lv_obj_set_style_text_font(lbl_modal_tool, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_tool, UITheme::UI_WARNING, 0);
    lv_obj_set_pos(lbl_modal_tool, 735, 315);

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
    lv_obj_t *message_header = lv_label_create(tab);
    lv_label_set_text(message_header, "MESSAGE");
    lv_obj_set_style_text_font(message_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(message_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(message_header, 0, 235);

    lbl_message = lv_label_create(tab);
    lv_label_set_text(lbl_message, "No messages.");
    lv_obj_set_size(lbl_message, 600, 80);  // Height to fit 10px margin at bottom
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_bg_color(lbl_message, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_bg_opa(lbl_message, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lbl_message, 1, 0);
    lv_obj_set_style_border_color(lbl_message, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_radius(lbl_message, 5, 0);
    lv_obj_set_style_pad_all(lbl_message, 5, 0);
    lv_obj_set_pos(lbl_message, 0, 260);
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
    } else if (strcmp(state, "ALARM") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    } else {
        lv_obj_set_style_text_color(lbl_state, UITheme::UI_WARNING, 0);
    }
}

void UITabStatus::updateWorkPosition(float x, float y, float z) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_wpos_x && y == last_wpos_y && z == last_wpos_z) {
        return;
    }
    
    char buf[20];
    
    if (lbl_wpos_x && x != last_wpos_x) {
        snprintf(buf, sizeof(buf), "X  %04.3f", x);
        lv_label_set_text(lbl_wpos_x, buf);
        last_wpos_x = x;
    }
    
    if (lbl_wpos_y && y != last_wpos_y) {
        snprintf(buf, sizeof(buf), "Y  %04.3f", y);
        lv_label_set_text(lbl_wpos_y, buf);
        last_wpos_y = y;
    }
    
    if (lbl_wpos_z && z != last_wpos_z) {
        snprintf(buf, sizeof(buf), "Z  %04.3f", z);
        lv_label_set_text(lbl_wpos_z, buf);
        last_wpos_z = z;
    }
}

void UITabStatus::updateMachinePosition(float x, float y, float z) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_mpos_x && y == last_mpos_y && z == last_mpos_z) {
        return;
    }
    
    char buf[20];
    
    if (lbl_mpos_x && x != last_mpos_x) {
        snprintf(buf, sizeof(buf), "X  %04.3f", x);
        lv_label_set_text(lbl_mpos_x, buf);
        last_mpos_x = x;
    }
    
    if (lbl_mpos_y && y != last_mpos_y) {
        snprintf(buf, sizeof(buf), "Y  %04.3f", y);
        lv_label_set_text(lbl_mpos_y, buf);
        last_mpos_y = y;
    }
    
    if (lbl_mpos_z && z != last_mpos_z) {
        snprintf(buf, sizeof(buf), "Z  %04.3f", z);
        lv_label_set_text(lbl_mpos_z, buf);
        last_mpos_z = z;
    }
}

void UITabStatus::updateFeedRate(float rate, float override_pct) {
    // Only update if values changed
    if (rate == last_feed_rate && override_pct == last_feed_override) {
        return;
    }
    
    char buf[32];
    
    if (lbl_feed_value && rate != last_feed_rate) {
        snprintf(buf, sizeof(buf), "%.0f", rate);  // Just the number, units on separate line
        lv_label_set_text(lbl_feed_value, buf);
        last_feed_rate = rate;
    }
    
    if (lbl_feed_override && override_pct != last_feed_override) {
        snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
        lv_label_set_text(lbl_feed_override, buf);
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
        snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
        lv_label_set_text(lbl_rapid_override, buf);
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
        snprintf(buf, sizeof(buf), "%.0f", speed);  // Just the number, units on separate line
        lv_label_set_text(lbl_spindle_value, buf);
        last_spindle_speed = speed;
    }
    
    if (lbl_spindle_override && override_pct != last_spindle_override) {
        snprintf(buf, sizeof(buf), "%.0f%%", override_pct);
        lv_label_set_text(lbl_spindle_override, buf);
        last_spindle_override = override_pct;
    }
}

void UITabStatus::updateModalStates(const char *wcs, const char *plane, const char *dist, 
                                    const char *units, const char *motion, const char *feedrate,
                                    const char *spindle, const char *coolant, const char *tool) {
    if (lbl_modal_wcs && strcmp(wcs, last_modal_wcs) != 0) {
        lv_label_set_text(lbl_modal_wcs, wcs);
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

void UITabStatus::updateFileProgress(bool is_printing, float percent, const char *filename, 
                                     uint32_t elapsed_ms) {
    if (!lbl_file_progress_container) return;
    
    if (is_printing && percent > 0) {
        // Show the file progress container
        lv_obj_clear_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);
        
        // Update filename
        if (lbl_filename && filename) {
            lv_label_set_text(lbl_filename, filename);
        }
        
        // Update progress bar
        if (bar_progress) {
            lv_bar_set_value(bar_progress, (int)percent, LV_ANIM_OFF);
        }
        
        // Update percentage label (1 decimal place)
        if (lbl_percent) {
            char percent_buf[8];
            snprintf(percent_buf, sizeof(percent_buf), "%.1f%%", percent);
            lv_label_set_text(lbl_percent, percent_buf);
        }
        
        // Calculate and display elapsed time (just the time value, no label)
        if (lbl_elapsed_time) {
            uint32_t seconds = elapsed_ms / 1000;
            uint32_t minutes = seconds / 60;
            uint32_t hours = minutes / 60;
            
            char time_buf[16];
            if (hours > 0) {
                snprintf(time_buf, sizeof(time_buf), "%02lu:%02lu", hours, minutes % 60);
                if (lbl_elapsed_unit) {
                    lv_label_set_text(lbl_elapsed_unit, "hr:min");
                }
            } else {
                snprintf(time_buf, sizeof(time_buf), "%02lu:%02lu", minutes, seconds % 60);
                if (lbl_elapsed_unit) {
                    lv_label_set_text(lbl_elapsed_unit, "min:sec");
                }
            }
            lv_label_set_text(lbl_elapsed_time, time_buf);
        }
        
        // Calculate and display estimated completion time - throttled to update every 5 seconds
        if (lbl_estimated_time && percent > 0.1f) {
            static uint32_t last_estimate_update_ms = 0;
            uint32_t current_ms = millis();
            
            if (current_ms - last_estimate_update_ms >= 5000) {
                last_estimate_update_ms = current_ms;
                
                // Calculate total estimated time based on elapsed time and percentage
                uint32_t total_estimated_ms = (uint32_t)((elapsed_ms * 100.0f) / percent);
                uint32_t remaining_ms = total_estimated_ms - elapsed_ms;
                uint32_t remaining_seconds = remaining_ms / 1000;
                uint32_t remaining_minutes = remaining_seconds / 60;
                uint32_t remaining_hours = remaining_minutes / 60;
                
                char est_buf[16];
                if (remaining_hours > 0) {
                    snprintf(est_buf, sizeof(est_buf), "%02lu:%02lu", 
                            remaining_hours, remaining_minutes % 60);
                    if (lbl_estimated_unit) {
                        lv_label_set_text(lbl_estimated_unit, "hr:min");
                    }
                } else {
                    snprintf(est_buf, sizeof(est_buf), "%02lu:%02lu", 
                            remaining_minutes, remaining_seconds % 60);
                    if (lbl_estimated_unit) {
                        lv_label_set_text(lbl_estimated_unit, "min:sec");
                    }
                }
                lv_label_set_text(lbl_estimated_time, est_buf);
            }
        }
    } else {
        // Hide the file progress container when not printing
        lv_obj_add_flag(lbl_file_progress_container, LV_OBJ_FLAG_HIDDEN);
    }
}
