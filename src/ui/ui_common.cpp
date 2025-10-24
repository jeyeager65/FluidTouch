#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

// Static member initialization
lv_display_t *UICommon::display = nullptr;
lv_obj_t *UICommon::status_bar = nullptr;
lv_obj_t *UICommon::lbl_modal_states = nullptr;
lv_obj_t *UICommon::lbl_status = nullptr;

// Work Position labels
lv_obj_t *UICommon::lbl_wpos_label = nullptr;
lv_obj_t *UICommon::lbl_wpos_x = nullptr;
lv_obj_t *UICommon::lbl_wpos_y = nullptr;
lv_obj_t *UICommon::lbl_wpos_z = nullptr;

// Machine Position labels
lv_obj_t *UICommon::lbl_mpos_label = nullptr;
lv_obj_t *UICommon::lbl_mpos_x = nullptr;
lv_obj_t *UICommon::lbl_mpos_y = nullptr;
lv_obj_t *UICommon::lbl_mpos_z = nullptr;

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
    lv_obj_set_style_bg_color(status_bar, UITheme::BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_bar, UITheme::BORDER_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 0, LV_PART_MAIN); // No rounded corners
    lv_obj_set_style_pad_all(status_bar, 5, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Make status bar clickable to switch to Status tab
    lv_obj_add_flag(status_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar, status_bar_click_handler, LV_EVENT_CLICKED, NULL);

    // Left side: Large Status (centered vertically)
    lbl_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0);
    lv_obj_align(lbl_status, LV_ALIGN_LEFT_MID, 5, 0);

    // Middle: Work Position (line 1) and Machine Position (line 2)
    // Work Position - Line 1
    lbl_wpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_label, "WPos:");
    lv_obj_set_style_text_font(lbl_wpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_label, UITheme::POS_WORK, 0);  // Orange - primary data
    lv_obj_align(lbl_wpos_label, LV_ALIGN_TOP_MID, -198, 3);  // -210 + 2 + 10 = -198

    lbl_wpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_x, "X:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_wpos_x, LV_ALIGN_TOP_MID, -110, 3);  // -120 + 10 = -110

    lbl_wpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_y, "Y:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_wpos_y, LV_ALIGN_TOP_MID, 0, 3);  // -10 + 10 = 0

    lbl_wpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_z, "Z:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_wpos_z, LV_ALIGN_TOP_MID, 110, 3);  // 100 + 10 = 110

    // Machine Position - Line 2
    lbl_mpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_label, "MPos:");
    lv_obj_set_style_text_font(lbl_mpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_label, UITheme::POS_MACHINE, 0);  // Cyan - secondary data
    lv_obj_align(lbl_mpos_label, LV_ALIGN_BOTTOM_MID, -198, -3);  // -210 + 2 + 10 = -198

    lbl_mpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_x, "X:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_mpos_x, LV_ALIGN_BOTTOM_MID, -110, -3);  // -120 + 10 = -110

    lbl_mpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_y, "Y:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_mpos_y, LV_ALIGN_BOTTOM_MID, 0, -3);  // -10 + 10 = 0

    lbl_mpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_z, "Z:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_mpos_z, LV_ALIGN_BOTTOM_MID, 110, -3);  // 100 + 10 = 110

    // Right side Line 1: Machine name with symbol
    // Get selected machine from preferences
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String machine_name = prefs.getString("machine", "No Machine");
    prefs.end();
    
    // Determine connection symbol based on machine name
    String machine_display;
    if (machine_name == "Test Wired Machine") {
        machine_display = LV_SYMBOL_USB " " + machine_name;
    } else if (machine_name != "No Machine") {
        machine_display = LV_SYMBOL_WIFI " " + machine_name;
    } else {
        machine_display = machine_name;
    }
    
    lbl_modal_states = lv_label_create(status_bar);
    lv_label_set_text(lbl_modal_states, machine_display.c_str());
    lv_obj_set_style_text_font(lbl_modal_states, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_modal_states, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_align(lbl_modal_states, LV_ALIGN_TOP_RIGHT, -5, 3);

    // Right side Line 2: WiFi network
    // Get WiFi network name
    String wifi_ssid = WiFi.isConnected() ? WiFi.SSID() : "Not Connected";
    String wifi_display = LV_SYMBOL_WIFI " " + wifi_ssid;
    
    lv_obj_t *lbl_wifi = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi, wifi_display.c_str());
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wifi, UITheme::UI_INFO, 0);
    lv_obj_align(lbl_wifi, LV_ALIGN_BOTTOM_RIGHT, -5, -3);
}

void UICommon::updateModalStates(const char *text) {
    if (lbl_modal_states) {
        lv_label_set_text(lbl_modal_states, text);
    }
}

void UICommon::updateMachinePosition(float x, float y, float z) {
    if (lbl_mpos_x) {
        lv_label_set_text_fmt(lbl_mpos_x, "X:%08.3f", x);
    }
    if (lbl_mpos_y) {
        lv_label_set_text_fmt(lbl_mpos_y, "Y:%08.3f", y);
    }
    if (lbl_mpos_z) {
        lv_label_set_text_fmt(lbl_mpos_z, "Z:%08.3f", z);
    }
}

void UICommon::updateWorkPosition(float x, float y, float z) {
    if (lbl_wpos_x) {
        lv_label_set_text_fmt(lbl_wpos_x, "X:%08.3f", x);
    }
    if (lbl_wpos_y) {
        lv_label_set_text_fmt(lbl_wpos_y, "Y:%08.3f", y);
    }
    if (lbl_wpos_z) {
        lv_label_set_text_fmt(lbl_wpos_z, "Z:%08.3f", z);
    }
}

void UICommon::updateMachineState(const char *state) {
    if (lbl_status) {
        // Convert to uppercase
        String state_upper = String(state);
        state_upper.toUpperCase();
        lv_label_set_text(lbl_status, state_upper.c_str());
        
        // Color code the status
        if (strcmp(state, "Idle") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0); // Green
        } else if (strcmp(state, "Run") == 0 || strcmp(state, "Jog") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_RUN, 0); // Cyan
        } else if (strcmp(state, "Alarm") == 0 || strcmp(state, "Error") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_ALARM, 0); // Red
        } else if (strcmp(state, "Hold") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_HOLD, 0); // Yellow
        } else {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_UNKNOWN, 0); // Gray
        }
    }
}
