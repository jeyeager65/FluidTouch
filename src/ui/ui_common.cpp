#include "ui/ui_common.h"
#include "ui/ui_tabs.h"

// Static member initialization
lv_display_t *UICommon::display = nullptr;
lv_obj_t *UICommon::status_bar = nullptr;
lv_obj_t *UICommon::lbl_modal_states = nullptr;
lv_obj_t *UICommon::lbl_mpos = nullptr;
lv_obj_t *UICommon::lbl_status = nullptr;
lv_obj_t *UICommon::lbl_wpos = nullptr;

// Event handler for status bar click
static void status_bar_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Switch to Status tab (index 0)
        lv_obj_t *tabview = UITabs::getTabview();
        if (tabview) {
            lv_tabview_set_active(tabview, 0, LV_ANIM_ON);
        }
    }
}

void UICommon::init(lv_display_t *disp) {
    display = disp;
}

void UICommon::createStatusBar() {
    // Create status bar at bottom (always visible) - 2 lines with CNC info
    status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, SCREEN_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
    lv_obj_set_style_border_color(status_bar, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 0, LV_PART_MAIN); // No rounded corners
    lv_obj_set_style_pad_all(status_bar, 5, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Make status bar clickable to switch to Status tab
    lv_obj_add_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar, status_bar_click_handler, LV_EVENT_CLICKED, NULL);

    // Line 1: Modal states and Machine coordinates
    lbl_modal_states = lv_label_create(status_bar);
    lv_label_set_text(lbl_modal_states, "G54 G0 G17 G90 G94 G21 M5 M9");
    lv_obj_set_style_text_font(lbl_modal_states, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_modal_states, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_pos(lbl_modal_states, 5, 5);

    lbl_mpos = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos, "MPos: X:0000.000 Y:0000.000 Z:0000.000");
    lv_obj_set_style_text_font(lbl_mpos, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_mpos, lv_color_hex(0x00BFFF), 0);
    lv_obj_align(lbl_mpos, LV_ALIGN_TOP_RIGHT, -5, 5);

    // Line 2: Status and Work coordinates
    lbl_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_status, "Status: Idle");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00FF00), 0);
    lv_obj_set_pos(lbl_status, 5, 32);

    lbl_wpos = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos, "WPos: X:0000.000 Y:0000.000 Z:0000.000");
    lv_obj_set_style_text_font(lbl_wpos, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_wpos, lv_color_hex(0xFF8C00), 0);
    lv_obj_align(lbl_wpos, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

void UICommon::updateModalStates(const char *text) {
    if (lbl_modal_states) {
        lv_label_set_text(lbl_modal_states, text);
    }
}

void UICommon::updateMachinePosition(float x, float y, float z) {
    if (lbl_mpos) {
        lv_label_set_text_fmt(lbl_mpos, "MPos: X:%08.3f Y:%08.3f Z:%08.3f", x, y, z);
    }
}

void UICommon::updateWorkPosition(float x, float y, float z) {
    if (lbl_wpos) {
        lv_label_set_text_fmt(lbl_wpos, "WPos: X:%08.3f Y:%08.3f Z:%08.3f", x, y, z);
    }
}

void UICommon::updateMachineState(const char *state) {
    if (lbl_status) {
        lv_label_set_text_fmt(lbl_status, "Status: %s", state);
        
        // Color code the status
        if (strcmp(state, "Idle") == 0) {
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00FF00), 0); // Green
        } else if (strcmp(state, "Run") == 0 || strcmp(state, "Jog") == 0) {
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x00BFFF), 0); // Cyan
        } else if (strcmp(state, "Alarm") == 0 || strcmp(state, "Error") == 0) {
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFF0000), 0); // Red
        } else if (strcmp(state, "Hold") == 0) {
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xFFFF00), 0); // Yellow
        } else {
            lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xCCCCCC), 0); // Gray
        }
    }
}
